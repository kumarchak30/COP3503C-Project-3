#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <vector>
#include <map>
#include <SFML/Graphics.hpp>
#include <random>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>
using namespace std;

void setText(sf::Text& text, float x, float y) {
    sf::FloatRect textRect = text.getLocalBounds();
    text.setOrigin({textRect.position.x + textRect.size.x / 2.0f, textRect.position.y + textRect.size.y / 2.0f});
    text.setPosition(sf::Vector2f(x, y));
}

// holding textures
struct TextureManager {
    std::map<std::string, sf::Texture> textures;

    void load(const std::string& name, const std::string& path) {
        sf::Texture tex;
        if (tex.loadFromFile(path)) {
            textures[name] = tex;
        }
    }
    sf::Texture& get(const std::string& name) {
        return textures.at(name);
    }
};

//tiles
struct Tile {
    sf::Sprite sprite;
    sf::Sprite flag_sprite;
    sf::Sprite mine_sprite;
    sf::Vector2f position;

    bool isMine = false;
    bool isFlagged = false;
    bool isRevealed = false;
    int adjacentMines = 0;

    Tile(float x, float y, TextureManager& texm)
        : sprite(texm.get("hidden")),
          flag_sprite(texm.get("flag")),
          mine_sprite(texm.get("mine")),
          position({x, y})
    {
        sprite.setPosition(position);
        flag_sprite.setPosition(position);
        mine_sprite.setPosition(position);
    }

    int toggleFlag() {
        if (isRevealed) {
            return 0;
        }
        isFlagged = !isFlagged;
        return isFlagged ? 1 : -1;
    }

    void reveal() {
        if (isFlagged || isRevealed) {
            return;
        }
        isRevealed = true;
    }

    void draw(sf::RenderWindow& window, TextureManager& texm, bool isDebug) {
        if (isRevealed) {
            if (isMine) {
                sprite.setTexture(texm.get("mine"));
            } else if (adjacentMines > 0) {
                sprite.setTexture(texm.get("number_" + std::to_string(adjacentMines)));
            } else {
                sprite.setTexture(texm.get("revealed"));
            }
        } else {
            sprite.setTexture(texm.get("hidden"));
        }

        window.draw(sprite);

        if (!isRevealed && isFlagged) {
            window.draw(flag_sprite);
        }

        if (isDebug && isMine) {
            window.draw(mine_sprite);
        }
    }
};

// calculating the adjacency for all the mines
void calculateAdjacency(std::vector<Tile> &tiles, int columns, int rows) {
    for (int i = 0; i < tiles.size(); i++) {
        if (tiles[i].isMine) {
            tiles[i].adjacentMines = -1;
            continue;
        }

        int count = 0;
        int currentX = i % columns;
        int currentY = i / columns;

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;

                int neighborX = currentX + dx;
                int neighborY = currentY + dy;

                if (neighborX >= 0 && neighborX < columns && neighborY >= 0 && neighborY < rows) {
                    int neighborIndex = neighborX + (neighborY * columns);
                    if (tiles[neighborIndex].isMine) {
                        count++;
                    }
                }
            }
        }
        tiles[i].adjacentMines = count;
    }
}

void revealTile(std::vector<Tile>& tiles, int index, int columns, int rows) {
    if (tiles[index].isRevealed || tiles[index].isFlagged) {
        return;
    }

    tiles[index].isRevealed = true;

    if (tiles[index].adjacentMines > 0) {
        return;
    }

    int currentX = index % columns;
    int currentY = index / columns;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;

            int neighborX = currentX + dx;
            int neighborY = currentY + dy;

            if (neighborX >= 0 && neighborX < columns && neighborY >= 0 && neighborY < rows) {
                int neighborIndex = neighborX + (neighborY * columns);
                if (!tiles[neighborIndex].isMine) {
                    revealTile(tiles, neighborIndex, columns, rows);
                }
            }
        }
    }
}

bool checkWin(const std::vector<Tile>& tiles, int mineCount) {
    int revealedCount = 0;
    for (const auto& tile : tiles) {
        if (tile.isRevealed) {
            revealedCount++;
        }
    }
    return revealedCount == (tiles.size() - mineCount);
}

void setGameLost(std::vector<Tile>& tiles, sf::Sprite& faceSprite, TextureManager& texm) {
    faceSprite.setTexture(texm.get("lose"));
    for (auto& tile : tiles) {
        if (tile.isMine) {
            tile.isRevealed = true;
        }
    }
}

void setGameWon(std::vector<Tile>& tiles, sf::Sprite& faceSprite, TextureManager& texm, int& flagCount, int mineCount) {
    faceSprite.setTexture(texm.get("win"));
    flagCount = 0;
    for (auto& tile : tiles) {
        if (tile.isMine) {
            tile.isFlagged = true;
            flagCount++;
        }
    }
    flagCount = mineCount;
}

void updateTimer(std::chrono::high_resolution_clock::time_point startTime, std::chrono::high_resolution_clock::time_point pausedTime, bool isPaused, sf::Sprite timerDigits[4], sf::Texture& digitTexture) {
    auto now = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    int totalSeconds = duration.count();

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    int digits[4];
    digits[0] = minutes / 10;
    digits[1] = minutes % 10;
    digits[2] = seconds / 10;
    digits[3] = seconds % 10;

    const int DIGIT_WIDTH = 21;
    const int DIGIT_HEIGHT = 32;

    for (int i = 0; i < 4; i++) {
        sf::Rect<int> rect({0, 0}, {DIGIT_WIDTH, DIGIT_HEIGHT});
        rect.position.x = digits[i] * DIGIT_WIDTH;
        timerDigits[i].setTextureRect(rect);
    }
}

// randomizing the mines
void setupBoard(std::vector<Tile>& tiles, int mineCount, int columns, int rows) {
    // reset all tiles
    for (auto& tile : tiles) {
        tile.isMine = false;
        tile.isFlagged = false;
        tile.isRevealed = false;
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, (columns * rows) - 1);

    int minesPlaced = 0;
    while (minesPlaced < mineCount) {
        int index = dist(rng);
        if (!tiles[index].isMine) {
            tiles[index].isMine = true;
            minesPlaced++;
        }
    }
}

void updateCounter(int count, sf::Sprite counterDigits[3], sf::Texture& digitTexture) {
    const int DIGIT_WIDTH = 21;
    const int DIGIT_HEIGHT = 32;

    sf::Rect<int> rect({0, 0}, {DIGIT_WIDTH, DIGIT_HEIGHT});

    if (count < 0) {
        rect.position.x = 10 * DIGIT_WIDTH;
        counterDigits[0].setTextureRect(rect);
        count = -count;
    } else {
        rect.position.x = (count / 100) % 10 * DIGIT_WIDTH;
        counterDigits[0].setTextureRect(rect);
    }

    rect.position.x = (count / 10) % 10 * DIGIT_WIDTH;
    counterDigits[1].setTextureRect(rect);

    rect.position.x = count % 10 * DIGIT_WIDTH;
    counterDigits[2].setTextureRect(rect);
}

void showLeaderboard(sf::Font& font, unsigned int parentWidth, unsigned int parentHeight, int highlightRank = -1) {
    unsigned int lbWidth = parentWidth / 2;
    unsigned int lbHeight = (parentHeight - 100) / 2 + 50;

    sf::RenderWindow leaderWindow(sf::VideoMode({lbWidth, lbHeight}), "Leaderboard", sf::Style::Close);

    std::string leaderboardContent;
    std::ifstream file("files/leaderboard.txt");
    if (file.is_open()) {
        std::string line;
        int rank = 0; // 0-based index for comparison

        while (std::getline(file, line)) {
            size_t commaPos = line.find(',');
            std::string time = line.substr(0, commaPos);
            std::string name = line.substr(commaPos + 1);

            if (rank == highlightRank) {
                name += "*";
            }

            leaderboardContent += std::to_string(rank + 1) + ".\t" + time + "\t" + name + "\n\n";
            rank++;
        }
        file.close();
    }

    sf::Text title(font, "LEADERBOARD", 20);
    title.setStyle(sf::Text::Bold | sf::Text::Underlined);
    setText(title, lbWidth / 2.0f, (lbHeight / 2.0f) - 120);

    sf::Text content(font, leaderboardContent, 18);
    content.setStyle(sf::Text::Bold);
    setText(content, lbWidth / 2.0f, (lbHeight / 2.0f) + 20);

    while (leaderWindow.isOpen()) {
        while (const auto event = leaderWindow.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                leaderWindow.close();
            }
        }
        leaderWindow.clear(sf::Color::Blue);
        leaderWindow.draw(title);
        leaderWindow.draw(content);
        leaderWindow.display();
    }
}

int updateLeaderboard(int newTime, std::string name) {
    std::vector<std::pair<int, std::string>> scores;
    std::ifstream fileIn("files/leaderboard.txt");

    if (fileIn.is_open()) {
        std::string line;
        while (std::getline(fileIn, line)) {
            size_t commaPos = line.find(',');
            if (commaPos != std::string::npos) {
                std::string timeStr = line.substr(0, commaPos);
                std::string playerName = line.substr(commaPos + 1);

                size_t colonPos = timeStr.find(':');
                int minutes = std::stoi(timeStr.substr(0, colonPos));
                int seconds = std::stoi(timeStr.substr(colonPos + 1));
                int totalSeconds = (minutes * 60) + seconds;

                scores.push_back({totalSeconds, playerName});
            }
        }
        fileIn.close();
    }

    scores.push_back({newTime, name});

    std::sort(scores.begin(), scores.end());

    if (scores.size() > 5) {
        scores.resize(5);
    }

    std::ofstream fileOut("files/leaderboard.txt", std::ios::trunc);
    if (fileOut.is_open()) {
        for (const auto& entry : scores) {
            int mins = entry.first / 60;
            int secs = entry.first % 60;

            fileOut << std::setfill('0') << std::setw(2) << mins << ":"
                    << std::setw(2) << secs << ","
                    << entry.second << "\n";
        }
        fileOut.close();
    }

    for (int i = 0; i < scores.size(); i++) {
        if (scores[i].first == newTime && scores[i].second == name) {
            return i; // Return the index (0-based)
        }
    }

    return -1;
}

int main() {
    unsigned int columns, rows, minecount;
    ifstream configFile("files/config.cfg");
    if (configFile.is_open()) {
        configFile >> columns >> rows >> minecount;
        configFile.close();
    } else {
        cerr << "Could not open files/config.cfg" << endl;
        return 1;
    }

    unsigned int windowWidth = columns * 32;
    unsigned int windowHeight = (rows * 32) + 100;

    sf::RenderWindow welcomeWindow(sf::VideoMode({windowWidth, windowHeight}), "SFML Window", sf::Style::Close);

    sf::Font font;
    if (!font.openFromFile("files/font.ttf")) {
        cerr << "Could not open files/font.ttf" << endl;
        return 1;
    }

    sf::Text text(font);
    text.setCharacterSize(24);
    text.setFillColor(sf::Color::White);
    text.setStyle(sf::Text::Bold | sf::Text::Underlined);
    text.setString("WELCOME TO MINESWEEPER!");
    setText(text, windowWidth / 2.0f, (windowHeight / 2.0f) - 150);

    sf::Text enterNametext(font);
    enterNametext.setCharacterSize(20);
    enterNametext.setStyle(sf::Text::Bold);
    enterNametext.setFillColor(sf::Color::White);
    enterNametext.setString("Enter your name:");
    setText(enterNametext, windowWidth / 2.0f, (windowHeight / 2.0f) - 75);

    std::string playerName;
    sf::Text nameText(font);
    nameText.setCharacterSize(18);
    nameText.setFillColor(sf::Color::Yellow);
    nameText.setStyle(sf::Text::Bold);

    bool openGameWindow = false;


    while (welcomeWindow.isOpen())
    {
        while (const optional event = welcomeWindow.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                openGameWindow = false;
                welcomeWindow.close();
            }

            if (const auto* textEntered = event->getIf<sf::Event::TextEntered>())
            {
                char typedChar = static_cast<char>(textEntered->unicode);
                if (std::isalpha(typedChar) && playerName.length() < 10)
                {
                    playerName += (playerName.empty() ? std::toupper(typedChar) : std::tolower(typedChar));
                }
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Backspace && !playerName.empty())
                {
                    playerName.pop_back();
                }

                if (keyPressed->code == sf::Keyboard::Key::Enter && !playerName.empty())
                {
                    openGameWindow = true;
                    welcomeWindow.close();
                }
            }
        }

        nameText.setString(playerName + "|");
        setText(nameText, windowWidth / 2.0f, (windowHeight / 2.0f) - 45);

        welcomeWindow.clear(sf::Color::Blue);
        welcomeWindow.draw(text);
        welcomeWindow.draw(enterNametext);
        welcomeWindow.draw(nameText);
        welcomeWindow.display();
    }

    if (!openGameWindow) {return 0;}

    //game window

    sf::RenderWindow gameWindow(sf::VideoMode({windowWidth, windowHeight}), "Minesweeper", sf::Style::Close);
    gameWindow.setFramerateLimit(60); // Good practice

    //img textures
    TextureManager texm;
    texm.load("hidden", "files/images/tile_hidden.png");
    texm.load("revealed", "files/images/tile_revealed.png");
    texm.load("happy", "files/images/face_happy.png");
    texm.load("debug", "files/images/debug.png");
    texm.load("play", "files/images/play.png");
    texm.load("pause", "files/images/pause.png");
    texm.load("leaderboard", "files/images/leaderboard.png");
    texm.load("flag", "files/images/flag.png");
    texm.load("mine", "files/images/mine.png");
    texm.load("digits", "files/images/digits.png");
    texm.load("win", "files/images/face_win.png");
    texm.load("lose", "files/images/face_lose.png");

    bool gameOver = false;
    bool gamePaused = false;

    auto startTime = std::chrono::high_resolution_clock::now();
    auto pauseTime = std::chrono::high_resolution_clock::now();
    long long elapsedPausedTime = 0;

    sf::Sprite timerDigits[4] = {
        sf::Sprite(texm.get("digits")),
        sf::Sprite(texm.get("digits")),
        sf::Sprite(texm.get("digits")),
        sf::Sprite(texm.get("digits"))
    };

    float y_pos_buttons = 32.0f * rows;
    float timer_y = y_pos_buttons + 16.0f;

    timerDigits[0].setPosition({ (columns * 32.0f) - 97.0f, timer_y });
    timerDigits[1].setPosition({ (columns * 32.0f) - 97.0f + 21.0f, timer_y });

    timerDigits[2].setPosition({ (columns * 32.0f) - 54.0f, timer_y });
    timerDigits[3].setPosition({ (columns * 32.0f) - 54.0f + 21.0f, timer_y });

    for(int i=0; i<4; i++) timerDigits[i].setTexture(texm.get("digits"));

    for (int i = 1; i <= 8; i++) {
        std::string name = "number_" + std::to_string(i);
        texm.load(name, "files/images/" + name + ".png");
    }

    sf::Sprite happyFace(texm.get("happy"));
    happyFace.setPosition({((columns * 32.0f) / 2.0f) - 32.0f, y_pos_buttons});

    sf::Sprite debugButton(texm.get("debug"));
    debugButton.setPosition({(columns * 32.0f) - 304.0f, y_pos_buttons});

    sf::Sprite pauseButton(texm.get("pause"));
    pauseButton.setPosition({(columns * 32.0f) - 240.0f, y_pos_buttons});

    sf::Sprite leaderboardButton(texm.get("leaderboard"));
    leaderboardButton.setPosition({(columns * 32.0f) - 176.0f, y_pos_buttons});


    sf::Sprite counterDigits[3] = {
        sf::Sprite(texm.get("digits")),
        sf::Sprite(texm.get("digits")),
        sf::Sprite(texm.get("digits"))
    };

    float counter_y = y_pos_buttons + 16.0f;
    for (int i = 0; i < 3; i++) {
        counterDigits[i].setPosition({ 33.0f + (i * 21.0f), counter_y });
    }

    std::vector<Tile> tiles;
    for (unsigned int j = 0; j < rows; j++) {
        for (unsigned int i = 0; i < columns; i++) {
            tiles.emplace_back(i * 32.0f, j * 32.0f, texm);
        }
    }

    // vars for game state
    bool isDebugMode = false;
    int flagCount = 0;

    // adding the mines
    setupBoard(tiles, minecount, columns, rows);
    calculateAdjacency(tiles, columns, rows);
    updateCounter(minecount - flagCount, counterDigits, texm.get("digits"));


    // game loop
    while (gameWindow.isOpen())
    {
        if (!gameOver && !gamePaused) {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            long long totalSeconds = duration.count() - elapsedPausedTime;

            int minutes = totalSeconds / 60;
            int seconds = totalSeconds % 60;

            int t_digits[4] = { minutes / 10, minutes % 10, seconds / 10, seconds % 10 };
            for (int i = 0; i < 4; i++) {
                sf::Rect<int> rect({0, 0}, {21, 32});
                rect.position.x = t_digits[i] * 21;
                timerDigits[i].setTextureRect(rect);
            }
        }

        while (const optional event = gameWindow.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) gameWindow.close();

            if (event->is<sf::Event::MouseButtonPressed>()) {
                auto mouseButton = event->getIf<sf::Event::MouseButtonPressed>();
                sf::Vector2i mousePos = sf::Mouse::getPosition(gameWindow);
                auto mousePosF = static_cast<sf::Vector2f>(mousePos);

                if (happyFace.getGlobalBounds().contains(mousePosF)) {
                    setupBoard(tiles, minecount, columns, rows);
                    calculateAdjacency(tiles, columns, rows);

                    // reset stuff
                    flagCount = 0;
                    gameOver = false;
                    gamePaused = false;
                    isDebugMode = false;
                    startTime = std::chrono::high_resolution_clock::now();
                    elapsedPausedTime = 0;

                    happyFace.setTexture(texm.get("happy"));
                    pauseButton.setTexture(texm.get("pause"));
                    updateCounter(minecount, counterDigits, texm.get("digits"));

                    sf::Rect<int> zeroRect({0, 0}, {21, 32});
                    for(int i=0; i<4; i++) timerDigits[i].setTextureRect(zeroRect);
                    continue;
                }

                if (!gameOver && pauseButton.getGlobalBounds().contains(mousePosF)) {
                    gamePaused = !gamePaused;
                    if (gamePaused) {
                        pauseTime = std::chrono::high_resolution_clock::now();
                        pauseButton.setTexture(texm.get("play"));
                    } else {
                        auto now = std::chrono::high_resolution_clock::now();
                        elapsedPausedTime += std::chrono::duration_cast<std::chrono::seconds>(now - pauseTime).count();
                        pauseButton.setTexture(texm.get("pause"));
                    }
                }

                if (leaderboardButton.getGlobalBounds().contains(mousePosF)) {
                    gamePaused = true;
                    pauseButton.setTexture(texm.get("play"));
                    pauseTime = std::chrono::high_resolution_clock::now();

                    gameWindow.clear(sf::Color::White);
                    sf::Sprite revealedSprite(texm.get("revealed"));
                    for (const auto& tile : tiles) {
                        revealedSprite.setPosition(tile.position);
                        gameWindow.draw(revealedSprite);
                    }
                    gameWindow.draw(happyFace);
                    gameWindow.draw(debugButton);
                    gameWindow.draw(pauseButton);
                    gameWindow.draw(leaderboardButton);
                    for (int i=0; i<3; i++) gameWindow.draw(counterDigits[i]);
                    for (int i=0; i<4; i++) gameWindow.draw(timerDigits[i]);
                    gameWindow.display();

                    showLeaderboard(font, windowWidth, windowHeight);

                    auto now = std::chrono::high_resolution_clock::now();
                    elapsedPausedTime += std::chrono::duration_cast<std::chrono::seconds>(now - pauseTime).count();
                    gamePaused = false;
                    pauseButton.setTexture(texm.get("pause"));
                }


                if (!gameOver && !gamePaused) {

                    // debug button
                    if (debugButton.getGlobalBounds().contains(mousePosF)) {
                        isDebugMode = !isDebugMode;
                    }

                    // tiles clickings
                    for (int i = 0; i < tiles.size(); i++) {
                        if (tiles[i].sprite.getGlobalBounds().contains(mousePosF)) {

                            // right click -> reveals
                            if (mouseButton->button == sf::Mouse::Button::Right) {
                                int flagChange = tiles[i].toggleFlag();
                                flagCount += flagChange;
                                updateCounter(minecount - flagCount, counterDigits, texm.get("digits"));
                            }

                            // left click -> reveals
                            if (mouseButton->button == sf::Mouse::Button::Left) {
                                if (!tiles[i].isFlagged) {
                                    revealTile(tiles, i, columns, rows);

                                    if (tiles[i].isMine) {
                                        gameOver = true;
                                        setGameLost(tiles, happyFace, texm);
                                    }

                                    else if (checkWin(tiles, minecount)) {
                                        gameOver = true;
                                        setGameWon(tiles, happyFace, texm, flagCount, minecount);
                                        updateCounter(0, counterDigits, texm.get("digits"));

                                        auto now = std::chrono::high_resolution_clock::now();
                                        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
                                        int totalSeconds = duration.count() - elapsedPausedTime;

                                        int newRank = updateLeaderboard(totalSeconds, playerName);

                                        gameWindow.clear(sf::Color::White);
                                        for (auto& tile : tiles) { tile.draw(gameWindow, texm, isDebugMode); }
                                        gameWindow.draw(happyFace);
                                        gameWindow.draw(debugButton);
                                        gameWindow.draw(pauseButton);
                                        gameWindow.draw(leaderboardButton);
                                        for (int i=0; i<3; i++) gameWindow.draw(counterDigits[i]);
                                        for (int i=0; i<4; i++) gameWindow.draw(timerDigits[i]);
                                        gameWindow.display();

                                        showLeaderboard(font, windowWidth, windowHeight, newRank);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        gameWindow.clear(sf::Color::White);

        if (gamePaused) {

            sf::Sprite revealedSprite(texm.get("revealed"));
            for (const auto& tile : tiles) {
                revealedSprite.setPosition(tile.position);
                gameWindow.draw(revealedSprite);
            }
        } else {
            // noraml tiles drawing
            for (auto& tile : tiles) {
                tile.draw(gameWindow, texm, isDebugMode);
            }
        }

        gameWindow.draw(happyFace);
        gameWindow.draw(debugButton);
        gameWindow.draw(pauseButton);
        gameWindow.draw(leaderboardButton);
        for (int i = 0; i < 3; i++) gameWindow.draw(counterDigits[i]);
        for (int i = 0; i < 4; i++) gameWindow.draw(timerDigits[i]);

        gameWindow.display();
    }
}