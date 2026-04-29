#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <random>
#include <limits>
#include <cctype>

using namespace std;

// =====================
// Utility
// =====================

struct Position {
    int x = -1;
    int y = -1;

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
};

class Random {
public:
    static mt19937& engine() {
        static mt19937 gen(random_device{}());
        return gen;
    }

    static int nextInt(int left, int right) {
        uniform_int_distribution<int> dist(left, right);
        return dist(engine());
    }
};

class World;

// =====================
// Player
// =====================

class Player {
public:
    static constexpr int MAX_HEALTH = 100;
    static constexpr int MAX_OXYGEN = 100;
    static constexpr int MAX_BATTERY = 100;

    void reset() {
        health = MAX_HEALTH;
        oxygen = MAX_OXYGEN;
        battery = MAX_BATTERY;
        score = 0;
        pos = { -1, -1 };
    }

    void resetPositionOnly() {
        pos = { -1, -1 };
    }

    void refillForNewLevel() {
        oxygen = MAX_OXYGEN;
        battery = MAX_BATTERY;
    }

    void setPosition(Position p) {
        pos = p;
    }

    const Position& getPosition() const {
        return pos;
    }

    int getHealth() const {
        return health;
    }

    int getOxygen() const {
        return oxygen;
    }

    int getBattery() const {
        return battery;
    }

    int getScore() const {
        return score;
    }

    void addScore(int amount) {
        score += amount;
        if (score < 0) {
            score = 0;
        }
    }

    void takeDamage(int amount) {
        health -= amount;
        if (health < 0) {
            health = 0;
        }
    }

    void consumeOxygen(int amount) {
        oxygen -= amount;
        if (oxygen < 0) {
            oxygen = 0;
        }
    }

    void addOxygen(int amount) {
        oxygen += amount;
        if (oxygen > MAX_OXYGEN) {
            oxygen = MAX_OXYGEN;
        }
    }

    void addBattery(int amount) {
        battery += amount;
        if (battery > MAX_BATTERY) {
            battery = MAX_BATTERY;
        }
    }

    bool canSpendBattery(int amount) const {
        return battery >= amount;
    }

    void spendBattery(int amount) {
        battery -= amount;
        if (battery < 0) {
            battery = 0;
        }
    }

    bool isDead() const {
        return health <= 0 || oxygen <= 0;
    }

private:
    int health = MAX_HEALTH;
    int oxygen = MAX_OXYGEN;
    int battery = MAX_BATTERY;
    int score = 0;
    Position pos;
};

// =====================
// Items
// =====================

class Item {
public:
    Item(Position pos, int value, char symbol, int scoreValue)
        : pos(pos), value(value), symbol(symbol), scoreValue(scoreValue) {
    }

    virtual ~Item() = default;

    const Position& getPosition() const {
        return pos;
    }

    char getSymbol() const {
        return symbol;
    }

    int getScoreValue() const {
        return scoreValue;
    }

    virtual void apply(Player& player) const = 0;
    virtual string getPickupMessage() const = 0;

protected:
    Position pos;
    int value = 0;
    char symbol = '?';
    int scoreValue = 0;
};

class OxygenItem : public Item {
public:
    OxygenItem(Position pos, int value = 25, char symbol = 'O', int scoreValue = 10)
        : Item(pos, value, symbol, scoreValue) {
    }

    void apply(Player& player) const override {
        player.addOxygen(value);
        player.addScore(getScoreValue());
    }

    string getPickupMessage() const override {
        return "Picked up extra oxygen! +10 score";
    }
};

class BatteryItem : public Item {
public:
    BatteryItem(Position pos, int value = 20, char symbol = 'B', int scoreValue = 10)
        : Item(pos, value, symbol, scoreValue) {
    }

    void apply(Player& player) const override {
        player.addBattery(value);
        player.addScore(getScoreValue());
    }

    string getPickupMessage() const override {
        return "Picked up battery! +10 score";
    }
};

// =====================
// Enemies
// =====================

class Enemy {
public:
    Enemy(Position pos, int damage, char symbol)
        : pos(pos), damage(damage), symbol(symbol) {
    }

    virtual ~Enemy() = default;

    const Position& getPosition() const {
        return pos;
    }

    void setPosition(Position p) {
        pos = p;
    }

    int giveDamage() const {
        return damage;
    }

    char getSymbol() const {
        return symbol;
    }

    bool isActive() const {
        return active;
    }

    void activate() {
        active = true;
    }

    virtual void move(World& world) = 0;

private:
    Position pos;
    int damage = 10;
    char symbol = 'M';
    bool active = false;
};

class StationaryEnemy : public Enemy {
public:
    StationaryEnemy(Position pos, int damage = 10, char symbol = 'M')
        : Enemy(pos, damage, symbol) {
    }

    void move(World&) override {
        // Stationary enemies intentionally do not move.
    }
};

class MovingEnemy : public Enemy {
public:
    MovingEnemy(Position pos, int damage = 10, char symbol = 'M')
        : Enemy(pos, damage, symbol) {
    }

    void move(World& world) override;
};

// =====================
// World
// =====================

class World {
public:
    bool loadFromFile(const string& filePath, bool keepPlayerState = false) {
        originalMapPath = filePath;

        tiles.clear();
        visible.clear();
        enemies.clear();
        items.clear();
        width = 0;
        height = 0;
        totalItemsOnLevel = 0;
        collectedItemsOnLevel = 0;

        ifstream in(filePath);
        if (!in) {
            cout << "Failed to open map file: " << filePath << "\n";
            return false;
        }

        string line;
        while (getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                tiles.push_back(line);
            }
        }

        height = static_cast<int>(tiles.size());
        if (height == 0) {
            cout << "Map file is empty or invalid.\n";
            return false;
        }

        width = static_cast<int>(tiles[0].size());
        if (width == 0) {
            cout << "Map file is empty or invalid.\n";
            return false;
        }

        for (string& row : tiles) {
            if ((int)row.size() < width) {
                row += string(width - row.size(), 'x');
            }
            else if ((int)row.size() > width) {
                row.resize(width);
            }
        }

        visible.assign(height, vector<bool>(width, false));

        if (!keepPlayerState) {
            player.reset();
        }
        else {
            player.resetPositionOnly();
        }

        return extractObjectsFromMap();
    }

    bool reload(bool keepPlayerState = false) {
        if (originalMapPath.empty()) {
            return false;
        }
        return loadFromFile(originalMapPath, keepPlayerState);
    }

    Player& getPlayer() {
        return player;
    }

    const Player& getPlayer() const {
        return player;
    }

    bool isPlayerDead() const {
        return player.isDead();
    }

    bool isLevelCompleted() const {
        return totalItemsOnLevel > 0 && collectedItemsOnLevel == totalItemsOnLevel && !player.isDead();
    }

    int getCollectedItemsOnLevel() const {
        return collectedItemsOnLevel;
    }

    int getTotalItemsOnLevel() const {
        return totalItemsOnLevel;
    }

    double getCollectedRatio() const {
        if (totalItemsOnLevel == 0) {
            return 0.0;
        }
        return static_cast<double>(collectedItemsOnLevel) / totalItemsOnLevel;
    }

    void render() const {
        cout << "\n--- MAP ---\n";

        Position pp = player.getPosition();

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (pp.x == x && pp.y == y) {
                    cout << 'P';
                    continue;
                }

                if (!isVisible(x, y)) {
                    cout << ' ';
                    continue;
                }

                const Enemy* enemy = getEnemyAt(x, y);
                if (enemy != nullptr) {
                    cout << enemy->getSymbol();
                    continue;
                }

                const Item* item = getItemAt(x, y);
                if (item != nullptr) {
                    cout << item->getSymbol();
                    continue;
                }

                cout << tiles[y][x];
            }
            cout << "\n";
        }

        cout << "Health:   " << player.getHealth() << "\n";
        cout << "Oxygen:   " << player.getOxygen() << "%\n";
        cout << "Battery:  " << player.getBattery() << "%\n";
        cout << "Score:    " << player.getScore() << "\n";
        cout << "Items:    " << collectedItemsOnLevel << "/" << totalItemsOnLevel << "\n";
    }

    bool requestPlayerMove(int dx, int dy) {
        player.consumeOxygen(2);

        Position oldPos = player.getPosition();
        Position newPos{ oldPos.x + dx, oldPos.y + dy };

        if (!inBounds(newPos.x, newPos.y)) {
            cout << "You cannot move outside the map.\n";
            return false;
        }

        if (!isWalkableBase(newPos.x, newPos.y)) {
            cout << "Wall blocks the way.\n";
            return false;
        }

        Enemy* enemy = getEnemyAtMutable(newPos.x, newPos.y);
        if (enemy != nullptr) {
            player.takeDamage(enemy->giveDamage());
            enemy->activate();
            cout << "You bumped into an enemy! -" << enemy->giveDamage() << " HP\n";
            return false;
        }

        player.setPosition(newPos);
        reveal(newPos.x, newPos.y);
        handleItemPickup();
        activateSeenEnemies();
        moveEnemies();
        return true;
    }

    bool illuminateTile(int dx, int dy) {
        player.consumeOxygen(2);

        Position pp = player.getPosition();
        int tx = pp.x + dx;
        int ty = pp.y + dy;

        if (!inBounds(tx, ty)) {
            cout << "Can't illuminate outside the map.\n";
            return false;
        }

        if (!player.canSpendBattery(5)) {
            cout << "Battery empty!\n";
            return false;
        }

        player.spendBattery(5);
        reveal(tx, ty);
        cout << "Illuminated tile (" << tx << "," << ty << ") -5% battery\n";

        activateSeenEnemies();
        moveEnemies();
        return true;
    }

    bool inBounds(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    bool tryMoveEnemy(Enemy& enemy, int dx, int dy) {
        Position oldPos = enemy.getPosition();
        Position newPos{ oldPos.x + dx, oldPos.y + dy };

        if (!inBounds(newPos.x, newPos.y)) {
            return false;
        }

        if (!isWalkableBase(newPos.x, newPos.y)) {
            return false;
        }

        if (isEnemyAt(newPos.x, newPos.y)) {
            return false;
        }

        Position pp = player.getPosition();
        if (pp == newPos) {
            player.takeDamage(enemy.giveDamage());
            cout << "Enemy hit you! -" << enemy.giveDamage() << " HP\n";
            return false;
        }

        enemy.setPosition(newPos);
        return true;
    }

private:
    bool extractObjectsFromMap() {
        Position playerPos = findFirst('P');
        if (playerPos.x == -1) {
            cout << "No 'P' found on the map!\n";
            return false;
        }

        player.setPosition(playerPos);
        reveal(playerPos.x, playerPos.y);
        setTile(playerPos.x, playerPos.y, 'o');

        vector<Position> enemyPositions = findAll('M');
        for (const Position& pos : enemyPositions) {
            setTile(pos.x, pos.y, 'o');

            // Random enemy type makes each playthrough slightly different.
            if (Random::nextInt(0, 1) == 0) {
                enemies.push_back(make_unique<StationaryEnemy>(pos));
            }
            else {
                enemies.push_back(make_unique<MovingEnemy>(pos));
            }
        }

        vector<Position> oxygenPositions = findAll('O');
        for (const Position& pos : oxygenPositions) {
            items.push_back(make_unique<OxygenItem>(pos, 25, 'O', 10));
            setTile(pos.x, pos.y, 'o');
        }

        vector<Position> batteryPositions = findAll('B');
        for (const Position& pos : batteryPositions) {
            items.push_back(make_unique<BatteryItem>(pos, 20, 'B', 10));
            setTile(pos.x, pos.y, 'o');
        }

        totalItemsOnLevel = static_cast<int>(items.size());
        return true;
    }

    Position findFirst(char symbol) const {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (tiles[y][x] == symbol) {
                    return { x, y };
                }
            }
        }
        return { -1, -1 };
    }

    vector<Position> findAll(char symbol) const {
        vector<Position> result;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (tiles[y][x] == symbol) {
                    result.push_back({ x, y });
                }
            }
        }
        return result;
    }

    void setTile(int x, int y, char c) {
        if (inBounds(x, y)) {
            tiles[y][x] = c;
        }
    }

    char getTile(int x, int y) const {
        if (!inBounds(x, y)) {
            return '\0';
        }
        return tiles[y][x];
    }

    bool isWalkableBase(int x, int y) const {
        char c = getTile(x, y);
        return c != '\0' && c != 'x';
    }

    bool isVisible(int x, int y) const {
        if (!inBounds(x, y)) {
            return false;
        }
        return visible[y][x];
    }

    void reveal(int x, int y) {
        if (inBounds(x, y)) {
            visible[y][x] = true;
        }
    }

    const Enemy* getEnemyAt(int x, int y) const {
        for (const auto& enemy : enemies) {
            Position ep = enemy->getPosition();
            if (ep.x == x && ep.y == y) {
                return enemy.get();
            }
        }
        return nullptr;
    }

    Enemy* getEnemyAtMutable(int x, int y) {
        for (auto& enemy : enemies) {
            Position ep = enemy->getPosition();
            if (ep.x == x && ep.y == y) {
                return enemy.get();
            }
        }
        return nullptr;
    }

    bool isEnemyAt(int x, int y) const {
        return getEnemyAt(x, y) != nullptr;
    }

    const Item* getItemAt(int x, int y) const {
        for (const auto& item : items) {
            Position ip = item->getPosition();
            if (ip.x == x && ip.y == y) {
                return item.get();
            }
        }
        return nullptr;
    }

    bool inPlayerFieldOfView(const Position& enemyPos) const {
        Position pp = player.getPosition();
        int dx = enemyPos.x - pp.x;
        int dy = enemyPos.y - pp.y;

        return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
    }

    void activateSeenEnemies() {
        for (auto& enemy : enemies) {
            Position ep = enemy->getPosition();
            if (isVisible(ep.x, ep.y) && inPlayerFieldOfView(ep)) {
                enemy->activate();
            }
        }
    }

    void moveEnemies() {
        for (auto& enemy : enemies) {
            if (enemy->isActive()) {
                enemy->move(*this);
            }
        }
    }

    void handleItemPickup() {
        Position pp = player.getPosition();

        auto it = find_if(items.begin(), items.end(),
            [&](const unique_ptr<Item>& item) {
                return item->getPosition() == pp;
            });

        if (it != items.end()) {
            (*it)->apply(player);
            collectedItemsOnLevel++;
            cout << (*it)->getPickupMessage() << "\n";
            cout << "Collected items: " << collectedItemsOnLevel << "/" << totalItemsOnLevel << "\n";
            items.erase(it);

            if (collectedItemsOnLevel == totalItemsOnLevel && totalItemsOnLevel > 0) {
                cout << "\nAll items on this level collected!\n";
            }
        }
    }

private:
    string originalMapPath;
    vector<string> tiles;
    vector<vector<bool>> visible;
    int width = 0;
    int height = 0;

    int totalItemsOnLevel = 0;
    int collectedItemsOnLevel = 0;

    Player player;
    vector<unique_ptr<Enemy>> enemies;
    vector<unique_ptr<Item>> items;
};

void MovingEnemy::move(World& world) {
    if (Random::nextInt(0, 99) < 30) {
        return;
    }

    static const int dirs[4][2] = {
        {0, -1},
        {0, 1},
        {-1, 0},
        {1, 0}
    };

    for (int attempt = 0; attempt < 4; ++attempt) {
        int index = Random::nextInt(0, 3);
        int dx = dirs[index][0];
        int dy = dirs[index][1];

        if (world.tryMoveEnemy(*this, dx, dy)) {
            return;
        }
    }
}

// =====================
// Game
// =====================

class Game {
public:
    explicit Game(string firstMapPath)
        : currentMapPath(move(firstMapPath)) {
    }

    void run() {
        showIntro();
        showHallOfFame();

        if (!world.loadFromFile(currentMapPath, false)) {
            cout << "Could not start the game.\n";
            waitForExit();
            return;
        }

        running = true;
        levelNumber = extractLevelNumber(currentMapPath);

        while (running) {
            world.render();

            if (world.isPlayerDead()) {
                showDeathMessage();
                break;
            }

            if (world.isLevelCompleted()) {
                finishCurrentLevel();
                continue;
            }

            char command = readCommand();
            handleCommand(command);
        }

        waitForExit();
    }

private:
    void showIntro() const {
        cout << "Epic Holy Diver Game\n\n";

        cout << "Goal:\n";
        cout << "  Collect all treasures/items on each underwater map.\n";
        cout << "  Manage oxygen, battery and health carefully.\n\n";

        cout << "Commands:\n";
        cout << "  w/a/s/d - move\n";
        cout << "  i/j/k/l - illuminate adjacent tile\n";
        cout << "  e       - abort current level safely\n";
        cout << "  r       - restart current map from beginning\n";
        cout << "  h       - show hall of fame\n";
        cout << "  q       - quit\n\n";

        cout << "Map symbols:\n";
        cout << "  x - wall\n";
        cout << "  o - floor\n";
        cout << "  P - player start\n";
        cout << "  M - enemy\n";
        cout << "  O - oxygen item\n";
        cout << "  B - battery item\n\n";
    }

    char readCommand() const {
        cout << ">>> ";
        string input;
        getline(cin, input);

        if (input.empty()) {
            return '\0';
        }

        return static_cast<char>(tolower(static_cast<unsigned char>(input[0])));
    }

    void handleCommand(char command) {
        switch (command) {
        case 'w':
            world.requestPlayerMove(0, -1);
            break;
        case 's':
            world.requestPlayerMove(0, 1);
            break;
        case 'a':
            world.requestPlayerMove(-1, 0);
            break;
        case 'd':
            world.requestPlayerMove(1, 0);
            break;
        case 'i':
            world.illuminateTile(0, -1);
            break;
        case 'k':
            world.illuminateTile(0, 1);
            break;
        case 'j':
            world.illuminateTile(-1, 0);
            break;
        case 'l':
            world.illuminateTile(1, 0);
            break;
        case 'e':
            abortCurrentLevel();
            break;
        case 'r':
            restartCurrentMap();
            break;
        case 'h':
            showHallOfFame();
            break;
        case 'q':
            running = false;
            cout << "Quitting game.\n";
            break;
        case '\0':
            cout << "Empty command. Please enter one command character.\n";
            break;
        default:
            cout << "Unknown command. Use w/a/s/d, i/j/k/l, e, r, h or q.\n";
            break;
        }
    }

    void restartCurrentMap() {
        if (world.reload(false)) {
            cout << "Current map restarted from the beginning.\n";
        }
        else {
            cout << "Restart failed.\n";
        }
    }

    void abortCurrentLevel() {
        int collected = world.getCollectedItemsOnLevel();
        int total = world.getTotalItemsOnLevel();
        double ratio = world.getCollectedRatio();

        cout << "\n=== LEVEL ABORTED ===\n";
        cout << "You surfaced safely.\n";
        cout << "Items found on this level are lost: " << collected << "/" << total << "\n";

        cout << "\nChoose what to do next:\n";
        cout << "  1 - restart the same map\n";

        if (levelNumber > 1) {
            cout << "  2 - return to previous map and start it from beginning\n";
        }

        if (ratio >= 0.5) {
            cout << "  3 - continue to next map because at least 50% was found\n";
        }

        cout << "Choice: ";

        string input;
        getline(cin, input);

        if (input == "1") {
            restartCurrentMap();
            return;
        }

        if (input == "2" && levelNumber > 1) {
            string previousMap = buildPreviousLevelPath(currentMapPath);

            if (world.loadFromFile(previousMap, false)) {
                currentMapPath = previousMap;
                levelNumber--;
                cout << "Returned to previous map: " << currentMapPath << "\n";
            }
            else {
                cout << "Previous map could not be loaded. Restarting current map instead.\n";
                restartCurrentMap();
            }
            return;
        }

        if (input == "3" && ratio >= 0.5) {
            string nextMapPath = buildNextLevelPath(currentMapPath);

            if (world.loadFromFile(nextMapPath, false)) {
                currentMapPath = nextMapPath;
                levelNumber++;
                cout << "Progress accepted. Loading next map: " << currentMapPath << "\n";
            }
            else {
                cout << "No next map found. Game finished.\n";
                saveHallOfFame();
                running = false;
            }
            return;
        }

        cout << "Invalid choice. Restarting the same map for safety.\n";
        restartCurrentMap();
    }

    void finishCurrentLevel() {
        totalCollectedItems += world.getCollectedItemsOnLevel();

        cout << "\n=== LEVEL COMPLETED ===\n";
        cout << "Level " << levelNumber << " finished.\n";
        cout << "Session collected items: " << totalCollectedItems << "\n";
        cout << "Total score: " << world.getPlayer().getScore() << "\n";

        string nextMapPath = buildNextLevelPath(currentMapPath);

        world.getPlayer().refillForNewLevel();

        if (!world.loadFromFile(nextMapPath, true)) {
            cout << "\nNo next level found. You completed all available levels!\n";
            cout << "Final score: " << world.getPlayer().getScore() << "\n";
            cout << "Total collected items: " << totalCollectedItems << "\n";
            saveHallOfFame();
            running = false;
            return;
        }

        currentMapPath = nextMapPath;
        levelNumber++;

        cout << "\nLoading next level: " << currentMapPath << "\n";
        cout << "Oxygen and battery restored for the new level.\n\n";
    }

    void showDeathMessage() const {
        const Player& player = world.getPlayer();

        if (player.getHealth() <= 0) {
            cout << "\nGame Over: health reached 0.\n";
        }
        else if (player.getOxygen() <= 0) {
            cout << "\nGame Over: oxygen reached 0.\n";
        }
        else {
            cout << "\nGame Over.\n";
        }

        cout << "Final score: " << player.getScore() << "\n";
        cout << "Collected items in session: "
            << totalCollectedItems + world.getCollectedItemsOnLevel() << "\n";
    }

    void saveHallOfFame() const {
        string name;

        cout << "\nEnter your name for Hall of Fame: ";
        getline(cin, name);

        if (name.empty()) {
            name = "Unknown Diver";
        }

        ofstream out("hall_of_fame.txt", ios::app);
        if (!out) {
            cout << "Could not save Hall of Fame file.\n";
            return;
        }

        out << name << " | Score: " << world.getPlayer().getScore()
            << " | Items: " << totalCollectedItems << "\n";

        cout << "Result saved to hall_of_fame.txt\n";
    }

    void showHallOfFame() const {
        ifstream in("hall_of_fame.txt");

        cout << "\n=== HALL OF FAME ===\n";

        if (!in) {
            cout << "No records yet.\n\n";
            return;
        }

        string line;
        bool hasRecords = false;

        while (getline(in, line)) {
            if (!line.empty()) {
                cout << line << "\n";
                hasRecords = true;
            }
        }

        if (!hasRecords) {
            cout << "No records yet.\n";
        }

        cout << "\n";
    }

    void waitForExit() const {
        cout << "Press Enter to exit...";
        string dummy;
        getline(cin, dummy);
    }

    int extractLevelNumber(const string& path) const {
        int number = 0;
        bool found = false;

        for (char c : path) {
            if (isdigit(static_cast<unsigned char>(c))) {
                found = true;
                number = number * 10 + (c - '0');
            }
        }

        return found ? number : 1;
    }

    string buildNextLevelPath(const string& currentPath) const {
        return replaceLastNumber(currentPath, 1);
    }

    string buildPreviousLevelPath(const string& currentPath) const {
        return replaceLastNumber(currentPath, -1);
    }

    string replaceLastNumber(const string& currentPath, int delta) const {
        string result = currentPath;

        int lastDigitPos = -1;
        for (int i = static_cast<int>(result.size()) - 1; i >= 0; --i) {
            if (isdigit(static_cast<unsigned char>(result[i]))) {
                lastDigitPos = i;
                break;
            }
        }

        if (lastDigitPos == -1) {
            return result;
        }

        int firstDigitPos = lastDigitPos;
        while (firstDigitPos - 1 >= 0 &&
            isdigit(static_cast<unsigned char>(result[firstDigitPos - 1]))) {
            firstDigitPos--;
        }

        int currentNumber = stoi(result.substr(firstDigitPos, lastDigitPos - firstDigitPos + 1));
        int newNumber = currentNumber + delta;

        if (newNumber < 1) {
            newNumber = 1;
        }

        result.replace(firstDigitPos, lastDigitPos - firstDigitPos + 1, to_string(newNumber));
        return result;
    }

private:
    string currentMapPath;
    World world;
    bool running = false;

    int totalCollectedItems = 0;
    int levelNumber = 1;
};

// =====================
// Main
// =====================

int main() {
    string mapPath;

    cout << "Enter map file path, for example level1.map: ";
    getline(cin, mapPath);

    if (!mapPath.empty() && mapPath.front() == '"' && mapPath.back() == '"') {
        mapPath = mapPath.substr(1, mapPath.size() - 2);
    }

    if (mapPath.empty()) {
        cout << "No file path provided.\n";
        return 1;
    }

    Game game(mapPath);
    game.run();

    return 0;
}