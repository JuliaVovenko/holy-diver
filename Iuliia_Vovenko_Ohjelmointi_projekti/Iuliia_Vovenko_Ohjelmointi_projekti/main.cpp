#include <iostream>
#include <string>
#include <cstdlib>   
#include <cstdio>    
using namespace std;


// global variables
char** game_map = nullptr; // array of map strings
int map_height = 0;        // how many lines are there in the map

// function to load a map from a file
bool load_map(const string& path)
{
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, path.c_str(), "r");
    if (err != 0 || file == nullptr) {
        cout << "Failed to open map file!\n";
        return false;
    }

    if (!file) {
        cout << "Failed to open map file!\n";
        return false;
    }

    const int CHUNK = 8;   
    int capacity = 0;      
    char buffer[1024];     

    while (fgets(buffer, sizeof(buffer), file)) {
        
        int len = 0;
        while (buffer[len] != '\0' && buffer[len] != '\n') len++;

       
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

        map_height++;
    }

    fclose(file);
    return true;
}

// map printing function
void print_map()
{
    for (int i = 0; i < map_height; i++) {
        cout << game_map[i] << "\n";
    }
}

// memory release function
void free_map()
{
    for (int i = 0; i < map_height; i++) {
        free(game_map[i]);     
    }
    free(game_map);            
    game_map = nullptr;
    map_height = 0;
}

int main()
{
    string path;
    cout << "Enter map file path: ";
    getline(cin, path);

    if (!load_map(path)) {
        cout << "Map loading failed.\n";
        return 1;
    }

    cout << "\n--- MAP ---\n";
    print_map();

    free_map();
    return 0;
}

