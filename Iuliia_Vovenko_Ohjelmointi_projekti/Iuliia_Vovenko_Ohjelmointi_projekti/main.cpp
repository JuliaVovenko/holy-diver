#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>

using namespace std;

/****************************************************/
// declaring functions:
/****************************************************/
void start_splash_screen(void);
void startup_routines(void);
void quit_routines(void);
bool load_level(string);          // load map from file into game_map
void free_level(void);            // free allocated map memory
int read_input(char*);
void update_state(char);          // one command per turn
void render_screen(void);

/****************************************************/
// global variables:
/****************************************************/
char** game_map = nullptr;        // dynamic array of map strings
int map_height = 0;               // map rows
int map_width = 0;               // map columns (assume rectangular-ish)

string g_level_path = "level_0.map"; // auto map path (no user input)

const int MAX_HEALTH = 100;
const int MAX_OXYGEN = 100;
const int INIT_LIVES = 3;

typedef struct Player {
    int health;
    int oxygen;
    int lives;
    int x; // player column
    int y; // player row
} Player;

Player player_data = { MAX_HEALTH, MAX_OXYGEN, INIT_LIVES, -1, -1 };

/****************************************************/
// helpers
/****************************************************/
static int my_strlen_line(const char* s)
{
    int n = 0;
    while (s[n] != '\0' && s[n] != '\n' && s[n] != '\r') n++;
    return n;
}

static bool in_bounds(int x, int y)
{
    return (y >= 0 && y < map_height && x >= 0 && x < map_width);
}

static bool is_walkable(char c)
{
    // 'x' is obstacle, forbidden
    // 'o' is free
    // 'M' is enemy square (allowed, but nothing happens for now)
    // 'P' is player
    return c != 'x' && c != '\0';
}

static bool find_player_on_map()
{
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            if (game_map[y][x] == 'P') {
                player_data.x = x;
                player_data.y = y;
                return true;
            }
        }
    }
    return false;
}

static bool try_move_player(int dx, int dy)
{
    int nx = player_data.x + dx;
    int ny = player_data.y + dy;

    if (!in_bounds(nx, ny)) return false;

    char target = game_map[ny][nx];
    if (!is_walkable(target)) return false;

    // move: old player square becomes free 'o'
    game_map[player_data.y][player_data.x] = 'o';
    // new square becomes 'P'
    game_map[ny][nx] = 'P';

    player_data.x = nx;
    player_data.y = ny;

    // oxygen consumption: 2 per move
    player_data.oxygen -= 2;
    if (player_data.oxygen < 0) player_data.oxygen = 0;

    return true;
}

static void reset_player_stats()
{
    player_data.health = MAX_HEALTH;
    player_data.oxygen = MAX_OXYGEN;
    player_data.lives = INIT_LIVES;
    player_data.x = -1;
    player_data.y = -1;
}

static void reload_level()
{
    free_level();
    reset_player_stats();

    if (!load_level(g_level_path)) {
        cout << "Reload failed.\n";
        return;
    }
    cout << "Reloaded!\n";
}

/****************************************************************
 *
 * MAIN
 *
 ****************************************************/
int main(void)
{
    start_splash_screen();
    startup_routines();

    char input;

    while (true)
    {
        input = '\0';
        if (0 > read_input(&input)) break; // quit if 'q'
        update_state(input);
        render_screen();
    }

    quit_routines();
    return 0;
}

/****************************************************************
 *
 * FUNCTION load_level
 *
 **************************************************************/
bool load_level(string path)
{
    // reset map vars (in case called after free)
    map_height = 0;
    map_width = 0;
    game_map = nullptr;

    FILE* file = nullptr;
    errno_t err = fopen_s(&file, path.c_str(), "r");
    if (err != 0 || file == nullptr) {
        cout << "Failed to open map file: " << path << "\n";
        return false;
    }

    const int CHUNK = 8;
    int capacity = 0;
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), file)) {

        int len = my_strlen_line(buffer);
        if (len <= 0) continue;

        if (map_height == capacity) {
            capacity += CHUNK;
            char** temp = (char**)realloc(game_map, capacity * sizeof(char*));
            if (!temp) {
                fclose(file);
                cout << "realloc failed!\n";
                return false;
            }
            game_map = temp;
        }

        game_map[map_height] = (char*)malloc((len + 1) * sizeof(char));
        if (!game_map[map_height]) {
            fclose(file);
            cout << "malloc failed!\n";
            return false;
        }

        for (int i = 0; i < len; i++)
            game_map[map_height][i] = buffer[i];

        game_map[map_height][len] = '\0';

        if (map_height == 0) map_width = len; // width from first row
        map_height++;
    }

    fclose(file);

    if (map_height == 0 || map_width == 0) {
        cout << "Map file is empty or invalid.\n";
        return false;
    }

    // find player position from 'P'
    if (!find_player_on_map()) {
        cout << "No 'P' found on the map!\n";
        return false;
    }

    return true;
}

/****************************************************************
 *
 * FUNCTION free_level
 *
 **************************************************************/
void free_level(void)
{
    if (!game_map) return;

    for (int i = 0; i < map_height; i++) {
        free(game_map[i]);
    }
    free(game_map);

    game_map = nullptr;
    map_height = 0;
    map_width = 0;
}

/****************************************************************
 *
 * FUNCTION read_input
 *
 **************************************************************/
int read_input(char* input)
{
    cout << ">>>";  // prompt
    try {
        cin >> *input;
    }
    catch (...) {
        return -1;
    }
    cout << endl;
    cin.ignore(10000, '\n');

    if (*input == 'q') return -2; // quit
    return 0;
}

/****************************************************************
 *
 * FUNCTION update_state
 *
 **************************************************************/
void update_state(char input)
{
    if (!game_map) return;

    // reload
    if (input == 'r') {
        reload_level();
        return;
    }

    // stop movement if oxygen ended (optional, but logical)
    if (player_data.oxygen <= 0) {
        cout << "Out of oxygen. Press 'r' to reload or 'q' to quit.\n";
        return;
    }

    bool moved = false;

    switch (input) {
    case 'w': moved = try_move_player(0, -1); break;
    case 's': moved = try_move_player(0, 1); break;
    case 'a': moved = try_move_player(-1, 0); break;
    case 'd': moved = try_move_player(1, 0); break;
    default:
        cout << "Commands: w/a/s/d move, r reload, q quit\n";
        return;
    }

    if (!moved) {
        cout << "Can't move there!\n";
    }
}

/****************************************************************
 *
 * FUNCTION render_screen
 *
 **************************************************************/
void render_screen(void)
{
    if (!game_map) return;

    cout << "\n--- MAP ---\n";
    for (int i = 0; i < map_height; i++) {
        cout << game_map[i] << "\n";
    }
    cout << "Oxygen: " << player_data.oxygen << "%\n";
}

/****************************************************************
 *
 * FUNCTION start_splash_screen
 *
 **************************************************************/
void start_splash_screen(void)
{
    cout << endl << "WELCOME to epic Holy Diver v0.01" << endl;
    cout << "Enter commands and enjoy! (press q to quit at all times)" << endl;
    cout << "Commands: w/a/s/d move, r reload, q quit" << endl;
    cout << "Map loads automatically: " << g_level_path << endl << endl;
}

/****************************************************************
 *
 * FUNCTION startup_routines
 *
 **************************************************************/
void startup_routines(void)
{
    reset_player_stats();

    if (!load_level(g_level_path)) {
        cout << "Map loading failed.\n";
        cout << "Press Enter to exit...";
        cin.get();
        exit(1);
    }

    render_screen();
}

/****************************************************************
 *
 * FUNCTION quit_routines
 *
 **************************************************************/
void quit_routines(void)
{
    free_level();
    cout << endl << "BYE! Welcome back soon." << endl;
    cout << "Press Enter to exit...";
    cin.get();
}
