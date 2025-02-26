/*******************************************************************************************
 * BlockLetter
*******************************************************************************************/

#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

#ifdef PLATFORM_WEB
    #include <emscripten/emscripten.h>
#endif

/*******************************************************************************************
*  DEFINES & CONSTANTS
*******************************************************************************************/

#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  600
#define BLOCK_SIZE     40
#define WALL_COUNT     3

// Color Definitions
#define BACKGROUND BLACK
#define CYAN          (Color){0, 255, 255, 255}
#define DARKGOLD      (Color){184, 134, 11, 255}
#define DARKORANGE    (Color){255, 140, 0, 255}
#define LIGHTGREEN    (Color){144, 238, 144, 255}
#define DARKRED       (Color){139, 0, 0, 255}
#define AQUA          (Color){0, 255, 255, 255}
#define TAN           (Color){210, 180, 140, 255}
#define PLUM          (Color){221, 160, 221, 255}
#define TEAL          (Color){0, 128, 128, 255}
#define SALMON        (Color){250, 128, 114, 255}

// Particle/Afterimage Limits
#define MAX_PARTICLES    1000
#define MAX_AFTERIMAGES  10

/*******************************************************************************************
*  GLOBAL VARIABLES
*******************************************************************************************/

// Game states
int    dashCount           = 2;  // Max dashes
bool   soundEnabled        = true;
bool   blackHoleActive     = false;
float  blackHoleEndTime    = 0.0f;
Vector2 blackHolePos       = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };

bool IsWindowFocused(void);
bool   gameOver            = false;
bool   paused              = false;
bool   inMainMenu          = true;  // Start in the main menu
int    score               = 0;
float  wallSpeed           = 100;

// Audio
Sound crashSound;
Sound blackHoleSound;
Sound blockDestroy;
Sound pauseSound;
Sound warpSound;
Sound scoreUpSound;
Sound startSound;
Sound comboSound;

// Screen shake
float  screenShake         = 0.0f; // Tracks screen shake intensity
bool   gameOverTriggered   = false; // Ensures effect plays only once
Vector2 playerCollisionPosition = { 0, 0 }; // Stores where the player collided
Vector2 shakeOffset        = { 0, 0 }; // Store shake movement offsets

/*******************************************************************************************
*  DATA STRUCTURES
*******************************************************************************************/

typedef struct {
    Rectangle rect;
    char letter;
    bool active;
    bool breakable;
    float fadeAlpha;
} Block;

typedef struct {
    Block blocks[SCREEN_HEIGHT / BLOCK_SIZE][5];
    float x;
    int thickness;
    bool active;
    bool scored;
} Wall;

typedef struct {
    Vector2 position;
    float alpha;
} Afterimage;

Afterimage afterimages[MAX_AFTERIMAGES];
int        afterimageIndex = 0;

// Particle system
typedef struct {
    Vector2 position;
    Vector2 velocity;
    float   lifetime;
    float   size;  // Random particle size
    Color   color;
} Particle;

Particle particles[MAX_PARTICLES];
int      particleIndex = 0;

/*******************************************************************************************
*  FUNCTION DECLARATIONS
*******************************************************************************************/

Color   GetBlockColor(char letter);
void    DrawDistortedGrid(float speed, int cellSize, Color gridColor, Vector2 blackHoleCenter);
void    ApplyScreenShake(float intensity);
void    UpdateScreenShake(float deltaTime);
Vector2 GetScreenShakeOffset();
Vector2 RotatePoint(Vector2 point, Vector2 origin, float angle);

void    SpawnParticles(Vector2 position, Color color);
void    UpdateAndDrawParticles(float deltaTime);

void    DrawMovingGrid(float speed, int cellSize, Color gridColor);
void    DrawRotatingBlock(Rectangle rect, Color color, float rotation, float scale);
void    DrawRotatedTriangleWithGlow(Vector2 center, float size, float rotation, Color color);

void    GenerateWall(Wall *wall, float x, int thickness, int score);
void    ResetGameState(Wall walls[], Vector2 *playerPosition, int *score, float *wallSpeed, bool *gameOver, bool *gameOverTriggered, int *dashCount);

/*******************************************************************************************
*  FUNCTION DEFINITIONS
*******************************************************************************************/

// Returns a color based on the letter of the block
Color GetBlockColor(char letter)
{
    switch (letter)
    {
        case '/': return Fade(WHITE, 0.8f); // Special color for the Black Hole block
        case 'A': return RED;
        case 'B': return ORANGE;
        case 'C': return GOLD;
        case 'D': return GREEN;
        case 'E': return SKYBLUE;
        case 'F': return BLUE;
        case 'G': return PURPLE;
        case 'H': return PINK;
        case 'I': return BEIGE;
        case 'J': return MAROON;
        case 'K': return DARKGREEN;
        case 'L': return DARKBLUE;
        case 'M': return DARKPURPLE;
        case 'N': return DARKBROWN;
        case 'O': return MAGENTA;
        case 'P': return LIME;
        case 'Q': return CYAN;
        case 'R': return YELLOW;
        case 'S': return GRAY;
        case 'T': return DARKGRAY;
        case 'U': return VIOLET;
        case 'V': return DARKGOLD;
        case 'W': return GRAY;
        case 'X': return PURPLE;
        case 'Y': return DARKORANGE;
        case 'Z': return LIGHTGREEN;
        case '0': return (Color){32, 32, 32, 255};
        case '1': return WHITE;
        case '2': return GOLD;
        case '3': return ORANGE;
        case '4': return DARKRED;
        case '5': return AQUA;
        case '6': return TAN;
        case '7': return PLUM;
        case '8': return TEAL;
        case '9': return SALMON;
        default:  return BLACK;
    }
}

// Draws a grid that distorts around a "black hole" center point
void DrawDistortedGrid(float speed, int cellSize, Color gridColor, Vector2 blackHoleCenter)
{
    float gridOffset = fmod(GetTime() * speed, cellSize);

    for (int x = 0; x < SCREEN_WIDTH; x += cellSize)
    {
        for (int y = 0; y < SCREEN_HEIGHT; y += cellSize)
        {
            float dx = blackHoleCenter.x - x;
            float dy = blackHoleCenter.y - y;
            float distance = sqrtf(dx * dx + dy * dy);

            // Apply distortion based on distance
            float distortion = sinf(distance * 0.05f + GetTime() * 2.0f) * 4.0f;
            x += distortion;
            y += distortion;

            DrawLine(x, 0, x, SCREEN_HEIGHT, Fade(gridColor, 0.15f));
            DrawLine(0, y, SCREEN_WIDTH, y, Fade(gridColor, 0.15f));
        }
    }
}

// Applies a screen shake by setting an intensity
void ApplyScreenShake(float intensity)
{
    screenShake = intensity;
}

// Updates the screen shake effect over time
void UpdateScreenShake(float deltaTime)
{
    if (screenShake > 0)
    {
        screenShake -= deltaTime * 2.0f; // Reduce shake over time
        if (screenShake < 0) screenShake = 0;
    }
}

// Returns a vector offset for applying screen shake
Vector2 GetScreenShakeOffset()
{
    return (screenShake > 0)
        ? (Vector2){ GetRandomValue(-2, 2) * screenShake, GetRandomValue(-2, 2) * screenShake }
        : (Vector2){ 0, 0 };
}

// Rotates a point around a given origin by a specified angle
Vector2 RotatePoint(Vector2 point, Vector2 origin, float angle)
{
    return (Vector2){
        origin.x + cos(angle) * (point.x - origin.x) - sin(angle) * (point.y - origin.y),
        origin.y + sin(angle) * (point.x - origin.x) + cos(angle) * (point.y - origin.y)
    };
}

// Spawns new particles at a given position with a given color
void SpawnParticles(Vector2 position, Color color)
{
    int numParticles = (gameOverTriggered) ? 30 : 10; // More particles on Game Over

    for (int i = 0; i < numParticles; i++)
    {
        int index = particleIndex % MAX_PARTICLES; // Circular buffer

        particles[index].position = position;

        // More dramatic explosion on Game Over
        float speed = GetRandomValue(30, (gameOverTriggered) ? 100 : 60) / 10.0f;
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        particles[index].velocity = (Vector2){ cos(angle) * speed, sin(angle) * speed };

        particles[index].lifetime = (gameOverTriggered) ? 1.5f : 0.8f; // Longer duration on death
        particles[index].size     = GetRandomValue(3, (gameOverTriggered) ? 7 : 5);
        particles[index].color    = color;

        particleIndex++;
    }
}

// Updates particles' positions and draws them
void UpdateAndDrawParticles(float deltaTime)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].lifetime > 0)
        {
            // Apply velocity
            particles[i].position.x += particles[i].velocity.x * deltaTime * 60;
            particles[i].position.y += particles[i].velocity.y * deltaTime * 60;

            // Apply slight gravity
            particles[i].velocity.y += 0.05f;

            // Reduce lifetime
            particles[i].lifetime -= deltaTime;

            // Render with size variation
            DrawCircleV(particles[i].position, particles[i].size, Fade(particles[i].color, particles[i].lifetime));
        }
    }
}

// Draws a moving grid that scrolls horizontally/vertically
void DrawMovingGrid(float speed, int cellSize, Color gridColor)
{
    float gridOffset = fmod(GetTime() * speed, cellSize);

    // Soft glow layer (draws slightly faded lines behind the grid)
    for (int i = -2; i <= 2; i += 2)
    {
        for (int j = -2; j <= 2; j += 2)
        {
            for (int x = -gridOffset; x < SCREEN_WIDTH; x += cellSize)
            {
                DrawLine(x + i, 0, x + i, SCREEN_HEIGHT, Fade(gridColor, 0.05f));
            }
            for (int y = -gridOffset; y < SCREEN_HEIGHT; y += cellSize)
            {
                DrawLine(0, y + j, SCREEN_WIDTH, y + j, Fade(gridColor, 0.05f));
            }
        }
    }

    // Main grid
    for (int x = -gridOffset; x < SCREEN_WIDTH; x += cellSize)
    {
        DrawLine(x, 0, x, SCREEN_HEIGHT, Fade(gridColor, 0.15f));
    }
    for (int y = -gridOffset; y < SCREEN_HEIGHT; y += cellSize)
    {
        DrawLine(0, y, SCREEN_WIDTH, y, Fade(gridColor, 0.15f));
    }
}

// Draws a rotating rectangle (used for block destruction animation)
void DrawRotatingBlock(Rectangle rect, Color color, float rotation, float scale)
{
    Vector2 center = { rect.x + rect.width / 2, rect.y + rect.height / 2 };

    // Adjust the rectangle size based on the shrinking effect
    Rectangle scaledRect = {
        center.x,
        center.y,
        rect.width * scale,
        rect.height * scale
    };

    DrawRectanglePro(scaledRect, (Vector2){scaledRect.width / 2, scaledRect.height / 2}, rotation, color);
}

// Draws a triangle (player ship) with a glow effect and rotation
void DrawRotatedTriangleWithGlow(Vector2 center, float size, float rotation, Color color)
{
    float radians = rotation * DEG2RAD;

    // Define original triangle points
    Vector2 top    = { center.x + size * 1.5f, center.y };
    Vector2 left   = { center.x, center.y - size / 2 };
    Vector2 right  = { center.x, center.y + size / 2 };

    // Rotate all points
    Vector2 rotatedTop   = RotatePoint(top,   center, radians);
    Vector2 rotatedLeft  = RotatePoint(left,  center, radians);
    Vector2 rotatedRight = RotatePoint(right, center, radians);

    // Draw glow effect with slightly larger, transparent outlines
    for (float glowSize = 1.0f; glowSize <= 3.0f; glowSize += 1.0f)
    {
        Vector2 glowTop   = RotatePoint((Vector2){ top.x + glowSize,   top.y + glowSize },   center, radians);
        Vector2 glowLeft  = RotatePoint((Vector2){ left.x - glowSize,  left.y - glowSize },  center, radians);
        Vector2 glowRight = RotatePoint((Vector2){ right.x - glowSize, right.y + glowSize }, center, radians);

        DrawTriangleLines(glowTop, glowLeft, glowRight, Fade(color, 0.2f));
    }

    // Draw the actual rotated player triangle
    DrawTriangleLines(rotatedTop, rotatedLeft, rotatedRight, color);
}

// Generates a wall of blocks at a given x position with certain thickness
void GenerateWall(Wall *wall, float x, int thickness, int score)
{
    wall->x         = x;
    wall->thickness = thickness;
    wall->active    = true;
    wall->scored    = false;

    for (int row = 0; row < SCREEN_HEIGHT / BLOCK_SIZE; row++)
    {
        bool hasBreakableBlock = false;

        for (int col = 0; col < thickness; col++)
        {
            Block *block  = &wall->blocks[row][col];
            block->rect   = (Rectangle){ x + col * BLOCK_SIZE, row * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE };
            block->breakable = (GetRandomValue(1, 100) <= 80);

            if (block->breakable)
            {
                if (GetRandomValue(1, 100) <= 5)
                {
                    block->letter = '0' + GetRandomValue(0, 9);
                }
                else if (GetRandomValue(1, 200) <= 1)
                {
                    block->letter = '/';
                }
                else
                {
                    // Avoid W or S if possible
                    do {
                        block->letter = 'A' + GetRandomValue(0, 25);
                    } while (block->letter == 'W' || block->letter == 'S');
                }
                hasBreakableBlock = true;
            }
            else
            {
                block->letter = '\0';
            }

            block->active    = true;
            block->fadeAlpha = 0.0f;
        }

        // Ensure at least one breakable block in each row
        if (!hasBreakableBlock)
        {
            int col = GetRandomValue(0, thickness - 1);
            Block *block = &wall->blocks[row][col];
            block->breakable = true;

            if (GetRandomValue(1, 100) <= 5)
            {
                block->letter = '0' + GetRandomValue(0, 9);
            }
            else
            {
                // Avoid W or S if possible
                do {
                    block->letter = 'A' + GetRandomValue(0, 25);
                } while (block->letter == 'W' || block->letter == 'S');
            }
            block->active = true;
        }
    }
}

// Resets the game state for a new round
void ResetGameState(Wall walls[], Vector2 *playerPosition, int *score, float *wallSpeed, bool *gameOver, bool *gameOverTriggered, int *dashCount)
{
    StopSound(crashSound);

    // Reset player
    playerPosition->y = SCREEN_HEIGHT / 2;
    playerPosition->x = 50;

    // Reset other parameters
    *dashCount        = 2;
    *score            = 0;
    *wallSpeed        = 200;
    *gameOver         = false;
    *gameOverTriggered= false;

    // Reset screen shake effects
    screenShake = 0.0f;
    shakeOffset = (Vector2){0, 0};

    // Reset walls
    for (int i = 0; i < WALL_COUNT; i++)
    {
        GenerateWall(&walls[i], SCREEN_WIDTH + i * 300, 2, *score);
        walls[i].scored = false;
    }

    PlaySound(startSound);
}

/*******************************************************************************************
*  MAIN FUNCTION
*******************************************************************************************/

int main()
{
    // Initialization
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "BlockLetter");
    SetExitKey(0);
    SetTargetFPS(60);

    // Load custom font
    Font font = LoadFont("assets/vcr.ttf");

    // Audio device init
    InitAudioDevice();
    crashSound     = LoadSound("assets/crash.wav");
    blackHoleSound = LoadSound("assets/blackhole.wav");
    blockDestroy   = LoadSound("assets/blockDestroy.wav");
    pauseSound     = LoadSound("assets/pause.wav");
    warpSound      = LoadSound("assets/warp.wav");
    scoreUpSound   = LoadSound("assets/scoreup.wav");
    startSound     = LoadSound("assets/start.wav");
    comboSound     = LoadSound("assets/combo.wav");

    // Player initialization
    Vector2 playerPosition = { 50, SCREEN_HEIGHT / 2 }; // Triangle center position
    float   playerSpeedY   = 200;
    float   playerSize     = 20.0f; // Size of the spaceship triangle

    // Wall initialization
    Wall walls[WALL_COUNT];
    for (int i = 0; i < WALL_COUNT; i++)
    {
        GenerateWall(&walls[i], SCREEN_WIDTH + i * 300, 2, score);
    }

    // Game Loop
    while (!WindowShouldClose())
    {
        float deltaTime = GetFrameTime();
        UpdateAndDrawParticles(deltaTime);

        // MAIN MENU
        if (inMainMenu)
        {
            // Check for spacebar to start the game
            if (IsKeyPressed(KEY_SPACE))
            {
                inMainMenu = false;
                PlaySound(startSound);
            }
            // Toggle sound
            if (IsKeyPressed(KEY_M))
            {
                soundEnabled = !soundEnabled;
                if (!soundEnabled) SetMasterVolume(0.0f); // Mute all audio
                else               SetMasterVolume(1.0f); // Restore audio
            }

            char soundText[100];
            sprintf(soundText, "Sound: %s (Press M)", soundEnabled ? "ON" : "OFF");

            BeginDrawing();
            ClearBackground(BACKGROUND);

            // Moving Grid BG
            DrawMovingGrid(10.0f, 40, DARKGREEN);

            // Centered title and instructions
            Vector2 titleSize       = MeasureTextEx(font, "0xDEAD//TYPE", 40, 1);
            Vector2 instructionSize = MeasureTextEx(font, "Press SPACE to Start", 20, 1);
            Vector2 controlsSize    = MeasureTextEx(font, "Controls:", 25, 1);
            Vector2 moveSize       = MeasureTextEx(font, "Move: W/S or Up/Down", 18, 1);
            Vector2 dashSize       = MeasureTextEx(font, "Dash: SPACE", 18, 1);
            Vector2 pauseSize      = MeasureTextEx(font, "Pause: ESC", 18, 1);
            Vector2 soundTextSize  = MeasureTextEx(font, soundText, 20, 1);

            DrawTextEx(font, "0xDEAD//TYPE",
                    (Vector2){(SCREEN_WIDTH - titleSize.x) / 2, (SCREEN_HEIGHT - titleSize.y) / 2 - 50},
                    40, 1, WHITE);

            DrawTextEx(font, "Press SPACE to Start",
                    (Vector2){(SCREEN_WIDTH - instructionSize.x) / 2, (SCREEN_HEIGHT - instructionSize.y) / 2 + 20},
                    20, 1, GRAY);

            // Controls Section
            float controlsStartY = (SCREEN_HEIGHT - instructionSize.y) / 2 + 70;

            // Sound toggle text
            DrawTextEx(font, soundText,
                    (Vector2){(SCREEN_WIDTH - instructionSize.x) / 2, (SCREEN_HEIGHT - instructionSize.y) - 10 },
                    20, 1, GRAY);

            DrawTextEx(font, "Controls:", 
                    (Vector2){(SCREEN_WIDTH - controlsSize.x) / 2, controlsStartY}, 
                    25, 1, LIGHTGRAY);

            DrawTextEx(font, "Move: W/S or Up/Down", 
                    (Vector2){(SCREEN_WIDTH - moveSize.x) / 2, controlsStartY + 40}, 
                    18, 1, GRAY);

            DrawTextEx(font, "Dash: SPACE", 
                    (Vector2){(SCREEN_WIDTH - dashSize.x) / 2, controlsStartY + 65}, 
                    18, 1, GRAY);

            DrawTextEx(font, "Pause: ESC", 
                    (Vector2){(SCREEN_WIDTH - pauseSize.x) / 2, controlsStartY + 90}, 
                    18, 1, GRAY);

/*            DrawTextEx(font, "Quit: Q", 
                    (Vector2){(SCREEN_WIDTH - quitSize.x) / 2, controlsStartY + 115}, 
                    18, 1, GRAY); */

            EndDrawing();
            continue;
        }

        // GAME CONTROLS: dash, pause, etc.
        if (IsKeyPressed(KEY_SPACE) && dashCount > 0 && !paused && !gameOver)
        {
            PlaySound(warpSound);
            playerPosition.x += 100;  // Dash forward
            dashCount--;

            // Store afterimage
            afterimages[afterimageIndex] = (Afterimage){ playerPosition, 1.0f };
            afterimageIndex = (afterimageIndex + 1) % MAX_AFTERIMAGES;
        }

        // Toggle pause with ESC
        if (IsKeyPressed(KEY_ESCAPE) && !gameOver)
        {
            paused = !paused;
            PlaySound(pauseSound);
        }

        if (!IsWindowFocused()) {
            paused = true;
        }

        // UPDATE & DRAW (if not paused)
        if (!paused && !gameOver)
        {
            // Player movement
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   playerPosition.y -= playerSpeedY * deltaTime;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) playerPosition.y += playerSpeedY * deltaTime;

            // Black Hole active
            if (blackHoleActive)
            {
                ApplyScreenShake(2.0f);
                DrawDistortedGrid(5.0f, 50, DARKGREEN, blackHolePos);

                // Pull blocks in
                for (int i = 0; i < WALL_COUNT; i++)
                {
                    for (int row = 0; row < SCREEN_HEIGHT / BLOCK_SIZE; row++)
                    {
                        for (int col = 0; col < walls[i].thickness; col++)
                        {
                            Block *block = &walls[i].blocks[row][col];
                            if (!block->active) continue;

                            Vector2 dir = { blackHolePos.x - block->rect.x, blackHolePos.y - block->rect.y };
                            float distance = sqrtf(dir.x * dir.x + dir.y * dir.y);
                            if (distance < 10)
                            {
                                block->active = false;
                                continue;
                            }

                            // Normalize direction
                            dir.x /= distance;
                            dir.y /= distance;

                            // Swirling motion
                            float angle = GetTime() * 5.0f;
                            Vector2 swirl = { cos(angle) * 5.0f, sin(angle) * 5.0f };

                            // Move block
                            block->rect.x += (dir.x * 3.0f + swirl.x) * 2.0f;
                            block->rect.y += (dir.y * 3.0f + swirl.y) * 2.0f;
                        }
                    }
                }

                // Turn off Black Hole Mode after 1 second
                if (GetTime() > blackHoleEndTime)
                {
                    blackHoleActive = false;
                }
            }

            // Keep player in screen bounds
            if (playerPosition.y < playerSize)                      playerPosition.y = playerSize;
            if (playerPosition.y > SCREEN_HEIGHT - playerSize)       playerPosition.y = SCREEN_HEIGHT - playerSize;

            // Adjust wall speed/thickness by score
            wallSpeed = 100 + score * 2;
            int newThickness = 2 + score / 10;
            if (newThickness > 5) newThickness = 5;

            // Wall movement & collision
            for (int i = 0; i < WALL_COUNT; i++)
            {
                walls[i].x -= wallSpeed * deltaTime;

                if (walls[i].x + walls[i].thickness * BLOCK_SIZE < 0)
                {
                    GenerateWall(&walls[i], SCREEN_WIDTH, newThickness, score);
                    walls[i].scored = false;
                }

                if (walls[i].active)
                {
                    for (int row = 0; row < SCREEN_HEIGHT / BLOCK_SIZE; row++)
                    {
                        for (int col = 0; col < walls[i].thickness; col++)
                        {
                            Block *block = &walls[i].blocks[row][col];
                            block->rect.x = walls[i].x + col * BLOCK_SIZE;

                            // Collision
                            if (block->active)
                            {
                                if (CheckCollisionPointRec(playerPosition, block->rect))
                                {
                                    if (!gameOverTriggered)
                                    {
                                        gameOverTriggered = true;
                                        playerCollisionPosition = playerPosition;
                                        PlaySound(crashSound);
                                        ApplyScreenShake(2.0f); // Stronger shake
                                                                // Explosion particles
                                        SpawnParticles(playerCollisionPosition, RED);
                                    }
                                    gameOver = true;
                                }

                                // Break block if correct key pressed
                                if (block->breakable && IsKeyPressed((int)block->letter))
                                {
                                    block->fadeAlpha = 1.0f; // Start fading
                                    block->active    = false;

                                    if (block->letter == '/')
                                    {
                                        blackHoleActive   = true;
                                        blackHoleEndTime  = GetTime() + 1.0f;
                                        PlaySound(blackHoleSound);
                                    }
                                    SpawnParticles(
                                            (Vector2){
                                            block->rect.x + BLOCK_SIZE / 2,
                                            block->rect.y + BLOCK_SIZE / 2
                                            },
                                            GetBlockColor(block->letter)
                                            );
                                    PlaySound(blockDestroy);
                                }
                            }

                            // Fade and shrink destroyed blocks
                            if (!block->active && block->fadeAlpha > 0.0f)
                            {
                                block->fadeAlpha -= GetFrameTime() * 2.0f; // Fade speed
                                float shrinkScale = block->fadeAlpha;
                                float rotation    = (1.0f - block->fadeAlpha) * 360;

                                DrawRotatingBlock(block->rect, Fade(GetBlockColor(block->letter), block->fadeAlpha), rotation, shrinkScale);
                            }

                            // Draw active blocks
                            if (block->fadeAlpha > 0.0f)
                            {
                                DrawRectangleRec(block->rect, Fade(GetBlockColor(block->letter), block->fadeAlpha));
                            }
                        }
                    }
                }

                // Score increment if player passes wall
                if (!walls[i].scored && walls[i].x + walls[i].thickness * BLOCK_SIZE < playerPosition.x)
                {
                    if ((score + 1) % 5 == 0) {
                        PlaySound(comboSound);
                    } else {
                    PlaySound(scoreUpSound);
                    }
                    score++;
                    walls[i].scored = true;
                }
            }
        }

        // DRAW
        UpdateScreenShake(deltaTime);
        shakeOffset = GetScreenShakeOffset();

        BeginDrawing();
        Vector2 playerShakenPos = { playerPosition.x + shakeOffset.x, playerPosition.y + shakeOffset.y };

        if (!paused)
        {
            ClearBackground(BACKGROUND);
            DrawMovingGrid(10.0f, 40, DARKGREEN);

            // Player (if not gameOver)
            if (!gameOver)
            {
                // Rotation angle
                float rotationAngle = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   ? -15 :
                    (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ?  15 : 0;

                // Afterimages
                for (int i = 0; i < MAX_AFTERIMAGES; i++)
                {
                    if (afterimages[i].alpha > 0.0f)
                    {
                        DrawRotatedTriangleWithGlow(afterimages[i].position, playerSize, rotationAngle, Fade(GREEN, afterimages[i].alpha));
                        afterimages[i].alpha -= GetFrameTime();
                    }
                }

                // Current player ship
                DrawRotatedTriangleWithGlow(playerShakenPos, playerSize, rotationAngle, GREEN);
            }

            // Draw walls & blocks
            for (int i = 0; i < WALL_COUNT; i++)
            {
                if (walls[i].active)
                {
                    for (int row = 0; row < SCREEN_HEIGHT / BLOCK_SIZE; row++)
                    {
                        for (int col = 0; col < walls[i].thickness; col++)
                        {
                            Block *block = &walls[i].blocks[row][col];
                            if (block->active)
                            {
                                Color blockColor = block->breakable ? GetBlockColor(block->letter) : BLACK;

                                // Special rainbow color for black hole block
                                if (block->letter == '/')
                                {
                                    float hue = fmod(GetTime() * 400, 360); // Cycle 0-360
                                    Color rainbowColor = ColorFromHSV(hue, 0.9f, 0.9f);
                                    DrawRectangleRec(block->rect, rainbowColor);
                                }
                                else
                                {
                                    DrawRectangleRec(block->rect, blockColor);
                                }

                                if (block->breakable)
                                {
                                    Rectangle shakenRect = block->rect;
                                    shakenRect.x += shakeOffset.x;
                                    shakenRect.y += shakeOffset.y;
                                    DrawRectangleRec(shakenRect, GetBlockColor(block->letter));

                                    char letter[2] = { block->letter, '\0' };
                                    Vector2 textSize = MeasureTextEx(font, letter, 20, 1);
                                    float textX = block->rect.x + (block->rect.width  - textSize.x) / 2;
                                    float textY = block->rect.y + (block->rect.height - textSize.y) / 2;
                                    Color textColor = ((blockColor.r + blockColor.g + blockColor.b) > 400) ? BLACK : WHITE;
                                    DrawTextEx(font, letter, (Vector2){ textX + 1, textY + 1 }, 22, 1, Fade(BLACK, 0.5f));
                                    DrawTextEx(font, letter, (Vector2){ textX, textY }, 22, 1, textColor);
                                }
                                else
                                {
                                    // Outline for unbreakable blocks
                                    DrawRectangleLinesEx(block->rect, 0.5f, GREEN);
                                }
                            }
                        }
                    }
                }
            }

            // Dash Indicator (bottom-left)
            for (int i = 0; i < 2; i++)
            {
                Color dashColor = (i < dashCount) ? Fade(WHITE, 0.5f) : Fade(DARKGRAY, 0.3f);
                DrawCircle(30 + (i * 20), SCREEN_HEIGHT - 30, 6, dashColor);
            }

            // Persistent Personal Best
            static int pbScore = 0;
            if (score > pbScore) pbScore = score;

            // Score text
            char scoreText[1000];
            sprintf(scoreText, "Score: %d", score);

            // Measure text size
            Vector2 scoreTextSize = MeasureTextEx(font, scoreText, 20, 1);
            float padding = 10.0f;
            Rectangle scoreBackground = {
                10, 10,
                scoreTextSize.x + padding * 2,
                scoreTextSize.y + padding * 2
            };

            // Translucent background
            DrawRectangleRec(scoreBackground, Fade(LIGHTGRAY, 0.3f));
            DrawTextEx(font, scoreText, (Vector2){ scoreBackground.x + padding, scoreBackground.y + padding }, 20, 1, WHITE);

            // PB Score (top-right)
            if (pbScore > 0)
            {
                char pbScoreText[1000];
                sprintf(pbScoreText, "PB: %d", pbScore);
                Vector2 pbScoreTextSize = MeasureTextEx(font, pbScoreText, 20, 1);

                Rectangle pbScoreBackground = {
                    SCREEN_WIDTH - 110, 10,
                    100, 40
                };
                DrawRectangleRec(pbScoreBackground, Fade(GOLD, 0.5f));
                DrawTextEx(font, pbScoreText,
                        (Vector2){
                        pbScoreBackground.x + (pbScoreBackground.width  - pbScoreTextSize.x) / 2,
                        pbScoreBackground.y + (pbScoreBackground.height - pbScoreTextSize.y) / 2
                        },
                        20, 1, BLACK);
            }

            // GAME OVER SCREEN
            if (gameOver)
            {
                Vector2 gameOverTextSize     = MeasureTextEx(font, "GAME OVER", 40, 1);
                Vector2 restartTextSize      = MeasureTextEx(font, "Press R to Restart", 20, 1);
                Vector2 scoreGameOverTextSize= MeasureTextEx(font, scoreText, 30, 1);

                float boxWidth  = fmaxf(fmaxf(gameOverTextSize.x, scoreGameOverTextSize.x), restartTextSize.x) + 40;
                float boxHeight = gameOverTextSize.y + scoreGameOverTextSize.y + restartTextSize.y + 70;
                Vector2 boxPos  = {
                    (SCREEN_WIDTH  - boxWidth)  / 2,
                    (SCREEN_HEIGHT - boxHeight) / 2
                };

                // Background box
                DrawRectangleRec((Rectangle){ boxPos.x, boxPos.y, boxWidth, boxHeight }, BLACK);

                Vector2 gameOverPos        = { boxPos.x + (boxWidth - gameOverTextSize.x) / 2, boxPos.y + 10 };
                Vector2 scoreGameOverPos   = { boxPos.x + (boxWidth - scoreGameOverTextSize.x) / 2, gameOverPos.y + gameOverTextSize.y + 10 };
                Vector2 restartPos         = { boxPos.x + (boxWidth - restartTextSize.x) / 2, scoreGameOverPos.y + scoreGameOverTextSize.y + 10 };

                DrawTextEx(font, "GAME OVER", gameOverPos, 40, 1, RED);
                DrawTextEx(font, scoreText, scoreGameOverPos, 30, 1, YELLOW);
                DrawTextEx(font, "Press R to Restart", restartPos, 20, 1, GRAY);

                // "Press Q to Exit"
                Vector2 exitTextSize = MeasureTextEx(font, "Press Q to Exit", 20, 1);
                Vector2 exitTextPos  = {
                    restartPos.x + (restartTextSize.x - exitTextSize.x) / 2,
                    restartPos.y + restartTextSize.y + 10
                };
                DrawTextEx(font, "Press Q to Exit", exitTextPos, 20, 1, GRAY);

                // Restart
                if (IsKeyPressed(KEY_R))
                {
                    ResetGameState(walls, &playerPosition, &score, &wallSpeed, &gameOver, &gameOverTriggered, &dashCount);
                    paused = false;
                }

                // Return to main menu
                if (IsKeyPressed(KEY_Q))
                {
                    ResetGameState(walls, &playerPosition, &score, &wallSpeed, &gameOver, &gameOverTriggered, &dashCount);
                    inMainMenu = true;
                    gameOver   = false;
                }
            }
        }
        else
        {
            // PAUSED SCREEN
            ClearBackground(BACKGROUND);
            DrawMovingGrid(10.0f, 40, DARKGREEN);

            // Center screen calculations
            float centerX = SCREEN_WIDTH / 2;
            float centerY = SCREEN_HEIGHT / 2;

            // Sound toggle
            if (IsKeyPressed(KEY_M)) {
                soundEnabled = !soundEnabled;
                SetMasterVolume(soundEnabled ? 1.0f : 0.0f);
            }

            char soundText[100];
            sprintf(soundText, "Sound: %s (Press M)", soundEnabled ? "ON" : "OFF");

            // Precompute text sizes
            Vector2 pausedTextSize   = MeasureTextEx(font, "PAUSED", 30, 1);
            Vector2 resumeTextSize   = MeasureTextEx(font, "Press ESC to Resume", 20, 1);
            Vector2 restartTextSize  = MeasureTextEx(font, "Press R to Restart", 20, 1);
            Vector2 soundTextSize    = MeasureTextEx(font, soundText, 20, 1);
            Vector2 quitTextSize     = MeasureTextEx(font, "Press Q to Quit to Menu", 20, 1);

            // Draw text elements (centered)
            DrawTextEx(font, "PAUSED", 
                    (Vector2){ centerX - pausedTextSize.x / 2, centerY - 80 }, 
                    30, 1, WHITE);

            DrawTextEx(font, "Press ESC to Resume", 
                    (Vector2){ centerX - resumeTextSize.x / 2, centerY - 30 }, 
                    20, 1, GRAY);

            DrawTextEx(font, "Press R to Restart", 
                    (Vector2){ centerX - restartTextSize.x / 2, centerY }, 
                    20, 1, GRAY);

            DrawTextEx(font, soundText, 
                    (Vector2){(SCREEN_WIDTH - restartTextSize.x) / 2, (SCREEN_HEIGHT - restartTextSize.y) - 10 },
                    20, 1, GRAY);

            DrawTextEx(font, "Press Q to Quit to Menu", 
                    (Vector2){ centerX - quitTextSize.x / 2, centerY + 60 }, 
                    20, 1, GRAY);

            // Handle input
            if (IsKeyPressed(KEY_R)) {
                ResetGameState(walls, &playerPosition, &score, &wallSpeed, &gameOver, &gameOverTriggered, &dashCount);
                paused = false;
            }

            if (IsKeyPressed(KEY_Q)) {
                ResetGameState(walls, &playerPosition, &score, &wallSpeed, &gameOver, &gameOverTriggered, &dashCount);
                inMainMenu = true;
                paused = false;
            }
        }

        EndDrawing();
    }

    // Cleanup
    UnloadFont(font);
    UnloadSound(crashSound);
    UnloadSound(blackHoleSound);
    UnloadSound(blockDestroy);
    UnloadSound(pauseSound);
    UnloadSound(warpSound);
    UnloadSound(scoreUpSound);
    UnloadSound(startSound);
    UnloadSound(comboSound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
