/*
 * AURA MUSIC PLAYER - C BACKEND (V3.0 - ROBUST & LIBRARY AWARE)
 * -------------------------------------------------------------
 * UPGRADES:
 * 1. Duplicate Prevention: Checks if song exists before adding.
 * 2. Library Export: Can print all songs for UI rendering.
 * 3. Direct Play: Can play specific ID (for clicking cards).
 * 4. Improved Robustness: Better string handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// --- MACROS ---
#define MAX_TITLE 100
#define MAX_SONG_PATH 200
#define INITIAL_CAPACITY 20

// --- DATA STRUCTURES ---
typedef struct {
    int id;
    char title[MAX_TITLE];
    char filename[MAX_SONG_PATH];
} Song;

typedef struct {
    Song *songs;
    int count;
    int capacity;
} Playlist;

// --- GLOBAL VARIABLES ---
Playlist myPlaylist;
int current_song_index = 0;
int is_playing = 0;
int shuffle_mode = 0;
int repeat_mode = 0; // 0=None, 1=One, 2=All

// --- PROTOTYPES ---
void init_playlist();
void resize_playlist();
void load_songs_from_file(const char *filename);
int song_exists(const char *filename); // NEW: Check duplicate
void print_current_state();
void list_library(); // NEW: Export library to UI
void play_by_id(int id); // NEW: Direct play
void next_song();
void prev_song();
void search_song(char *query);
void free_memory();
void trim_string(char *str);

// --- MAIN FUNCTION ---
int main() {
    setvbuf(stdout, NULL, _IONBF, 0); 
    srand(time(0));

    printf("SYSTEM: C Backend V3.0 Initializing...\n");
    
    init_playlist();
    load_songs_from_file("songs.txt");
    
    printf("SYSTEM: Ready. Loaded %d unique songs.\n", myPlaylist.count);
    print_current_state();

    int command;
    char inputBuffer[256]; // Increased buffer size

    while (1) {
        if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) break;
        trim_string(inputBuffer);

        // Parse command
        int id_arg = -1;
        // Check for "11 <ID>" pattern for direct play
        if (sscanf(inputBuffer, "11 %d", &id_arg) == 1) {
            command = 11;
        } else if (sscanf(inputBuffer, "%d", &command) != 1) {
            // If not a number, treat as search
            if (strlen(inputBuffer) > 0) {
                search_song(inputBuffer);
            }
            continue;
        }

        switch (command) {
            case 1: // PLAY
                is_playing = 1;
                printf("ACTION: Playing\n");
                print_current_state();
                break;
            case 2: // PAUSE
                is_playing = 0;
                printf("ACTION: Paused\n");
                break;
            case 3: // NEXT
                next_song();
                break;
            case 4: // PREV
                prev_song();
                break;
            case 5: // SHUFFLE
                shuffle_mode = !shuffle_mode;
                printf("ACTION: Shuffle %s\n", shuffle_mode ? "ON" : "OFF");
                break;
            case 6: // REPEAT
                repeat_mode = (repeat_mode + 1) % 3;
                char *modes[] = {"None", "One", "All"};
                printf("ACTION: Repeat %s\n", modes[repeat_mode]);
                break;
            case 8: // RELOAD DB
                free(myPlaylist.songs);
                init_playlist();
                load_songs_from_file("songs.txt");
                printf("ACTION: Database Reloaded. Count: %d\n", myPlaylist.count);
                current_song_index = 0;
                print_current_state();
                // Auto-push library update
                list_library(); 
                break;
            case 10: // LIST LIBRARY (NEW)
                list_library();
                break;
            case 11: // PLAY SPECIFIC ID (NEW)
                play_by_id(id_arg);
                break;
            case 9: // EXIT
                printf("SYSTEM: Exiting...\n");
                free_memory();
                return 0;
            default:
                // Ignored if handled above, else error
                break;
        }
    }

    free_memory();
    return 0;
}

// --- LOGIC ---

void init_playlist() {
    myPlaylist.count = 0;
    myPlaylist.capacity = INITIAL_CAPACITY;
    myPlaylist.songs = (Song *)malloc(sizeof(Song) * myPlaylist.capacity);
    if (!myPlaylist.songs) exit(1);
}

void resize_playlist() {
    int new_capacity = myPlaylist.capacity * 2;
    Song *temp = (Song *)realloc(myPlaylist.songs, sizeof(Song) * new_capacity);
    if (temp) {
        myPlaylist.songs = temp;
        myPlaylist.capacity = new_capacity;
    }
}

void trim_string(char *str) {
    char *p = str;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(str, p, l + 1);
}

// NEW: Helper to check if filename already exists in playlist
int song_exists(const char *filename) {
    for (int i = 0; i < myPlaylist.count; i++) {
        if (strcmp(myPlaylist.songs[i].filename, filename) == 0) {
            return 1; // Exists
        }
    }
    return 0; // Unique
}

void load_songs_from_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        file = fopen(filename, "w");
        if(file) fclose(file);
        return;
    }

    char line[MAX_SONG_PATH];
    while (fgets(line, sizeof(line), file)) {
        trim_string(line);
        if (strlen(line) < 1) continue;

        // CHECK DUPLICATE BEFORE ADDING
        if (song_exists(line)) {
            // Skip duplicate
            continue;
        }

        if (myPlaylist.count >= myPlaylist.capacity) {
            resize_playlist();
        }

        Song newSong;
        newSong.id = myPlaylist.count + 1; // ID starts at 1
        strcpy(newSong.filename, line);
        strcpy(newSong.title, line); // Could handle cleaner titles here
        
        myPlaylist.songs[myPlaylist.count] = newSong;
        myPlaylist.count++;
    }
    fclose(file);
}

void list_library() {
    // Outputs special format for UI to parse:
    // LIBRARY_DATA|ID|TITLE
    printf("LIBRARY_START\n");
    for(int i = 0; i < myPlaylist.count; i++) {
        printf("LIB|%d|%s\n", myPlaylist.songs[i].id, myPlaylist.songs[i].title);
    }
    printf("LIBRARY_END\n");
}

void play_by_id(int id) {
    // Find index by ID
    int foundIndex = -1;
    for(int i = 0; i < myPlaylist.count; i++) {
        if (myPlaylist.songs[i].id == id) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex != -1) {
        current_song_index = foundIndex;
        is_playing = 1;
        printf("ACTION: Playing Selected Track\n");
        print_current_state();
    } else {
        printf("ERROR: Song ID %d not found.\n", id);
    }
}

void next_song() {
    if (myPlaylist.count == 0) return;

    if (shuffle_mode) {
        current_song_index = rand() % myPlaylist.count;
    } else {
        current_song_index++;
        if (current_song_index >= myPlaylist.count) current_song_index = 0; 
    }
    is_playing = 1;
    printf("ACTION: Skipped Next\n");
    print_current_state();
}

void prev_song() {
    if (myPlaylist.count == 0) return;

    current_song_index--;
    if (current_song_index < 0) current_song_index = myPlaylist.count - 1; 
    is_playing = 1;
    printf("ACTION: Skipped Previous\n");
    print_current_state();
}

void search_song(char *query) {
    printf("ACTION: Searching for '%s'...\n", query);
    int found = 0;
    printf("SEARCH_RESULTS_START\n");
    for (int i = 0; i < myPlaylist.count; i++) {
        if (strstr(myPlaylist.songs[i].title, query) != NULL) {
            printf("MATCH|%d|%s\n", myPlaylist.songs[i].id, myPlaylist.songs[i].title);
            found = 1;
        }
    }
    printf("SEARCH_RESULTS_END\n");
    if (!found) printf("RESULT: No songs found.\n");
}

void print_current_state() {
    if (myPlaylist.count > 0) {
        printf("NOW_PLAYING|%d|%s|%s\n", 
               current_song_index, 
               myPlaylist.songs[current_song_index].filename, 
               myPlaylist.songs[current_song_index].title);
    } else {
        printf("STATUS: Playlist Empty. Upload songs!\n");
    }
}

void free_memory() {
    if (myPlaylist.songs) free(myPlaylist.songs);
}