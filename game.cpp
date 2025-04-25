#include "raylib.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

// VectorUtils class for utility functions
class VectorUtils {
public:
    static Vector2 Subtract(Vector2 v1, Vector2 v2) {
        return {v1.x - v2.x, v1.y - v2.y};
    }

    static float Length(Vector2 v) {
        return sqrtf(v.x * v.x + v.y * v.y);
    }

    static Vector2 Scale(Vector2 v, float scale) {
        return {v.x * scale, v.y * scale};
    }

    static Vector2 Add(Vector2 v1, Vector2 v2) {
        return {v1.x + v2.x, v1.y + v2.y};
    }

    static Vector2 Normalize(Vector2 v) {
        float length = Length(v);
        if (length != 0) {
            return Scale(v, 1.0f / length);
        } else {
            return {0.0f, 0.0f};
        }
    }
};

// Object class as a base class for Wall and Coin
class Object {
public:
    virtual void draw() = 0;
    virtual ~Object() = default; // Virtual destructor
};

// Wall class inheriting from Object
class Wall : public Object {
public:
    Rectangle rect;

    Wall(Rectangle r) : rect(r) {}

    void draw() override {
        DrawRectangleRec(rect, GRAY);
    }
};

// Door class inheriting from Object
class Door : public Object {
public:
    Rectangle rect;
    bool isOpen;

    Door(Rectangle r) : rect(r), isOpen(false) {}

    void draw() override {  
        if (isOpen) {
            DrawRectangleRec(rect, BROWN);
        }
    }
};

// Character class as a base class for Robber and Cop
class Character {
public:
    Vector2 position;
    int radius;
    Color color;
    float speed;

    Character(Vector2 pos, int rad, Color col, float spd) : position(pos), radius(rad), color(col), speed(spd) {}

    virtual void draw() {
        DrawCircleV(position, radius, color);
    }

    virtual void move() = 0;
    virtual void move(Character*, const std::vector<Wall*>&) = 0;

    virtual ~Character() = default; // Virtual destructor
};

// Robber class inheriting from Character
class Robber : public Character {
public:
    Robber(Vector2 pos, int rad, Color col, float spd) : Character(pos, rad, col, spd) {}

    void move() override {
        if (IsKeyDown(KEY_W) && position.y - radius > 0) position.y -= speed;
        if (IsKeyDown(KEY_S) && position.y + radius < GetScreenHeight()) position.y += speed;
        if (IsKeyDown(KEY_A) && position.x - radius > 0) position.x -= speed;
        if (IsKeyDown(KEY_D) && position.x + radius < GetScreenWidth()) position.x += speed;
    }

    void move(Character* target, const std::vector<Wall*>& walls) override {}
};

// Cop class inheriting from Character
class Cop : public Character {
public:
    float rotation;

    Cop(Vector2 pos, int rad, Color col, float spd) : Character(pos, rad, col, spd), rotation(0.0f) {}

    void move() override {}

    void move(Character* target, const std::vector<Wall*>& walls) override {
        Vector2 direction = VectorUtils::Subtract(target->position, position);
        direction = VectorUtils::Normalize(direction);
        Vector2 nextPosition = VectorUtils::Add(position, VectorUtils::Scale(direction, speed));

        // Check for potential collisions and adjust direction
        for (const Wall* wall : walls) {
            if (CheckCollisionCircleRec(nextPosition, radius, wall->rect)) {
                Vector2 perpendicularDirection = { -direction.y, direction.x }; // Perpendicular direction
                Vector2 nextPosition1 = VectorUtils::Add(position, VectorUtils::Scale(perpendicularDirection, speed));
                Vector2 nextPosition2 = VectorUtils::Add(position, VectorUtils::Scale(perpendicularDirection, -speed));

                if (!CheckCollisionCircleRec(nextPosition1, radius, wall->rect)) {
                    nextPosition = nextPosition1;
                } else if (!CheckCollisionCircleRec(nextPosition2, radius, wall->rect)) {
                    nextPosition = nextPosition2;
                } else {
                    nextPosition = position; // Stay in place if both perpendicular directions are blocked
                }
                break;
            }
        }

        // Ensure cop stays within the screen boundaries
        if (nextPosition.x - radius < 0) nextPosition.x = radius;
        if (nextPosition.x + radius > GetScreenWidth()) nextPosition.x = GetScreenWidth() - radius;
        if (nextPosition.y - radius < 0) nextPosition.y = radius;
        if (nextPosition.y + radius > GetScreenHeight()) nextPosition.y = GetScreenHeight() - radius;

        position = nextPosition;

        // Calculate rotation angle
        rotation = atan2f(direction.y, direction.x) * (180.0f / PI);
    }

    void draw() override {
        DrawCircleV(position, radius, color);
        DrawLineEx(position, VectorUtils::Add(position, VectorUtils::Scale({cosf(rotation * (PI / 180.0f)), sinf(rotation * (PI / 180.0f))}, radius)), 2.0f, BLACK);
    }
};

// Coin class inheriting from Object
class Coin : public Object {
public:
    Vector2 position;
    bool collected;

    Coin(Vector2 pos) : position(pos), collected(false) {}

    void draw() override {
        if (!collected) {
            DrawCircleV(position, 10, GOLD);
        }
    }
};

// SlowingZone class inheriting from Object
class SlowingZone : public Object {
public:
    Rectangle rect;
    float slowEffect;

    SlowingZone(Rectangle r, float effect) : rect(r), slowEffect(effect) {}

    void draw() override {
        DrawRectangleRec(rect, Fade(GREEN, 0.5f));
    }

    bool isInside(Vector2 position) {
        return CheckCollisionPointRec(position, rect);
    }
};

// Game class to run the game
class Game {
public:
    const int screenWidth = 800;
    const int screenHeight = 600;
    const int playerRadius = 20;
    const int copRadius = 20;
    const int wallThickness = 20;
    const int targetFPS = 60;
    const int maxCoins = 5;

    Robber* robber;
    Cop* cop;
    Cop* cop2; // Second cop for level 3
    Door* door; // Door for level 3
    std::vector<Coin*> coins;
    std::vector<Wall*> walls;
    SlowingZone* slowingZone;
    int score;
    bool gameOver;
    bool robberEscaped;
    int level;

    Game() : score(0), gameOver(false), robberEscaped(false), level(1), cop2(nullptr), slowingZone(nullptr), door(nullptr) {
        InitWindow(screenWidth, screenHeight, "Cop and Robber Game");
        SetTargetFPS(targetFPS);
        srand(static_cast<unsigned>(time(0)));

        robber = new Robber({screenWidth / 2.0f, screenHeight / 2.0f}, playerRadius, BLUE, 4.5f);
        cop = new Cop({100.0f, 100.0f}, copRadius, RED, 3.0f);

        generateCoins();
        generateWalls();
    }

    ~Game() {
        delete robber;
        delete cop;
        delete cop2;
        delete slowingZone;
        delete door;
        for (Coin* coin : coins) delete coin;
        for (Wall* wall : walls) delete wall;
        CloseWindow();
    }

    void run() {
        while (!WindowShouldClose()) {
            update();
            draw();
        }
    }

private:
    void update() {
        if (!gameOver && !robberEscaped) {
            Vector2 oldPosition = robber->position;
            robber->move();

            for (Wall* wall : walls) {
                if (CheckCollisionCircleRec(robber->position, robber->radius, wall->rect)) {
                    robber->position = oldPosition;
                    break;
                }
            }

            if (slowingZone && slowingZone->isInside(robber->position)) {
                robber->speed = 3.375f; // 75% of normal speed
            } else {
                robber->speed = 4.5f;
            }

            cop->move(robber, walls);
            if (cop2) cop2->move(robber, walls);

            if (CheckCollisionCircles(robber->position, robber->radius, cop->position, cop->radius) ||
                (cop2 && CheckCollisionCircles(robber->position, robber->radius, cop2->position, cop2->radius))) {
                gameOver = true;
            }

            for (Coin* coin : coins) {
                if (!coin->collected && CheckCollisionCircles(robber->position, robber->radius, coin->position, 10)) {
                    coin->collected = true;
                    score++;
                }
            }

            if (score >= maxCoins) {
                advanceLevel();
            }

            if (door && door->isOpen && CheckCollisionCircleRec(robber->position, robber->radius, door->rect)) {
                robberEscaped = true;
            }
        } else {
            if (IsKeyPressed(KEY_R)) {
                resetGame();
            }
        }
    }

    void draw() {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (Wall* wall : walls) {
            wall->draw();
        }

        if (slowingZone) {
            slowingZone->draw();
        }

        for (Coin* coin : coins) {
            coin->draw();
        }

        if (door) {
            door->draw();
        }

        robber->draw();
        cop->draw();
        if (cop2) cop2->draw();

        DrawText(TextFormat("Score: %d", score), 10, 10, 20, BLACK);
        DrawText(TextFormat("Level: %d", level), 10, 40, 20, BLACK);

        if (gameOver) {
            DrawText("Game Over!", screenWidth / 2 - MeasureText("Game Over!", 40) / 2, screenHeight / 2 - 20, 40, RED);
            DrawText("Press 'R' to restart", screenWidth / 2 - MeasureText("Press 'R' to restart", 20) / 2, screenHeight / 2 + 30, 20, DARKGRAY);
        }

        if (robberEscaped) {
            ClearBackground(BLACK);
            robber->draw();
            DrawText("We have successfully robbed our neighbour! 😏", screenWidth / 2 - MeasureText("We have successfully robbed our neighbour! 😏", 20) / 2, screenHeight / 2, 20, GREEN);
        }

        EndDrawing();
    }

    void generateCoins() {
        coins.clear();
        for (int i = 0; i < maxCoins; i++) {
            Vector2 coinPosition;
            bool validPosition;
            do {
                validPosition = true;
                coinPosition = {static_cast<float>(rand() % (screenWidth - 2 * 10) + 10),
                                static_cast<float>(rand() % (screenHeight - 2 * 10) + 10)};
                for (const Wall* wall : walls) {
                    if (CheckCollisionPointRec(coinPosition, wall->rect)) {
                        validPosition = false;
                        break;
                    }
                }
            } while (!validPosition);
            coins.push_back(new Coin(coinPosition));
        }
    }

    void generateWalls() {
        walls.clear();
        walls.push_back(new Wall({150.0f, 150.0f, 200.0f, static_cast<float>(wallThickness)}));
        walls.push_back(new Wall({450.0f, 300.0f, static_cast<float>(wallThickness), 200.0f}));
        walls.push_back(new Wall({250.0f, 450.0f, 300.0f, static_cast<float>(wallThickness)}));
    }

    void generateSlowingZone() {
        delete slowingZone;
        float zoneWidth = screenWidth / 2.0f;
        float zoneHeight = screenHeight / 2.0f;
        slowingZone = new SlowingZone({static_cast<float>(rand() % (screenWidth - static_cast<int>(zoneWidth))),
                                       static_cast<float>(rand() % (screenHeight - static_cast<int>(zoneHeight))), 
                                       zoneWidth, zoneHeight}, 0.75f);
    }

    void generateDoor() {
        delete door;

        door = new Door({screenWidth / 2.0f - 40, screenHeight - 80.0f, 80.0f, 40.0f});
        door->isOpen = true;
    }

    void advanceLevel() {
        level++;
        score = 0;
        generateCoins();

        switch (level) {
            case 2:
                generateSlowingZone();
                break;
            case 3:
                cop2 = new Cop({screenWidth - 100.0f, screenHeight - 100.0f}, copRadius, PINK, 3.0f);
                generateDoor();
                break;
            default:
                gameOver = true;
                break;
        }
    }

    void resetGame() {
        score = 0;
        level = 1;
        gameOver = false;
        robberEscaped = false;
        delete cop2;
        cop2 = nullptr;
        delete slowingZone;
        slowingZone = nullptr;
        delete door;
        door = nullptr;
        delete cop;
        cop = new Cop({100.0f, 100.0f}, copRadius, RED, 3.0f);
        generateCoins();
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}
