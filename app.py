import os
import subprocess
import threading
import queue
import time
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

# --- CONFIGURATION ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MUSIC_FOLDER = os.path.join(BASE_DIR, 'static', 'music')
SONG_DB_FILE = os.path.join(BASE_DIR, 'songs.txt')

# Detect OS to pick correct executable name
if os.name == 'nt': # Windows
    C_EXECUTABLE = 'music_player.exe'
else: # Linux/Mac
    C_EXECUTABLE = './music_player'

# --- 1. ROBUST SETUP (Fixes Folders) ---
try:
    os.makedirs(MUSIC_FOLDER, exist_ok=True)
except Exception as e:
    print(f"Warning: Folder creation issue: {e}")

# Ensure songs.txt exists
if not os.path.exists(SONG_DB_FILE):
    with open(SONG_DB_FILE, 'w') as f:
        f.write("")

# --- GLOBAL STATE ---
output_queue = queue.Queue()
current_song_file = ""
c_process = None

def run_c_process():
    """Starts the C executable and keeps reading its output."""
    global c_process
    
    # Verify Executable exists
    exe_full_path = os.path.join(BASE_DIR, C_EXECUTABLE)
    
    # Windows fallback check (sometimes users type gcc -o music_player without .exe)
    if not os.path.exists(exe_full_path) and os.name == 'nt' and os.path.exists(exe_full_path + ".exe"):
         exe_full_path += ".exe"

    if not os.path.exists(exe_full_path):
        print(f"CRITICAL ERROR: {C_EXECUTABLE} not found.")
        print("Please compile your C code first: gcc music_player_backend.c -o music_player")
        return

    try:
        # Start C program as a subprocess
        c_process = subprocess.Popen(
            [exe_full_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1 # Line buffered to get output immediately
        )

        # Thread to read stdout from C
        def read_output():
            global current_song_file
            while True:
                # Check if process is still alive
                if c_process.poll() is not None: 
                    break
                
                line = c_process.stdout.readline()
                if not line: 
                    break
                
                line = line.strip()
                if line:
                    print(f"[C_BACKEND]: {line}") # Log to console
                    output_queue.put(line)        # Send to frontend
                    
                    # Logic Hook: Capture current song filename
                    if line.startswith("NOW_PLAYING"):
                        parts = line.split("|")
                        if len(parts) >= 3:
                            current_song_file = parts[2]

        threading.Thread(target=read_output, daemon=True).start()

    except Exception as e:
        print(f"Failed to start C process: {e}")

# Start C backend on launch
run_c_process()

# --- ROUTES ---

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/send', methods=['POST'])
def send_command():
    """Receives command from UI, sends to C process via stdin."""
    data = request.json
    command = str(data.get('command')) + "\n"
    
    if c_process and c_process.stdin:
        try:
            c_process.stdin.write(command)
            c_process.stdin.flush()
            return jsonify({"status": "sent", "cmd": command.strip()})
        except Exception:
            return jsonify({"status": "error", "msg": "Backend died"})
    return jsonify({"status": "error", "msg": "Backend not running"})

@app.route('/output')
def get_output():
    """Frontend polls this to get logs and current song."""
    lines = []
    while not output_queue.empty():
        lines.append(output_queue.get())
    return jsonify({"lines": lines, "current_song": current_song_file})

@app.route('/upload', methods=['POST'])
def upload_file():
    """Handles file upload and adds to songs.txt properly."""
    if 'file' not in request.files:
        return jsonify({"error": "No file part"})
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({"error": "No selected file"})

    if file:
        filename = file.filename
        # Save to static/music
        save_path = os.path.join(MUSIC_FOLDER, filename)
        file.save(save_path)
        
        # --- CRITICAL FIX: Handle Newlines in songs.txt ---
        prefix = ""
        try:
            with open(SONG_DB_FILE, "r") as f:
                content = f.read()
                # If file has content and doesn't end with a newline, add one
                if content and not content.endswith("\n"):
                    prefix = "\n"
        except FileNotFoundError:
            pass

        with open(SONG_DB_FILE, "a") as f:
            f.write(prefix + filename + "\n")
            
        # Tell C to reload DB (Command 8)
        if c_process and c_process.stdin:
            try:
                c_process.stdin.write("8\n")
                c_process.stdin.flush()
            except:
                pass

        return jsonify({"success": True, "filename": filename})

@app.route('/delete', methods=['POST'])
def delete_song():
    data = request.json
    filename = data.get('filename')
    song_id = data.get('id')

    # 1. Remove from File System
    try:
        file_path = os.path.join(MUSIC_FOLDER, filename)
        if os.path.exists(file_path):
            os.remove(file_path)
    except Exception as e:
        print(f"Delete Error: {e}")

    # 2. Remove from songs.txt
    try:
        if os.path.exists(SONG_DB_FILE):
            with open(SONG_DB_FILE, "r") as f:
                lines = f.readlines()
            
            with open(SONG_DB_FILE, "w") as f:
                for line in lines:
                    if line.strip() != filename:
                        f.write(line)
    except Exception as e:
        print(f"Error updating DB: {e}")

    # 3. Tell C to delete from memory
    if c_process and c_process.stdin:
        try:
            cmd = f"12 {song_id}\n"
            c_process.stdin.write(cmd)
            c_process.stdin.flush()
        except:
            pass

    return jsonify({"success": True})

if __name__ == '__main__':
    app.run(debug=True, port=5000)
