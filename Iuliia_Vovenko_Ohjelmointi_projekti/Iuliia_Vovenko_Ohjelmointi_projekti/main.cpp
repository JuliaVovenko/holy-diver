#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// =====================
// Global game state control (required by Exercise 2)
// =====================

// Global flag used to control the main game loop
static bool g_game_running = true;

// Global function that ends the game loop
static void gameover() {
    g_game_running = false;
}

// =====================
// Common structures
// =====================

// Represents a position on the map
struct Position {
    int x = -1;
    int y = -1;
};

// Position comparison operator
static bool operator==(const Position& a, const Position& b) {
    return a.x == b.x && a.y == b.y;
}

// =====================
// Forward declarations
// =====================

class Player;

// =====================
// Item class
// =====================

// Different types of items in the game
enum class ItemType {
    Oxygen
};

class Item {
public:
    // Constructor
    Item(ItemType t, Position p, int v, char sym)
        : type(t), pos(p), value(v), symbol(sym) {
    }

    // Copy constructor (explicit for the exercise)
    // NOTE: Compiler-generated copy constructor would be sufficient here,
    // because Item only contains value types (no raw pointers / manual memory).
    Item(const Item& other)
        : type(other.type), pos(other.pos), value(other.value), symbol(other.symbol) {
    }

    // Destructor
    ~Item() {
        // No manual cleanup needed (no dynamic allocation here)
    }

    // Get item position
    const Position& getPos() const { return pos; }

    // Get item map symbol
    char getSymbol() const { return symbol; }

    // Apply item effect to player
    void apply(Player& player) const;

private:
    ItemType type;
    Position pos;
    int value = 0;
    char symbol = '?';
};

// =====================
// Enemy class
// =====================

class Enemy {
public:
    // Constructor
    Enemy(Position p, int dmg = 10, char sym = 'M')
        : pos(p), damage(dmg), symbol(sym) {
    }

    // Copy constructor (explicit for the exercise)
    // NOTE: Compiler-generated copy constructor would be sufficient here,
    // because Enemy only contains value types (no raw pointers / manual memory).
    Enemy(const Enemy& other)
        : pos(other.pos), damage(other.damage), symbol(other.symbol) {
    }

    // Destructor
    ~Enemy() {
        // No manual cleanup needed (no dynamic allocation here)
    }

    const Position& getPos() const { return pos; }
    char getSymbol() const { return symbol; }
    int getDamage() const { return damage; }

private:
    Position pos;
    int damage = 10;
    char symbol = 'M';
};

// =====================
// World class
// =====================

class World {
public:
    // Constructor
    World() : width(0), height(0) {}

    // Destructor
    ~World() {
        // No manual cleanup needed (vector<string> manages memory automatically)
    }

    // Load map from file
    bool loadFromFile(const string& path) {
        tiles.clear();
        width = height = 0;

        ifstream in(path);
        if (!in) {
            cout << "Failed to open map file: " << path << "\n";
            return false;
        }

        string line;
        while (getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            tiles.push_back(line);
        }

        height = (int)tiles.size();
        if (height == 0) {
            cout << "Map file is empty or invalid.\n";
            return false;
        }

        width = (int)tiles[0].size();
        if (width == 0) {
            cout << "Map file is empty or invalid.\n";
            return false;
        }

        // Ensure rectangular map shape
        for (auto& row : tiles) {
            if ((int)row.size() < width)
                row += string(width - row.size(), 'x');
            if ((int)row.size() > width)
                row.resize(width);
        }

        return true;
    }

    // Render map to console
    void render() const {
        cout << "\n--- MAP ---\n";
        for (const auto& row : tiles)
            cout << row << "\n";
    }

    // Check map boundaries
    bool inBounds(int x, int y) const {
        return y >= 0 && y < height && x >= 0 && x < width;
    }

    // Get tile at position
    char at(int x, int y) const {
        if (!inBounds(x, y)) return '\0';
        return tiles[y][x];
    }

    // Set tile at position
    void set(int x, int y, char c) {
        if (inBounds(x, y)) tiles[y][x] = c;
    }

    // Check if tile is walkable
    bool isWalkable(int x, int y) const {
        // 'x' is blocked, everything else is allowed
        char c = at(x, y);
        return c != 'x' && c != '\0';
    }

    // Find first occurrence of symbol
    Position findFirst(char symbol) const {
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                if (tiles[y][x] == symbol)
                    return { x, y };
        return { -1, -1 };
    }

    // Find all occurrences of symbol
    vector<Position> findAll(char symbol) const {
        vector<Position> found;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                if (tiles[y][x] == symbol)
                    found.push_back({ x, y });
        return found;
    }

private:
    vector<string> tiles;
    int width = 0;
    int height = 0;
};

// =====================
// Player class
// =====================

class Player {
public:
    static constexpr int MAX_HEALTH = 100;
    static constexpr int MAX_OXYGEN = 100;
    static constexpr int INIT_LIVES = 3;

    // Constructor
    Player()
        : health(MAX_HEALTH), oxygen(MAX_OXYGEN), lives(INIT_LIVES), pos({ -1, -1 }) {
    }

    // Destructor (required by Exercise 2)
    ~Player() {
        // Farewell message
        cout << "\n[Player] Goodbye! Player object is being destroyed.\n";

        // Required: from Player destructor, call a global gameover() method
        // that changes the game state to ending.
        gameover();
    }

    // Reset player statistics
    void reset() {
        health = MAX_HEALTH;
        oxygen = MAX_OXYGEN;
        lives = INIT_LIVES;
        pos = { -1, -1 };
    }

    void setPosition(Position p) { pos = p; }
    const Position& getPos() const { return pos; }

    int getHealth() const { return health; }
    int getOxygen() const { return oxygen; }
    int getLives() const { return lives; }

    bool hasOxygen() const { return oxygen > 0; }

    void addOxygen(int amount) {
        oxygen += amount;
        if (oxygen > MAX_OXYGEN)
            oxygen = MAX_OXYGEN;
    }

    void takeDamage(int amount) {
        health -= amount;
        if (health < 0)
            health = 0;
    }

    bool isDead() const { return health <= 0; }

    void loseLife() {
        lives--;
        if (lives < 0)
            lives = 0;
    }

    // Attempt to move player 
    bool tryMove(World& world, int dx, int dy) {
        int nx = pos.x + dx;
        int ny = pos.y + dy;

        if (!world.inBounds(nx, ny)) return false;
        if (!world.isWalkable(nx, ny)) return false;

        // Old player tile becomes 'o'
        world.set(pos.x, pos.y, 'o');
        // New tile becomes 'P'
        world.set(nx, ny, 'P');

        pos = { nx, ny };

        // Oxygen consumption: 2 per move
        oxygen -= 2;
        if (oxygen < 0)
            oxygen = 0;

        return true;
    }

private:
    int health;
    int oxygen;
    int lives;
    Position pos;
};

// Apply item effect
void Item::apply(Player& player) const {
    if (type == ItemType::Oxygen)
        player.addOxygen(value);
}

// =====================
// Game class
// =====================

class Game {
public:
    // Constructor
    explicit Game(string levelPath)
        : levelPath(std::move(levelPath)) {
    }

    // Destructor
    ~Game() {
        // No manual cleanup needed (RAII handles it)
        // TODO: add save system cleanup here if implemented later
    }

    void run() {
        splash();

        if (!loadLevel()) {
            cout << "Map loading failed.\n";
            cout << "Press Enter to exit...";
            cin.get();
            return;
        }

        render();

        // Main loop controlled by the global flag (required by Exercise 2)
        while (g_game_running) {
            char cmd = readInput();

            if (cmd == 'q') {
                // End loop explicitly if user quits
                gameover();
                break;
            }

            update(cmd);
            render();
        }

        quit();
    }

private:
    bool loadLevel() {
        player.reset();
        enemies.clear();
        items.clear();

        if (!world.loadFromFile(levelPath))
            return false;

        Position p = world.findFirst('P');
        if (p.x == -1) {
            cout << "No 'P' found on the map!\n";
            return false;
        }

        player.setPosition(p);

        // Load enemies from 'M'
        for (auto& ep : world.findAll('M'))
            enemies.emplace_back(ep, 10, 'M');

        // Load oxygen items from 'O'
        // TODO: support multiple item types and values loaded from file later
        for (auto& ip : world.findAll('O'))
            items.emplace_back(ItemType::Oxygen, ip, 25, 'O');

        return true;
    }

    void reloadLevel() {
        if (!loadLevel())
            cout << "Reload failed.\n";
        else
            cout << "Reloaded!\n";
    }

    char readInput() {
        cout << ">>>";
        char c;
        cin >> c;
        cout << "\n";
        cin.ignore(10000, '\n');
        return c;
    }

    void update(char cmd) {
        if (cmd == 'r') {
            reloadLevel();
            return;
        }

        if (!player.hasOxygen()) {
            cout << "Out of oxygen. Press 'r' to reload or 'q' to quit.\n";
            return;
        }

        bool moved = false;

        switch (cmd) {
        case 'w': moved = player.tryMove(world, 0, -1); break;
        case 's': moved = player.tryMove(world, 0, 1); break;
        case 'a': moved = player.tryMove(world, -1, 0); break;
        case 'd': moved = player.tryMove(world, 1, 0); break;
        default:
            cout << "Commands: w/a/s/d move, r reload, q quit\n";
            return;
        }

        if (!moved) {
            cout << "Can't move there!\n";
            return;
        }

        handleItemPickup();
        handleEnemyContact();
        handleDeathIfNeeded();
    }

    void handleItemPickup() {
        Position p = player.getPos();

        auto it = find_if(items.begin(), items.end(),
            [&](const Item& item) { return item.getPos() == p; });

        if (it != items.end()) {
            it->apply(player);
            cout << "Picked up oxygen!\n";
            items.erase(it);
        }
    }

    void handleEnemyContact() {
        Position p = player.getPos();

        auto it = find_if(enemies.begin(), enemies.end(),
            [&](const Enemy& e) { return e.getPos() == p; });

        if (it != enemies.end()) {
            player.takeDamage(it->getDamage());
            cout << "Enemy hit you! -" << it->getDamage() << " HP\n";
        }
    }

    void handleDeathIfNeeded() {
        if (!player.isDead()) return;

        player.loseLife();
        cout << "You died. Lives left: " << player.getLives() << "\n";

        reloadLevel();
    }

    void render() {
        world.render();
        cout << "Health: " << player.getHealth() << "\n";
        cout << "Oxygen: " << player.getOxygen() << "%\n";
        cout << "Lives:  " << player.getLives() << "\n";
    }

    void splash() {
        cout << "\nWELCOME to epic Holy Diver v0.01\n";
        cout << "Commands: w/a/s/d move, r reload, q quit\n";
        cout << "Map loads automatically: " << levelPath << "\n\n";
    }

    void quit() {
        cout << "\nBYE! Welcome back soon.\n";
        cout << "Press Enter to exit...";
        cin.get();
    }

private:
    string levelPath;
    World world;
    Player player;
    vector<Enemy> enemies;
    vector<Item> items;
};

// =====================
// Main function
// =====================

int main() {
    Game game("level_0.map");
    game.run();
    return 0;
}
