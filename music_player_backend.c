/*
 * AURA MUSIC PLAYER - C BACKEND (V4.0 - DELETE SUPPORT)
 * -----------------------------------------------------
 * UPGRADES:
 * 1. Delete Song: Remove song from array and shift elements.
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
int repeat_mode = 0;

// --- PROTOTYPES ---
void init_playlist();
void resize_playlist();
void load_songs_from_file(const char *filename);
int song_exists(const char *filename);
void print_current_state();
void list_library();
void play_by_id(int id);
void delete_song(int id); // NEW
void next_song();
void prev_song();
void search_song(char *query);
void free_memory();
void trim_string(char *str);

// --- MAIN FUNCTION ---
int main() {
    setvbuf(stdout, NULL, _IONBF, 0); 
    srand(time(0));

    printf("SYSTEM: C Backend V4.0 (Delete Support) Initializing...\n");
    
    init_playlist();
    load_songs_from_file("songs.txt");
    
    printf("SYSTEM: Ready. Loaded %d unique songs.\n", myPlaylist.count);
    print_current_state();

    int command;
    char inputBuffer[256];

    while (1) {
        if (fgets(inputBuffer, sizeof(inputBuffer), stdin) == NULL) break;
        trim_string(inputBuffer);

        int arg = -1;
        // Check for "11 <ID>" (Play) or "12 <ID>" (Delete)
        if (sscanf(inputBuffer, "11 %d", &arg) == 1) {
            command = 11;
        } else if (sscanf(inputBuffer, "12 %d", &arg) == 1) {
            command = 12;
        } else if (sscanf(inputBuffer, "%d", &command) != 1) {
            if (strlen(inputBuffer) > 0) search_song(inputBuffer);
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
                list_library(); 
                break;
            case 10: // LIST LIBRARY
                list_library();
                break;
            case 11: // PLAY ID
                play_by_id(arg);
                break;
            case 12: // DELETE ID (NEW)
                delete_song(arg);
                break;
            case 9: // EXIT
                printf("SYSTEM: Exiting...\n");
                free_memory();
                return 0;
            default:
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

int song_exists(const char *filename) {
    for (int i = 0; i < myPlaylist.count; i++) {
        if (strcmp(myPlaylist.songs[i].filename, filename) == 0) return 1;
    }
    return 0;
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
        if (song_exists(line)) continue;

        if (myPlaylist.count >= myPlaylist.capacity) resize_playlist();

        Song newSong;
        newSong.id = myPlaylist.count + 1;
        strcpy(newSong.filename, line);
        strcpy(newSong.title, line); 
        
        myPlaylist.songs[myPlaylist.count] = newSong;
        myPlaylist.count++;
    }
    fclose(file);
}

void list_library() {
    printf("LIBRARY_START\n");
    for(int i = 0; i < myPlaylist.count; i++) {
        printf("LIB|%d|%s\n", myPlaylist.songs[i].id, myPlaylist.songs[i].title);
    }
    printf("LIBRARY_END\n");
}

void play_by_id(int id) {
    for(int i = 0; i < myPlaylist.count; i++) {
        if (myPlaylist.songs[i].id == id) {
            current_song_index = i;
            is_playing = 1;
            printf("ACTION: Playing Selected Track\n");
            print_current_state();
            return;
        }
    }
    printf("ERROR: Song ID %d not found.\n", id);
}

// NEW: Delete Function
void delete_song(int id) {
    int foundIndex = -1;
    for(int i = 0; i < myPlaylist.count; i++) {
        if (myPlaylist.songs[i].id == id) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex != -1) {
        printf("ACTION: Deleting ID %d (%s)...\n", id, myPlaylist.songs[foundIndex].title);
        
        // Shift Array Left (Syllabus: Deleting from Array)
        for(int i = foundIndex; i < myPlaylist.count - 1; i++) {
            myPlaylist.songs[i] = myPlaylist.songs[i+1];
        }
        myPlaylist.count--;
        
        // Adjust current index if needed
        if (current_song_index >= myPlaylist.count) current_song_index = 0;
        
        list_library(); // Auto-refresh UI
    } else {
        printf("ERROR: Cannot delete. ID %d not found.\n", id);
    }
}

void next_song() {
    if (myPlaylist.count == 0) return;
    if (shuffle_mode) current_song_index = rand() % myPlaylist.count;
    else {
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
        printf("STATUS: Playlist Empty.\n");
    }
}

void free_memory() {
    if (myPlaylist.songs) free(myPlaylist.songs);
}
