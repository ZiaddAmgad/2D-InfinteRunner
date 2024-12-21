#include <stdlib.h>
#include <cstdlib>
#include <glut.h>
#include <corecrt_math.h>
#include <stdio.h>
#include <al.h>
#include <alc.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>

// Global variables for player position, health, score, etc.
float playerY = 0.0f;  // Player's vertical position
float jumpTargetY = 0.2f;     // Target Y position for jump (1 step up)
float duckTargetY = -0.05f;    // Target Y position for duck (1 step down)
bool isDucking = false;       // Track if the player is ducking
bool isJumping = false;       // Track if the player is jumping
int health = 5;
int score = 0;
float obstacleX = 1.0f;  // Obstacle's horizontal position
float obstacleY = 0.0f;  // Ground obstacle
float jumpStartTime = 0.0f;   // Start time for jump animation
float lastCollisionTime = 0.0f;  // Time of last collision
float collisionCooldown = 1.0f;  // 1 second cooldown after a collision

// Global variables for obstacles
float groundObstacleX = 1.0f;  // First obstacle (on the ground)
float aboveObstacleX = 2.5f;   // Second obstacle (above player)

// Time control for jump (1.2 seconds)
const float jumpDuration = 1.2f;

bool isGameOver = false;      // Track whether the game has ended
bool isGameEnd = false;

// Global variables for collectibles
float collectibleX = 1.0f;  // Collectible's horizontal position
float collectibleY = 0.15f; // Collectible's vertical position (can vary)

// Global variable to keep track of rotation angle
float rotationAngle = 0.0f;

// Global variables for timer
int totalTime = 60;  // 60 seconds for the game duration
float elapsedTime = 0.0f;  // Time passed since the game started

// Speed control
float baseSpeed = 0.01f;  // Base speed of obstacles and collectibles
float speedMultiplier = 1.0f;  // This will increase as time progresses

// Global variables for background animation (circles)
const int numCircles = 5;  // Number of background circles
float circleX[numCircles] = { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f };  // Initial X positions of circles
float circleY[numCircles] = { 0.7f, -0.6f, 0.5f, -0.5f, 0.8f };  // Y positions (fixed)
float circleSpeed[numCircles] = { 0.002f, 0.0015f, 0.0018f, 0.0013f, 0.0021f };  // Different speeds for each circle

// Global variables for Power-Up 1 logic
float powerUp1X = 1.0f;
bool powerUp1Spawned = false;
bool powerUp1Active = false;

// Global variables for Power-Up 2 logic
float powerUp2X = 1.0f;
bool powerUp2Spawned = false;
bool powerUp2Active = false;
float oldSpeedMultiplier;


bool groundCollision = false;
bool aboveCollision = false;


// Function to initialize OpenAL
ALCdevice* device;
ALCcontext* context;
ALuint buffer, source;
ALuint bufferBackground, bufferYouDied, bufferYouWin, bufferCollision;
ALuint sourceBackground, sourceYouDied, sourceYouWin, sourceCollision;

void initOpenAL() {
    device = alcOpenDevice(NULL); // Open default device
    std::cout << device << std::endl;
    if (!device) {
        std::cerr << "Failed to open audio device." << std::endl;
        return;
    }

    context = alcCreateContext(device, NULL);
    if (!alcMakeContextCurrent(context)) {
        std::cerr << "Failed to set audio context." << std::endl;
        return;
    }

    // Set up listener properties
    ALfloat listenerPos[] = { 0.0f, 0.0f, 0.0f }; // Listener position
    ALfloat listenerVel[] = { 0.0f, 0.0f, 0.0f }; // Listener velocity
    ALfloat listenerOri[] = { 0.0f, 0.0f, -1.0f,  // Orientation: looking down -Z axis
                              0.0f, 1.0f, 0.0f }; // Up vector: +Y axis

    alListenerfv(AL_POSITION, listenerPos);
    alListenerfv(AL_VELOCITY, listenerVel);
    alListenerfv(AL_ORIENTATION, listenerOri);

    // Generate buffer and source
    // Generate buffer and source for background music
    alGenBuffers(1, &bufferBackground);
    alGenSources(1, &sourceBackground);

    // Generate buffer and source for "You Died" sound
    alGenBuffers(1, &bufferYouDied);
    alGenSources(1, &sourceYouDied);

    // Generate buffer and source for "You Win" sound
    alGenBuffers(1, &bufferYouWin);
    alGenSources(1, &sourceYouWin);

    // Generate buffer and source for obstacle collision sound
    alGenBuffers(1, &bufferCollision);
    alGenSources(1, &sourceCollision);

    alSourcef(source, AL_GAIN, 1.0f); // Set gain to normal volume

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "OpenAL Error: " << error << std::endl;
    }
}


// Function to load a .WAV file into OpenAL
struct WAVHeader {
    char riff[4];                // "RIFF"
    int chunkSize;                // File size minus 8 bytes
    char wave[4];                 // "WAVE"
    char fmt[4];                  // "fmt "
    int subChunk1Size;            // Size of the fmt chunk (16 for PCM)
    short audioFormat;            // Audio format (1 for PCM)
    short numChannels;            // Number of channels
    int sampleRate;               // Sample rate (e.g., 44100 Hz)
    int byteRate;                 // Byte rate (SampleRate * NumChannels * BitsPerSample / 8)
    short blockAlign;             // Block alignment
    short bitsPerSample;          // Bits per sample (e.g., 16)
    char data[4];                 // "data"
    int dataSize;                 // Size of the data chunk
};

// Function to load the WAV file and upload to OpenAL
void loadWAVFile(const char* filename, ALuint& buffer) {
    // Open the WAV file
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open WAV file: " << filename << std::endl;
        return;
    }

    // Read the WAV header
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Check if the file is in valid WAV format
    if (header.riff[0] != 'R' || header.riff[1] != 'I' || header.riff[2] != 'F' || header.riff[3] != 'F') {
        std::cerr << "Invalid WAV file format for: " << filename << std::endl;
        file.close();
        return;
    }

    // Read audio data
    char* data = new char[header.dataSize];
    file.read(data, header.dataSize);
    file.close();

    // Determine the audio format (Mono or Stereo)
    ALenum format;
    if (header.numChannels == 1) {
        format = (header.bitsPerSample == 16) ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    }
    else if (header.numChannels == 2) {
        format = (header.bitsPerSample == 16) ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
    }
    else {
        std::cerr << "Unsupported WAV format for: " << filename << std::endl;
        delete[] data;
        return;
    }

    // Generate the buffer if not already created
    if (buffer == 0) {
        alGenBuffers(1, &buffer);
    }

    // Load the audio data into the OpenAL buffer
    alBufferData(buffer, format, data, header.dataSize, header.sampleRate);
    delete[] data; // Free audio data
}


void loadSoundInBackground() {
    loadWAVFile("Super Mario Win.wav", bufferBackground);
}

// Play the background music
void playBackgroundMusic() {
    alSourcei(sourceBackground, AL_BUFFER, bufferBackground);
    alSourcei(sourceBackground, AL_LOOPING, AL_TRUE); // Loop background music
    alSourcePlay(sourceBackground);
}

// Stop the background music
void stopBackgroundMusic() {
    alSourceStop(sourceBackground);
}



// Cleanup OpenAL
void cleanupOpenAL() {
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);
}


// Initialize OpenGL
void init() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Background color
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);  // Set coordinate system
}

// Function to draw the player using 4 different primitives
void drawPlayer() {
    glPushMatrix();
    glTranslatef(-0.8f, playerY, 0.0f);  // Player's position
    if (isDucking) {
        glScalef(1.0f, 0.5f, 1.0f);  // Shrink player vertically (reduce height by 50%)
    }

    // Primitive 1: Body (Quad)
    glColor3f(0.0f, 1.0f, 0.0f);  // Green color
    glBegin(GL_QUADS);
    glVertex2f(-0.05f, -0.05f);
    glVertex2f(0.05f, -0.05f);
    glVertex2f(0.05f, 0.1f);  // Taller body
    glVertex2f(-0.05f, 0.1f);
    glEnd();

    // Primitive 2: Head (Triangle)
    glColor3f(1.0f, 1.0f, 0.0f);  // Yellow head
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.03f, 0.1f);
    glVertex2f(0.03f, 0.1f);
    glVertex2f(0.0f, 0.15f);  // Pointy head
    glEnd();

    // Primitive 3: Arms (Line Strips)
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue arms
    glBegin(GL_LINE_STRIP);
    glVertex2f(-0.05f, 0.05f);  // Left arm
    glVertex2f(-0.1f, 0.0f);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex2f(0.05f, 0.05f);  // Right arm
    glVertex2f(0.1f, 0.0f);
    glEnd();

    // Primitive 4: Feet (Points)
    glColor3f(1.0f, 0.0f, 0.0f);  // Red feet
    glPointSize(5.0f);  // Make points larger
    glBegin(GL_POINTS);
    glVertex2f(-0.03f, -0.05f);  // Left foot
    glVertex2f(0.03f, -0.05f);   // Right foot
    glEnd();

    glPopMatrix();
}

// Function to draw the first obstacle (on the ground) using 2 different primitives
void drawGroundObstacle(float x) {
    glPushMatrix();
    glTranslatef(x, obstacleY, 0.0f);  // Obstacle's position

    // Primitive 1: Base (Quad)
    glColor3f(1.0f, 0.0f, 0.0f);  // Red base
    glBegin(GL_QUADS);
    glVertex2f(-0.05f, -0.05f);
    glVertex2f(0.05f, -0.05f);
    glVertex2f(0.05f, 0.05f);
    glVertex2f(-0.05f, 0.05f);
    glEnd();

    // Primitive 2: Top (Triangle)
    glColor3f(0.0f, 1.0f, 1.0f);  // Cyan triangle on top
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.03f, 0.05f);  // Top point
    glVertex2f(0.03f, 0.05f);   // Bottom right
    glVertex2f(0.0f, 0.1f);     // Bottom left
    glEnd();

    glPopMatrix();
}

// Function to draw the second obstacle (above the player's height) using 2 different primitives
void drawAboveObstacle(float x) {
    glPushMatrix();
    glTranslatef(x, obstacleY + 0.2f, 0.0f);  // Positioned above player's height

    // Primitive 1: Base (Polygon)
    glColor3f(0.8f, 0.0f, 1.0f);  // Purple base
    glBegin(GL_POLYGON);  // Hexagon shape
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.03f, 0.02f);
    glVertex2f(0.05f, 0.05f);
    glVertex2f(0.03f, 0.08f);
    glVertex2f(0.0f, 0.1f);
    glVertex2f(-0.03f, 0.08f);
    glVertex2f(-0.05f, 0.05f);
    glVertex2f(-0.03f, 0.02f);
    glEnd();

    // Primitive 2: Connecting Line
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue line connecting base to "ground"
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -0.05f);  // Connect to below the obstacle
    glEnd();

    glPopMatrix();
}

// Function to draw the collectible using 3 different primitives
void drawCollectible(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);  // Collectible's position
    glRotatef(rotationAngle, 0.0f, 1.0f, 0.0f);  // Rotate around the Z-axis

    // Primitive 1: Base (Quad)
    glColor3f(1.0f, 0.5f, 0.0f);  // Orange base
    glBegin(GL_QUADS);
    glVertex2f(-0.03f, -0.03f);
    glVertex2f(0.03f, -0.03f);
    glVertex2f(0.03f, 0.03f);
    glVertex2f(-0.03f, 0.03f);
    glEnd();

    // Primitive 2: Top (Triangle)
    glColor3f(0.0f, 1.0f, 0.0f);  // Green triangle on top
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.02f, 0.03f);  // Bottom left
    glVertex2f(0.02f, 0.03f);   // Bottom right
    glVertex2f(0.0f, 0.06f);    // Top
    glEnd();

    // Primitive 3: Center (Circle approximation with a polygon)
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue circle
    glBegin(GL_POLYGON);
    for (int i = 0; i < 20; i++) {
        float theta = 2.0f * 3.14159f * float(i) / 20.0f;  // Angle for each segment
        float dx = 0.01f * cosf(theta);  // X component
        float dy = 0.01f * sinf(theta);  // Y component
        glVertex2f(dx, dy);
    }
    glEnd();

    glPopMatrix();
}

// Function to check for collision between player and collectible
bool checkCollectibleCollision() {
    float distanceXCollectible = fabs(-0.8f - collectibleX);
    float distanceYCollectible = fabs(playerY - collectibleY);
    return (distanceXCollectible < 0.1f && distanceYCollectible < 0.1f);  // Simple bounding box collision
}

// Function to randomly respawn collectibles, ensuring they do not overlap with obstacles
void respawnCollectible() {
    int maxAttempts = 10;  // Maximum attempts to find a valid position
    int attempts = 0;

    // Random vertical position within reachable range (between -0.05 and 0.2)
    float newY = ((rand() % 4) * 0.05f) - 0.05f;  // Possible values: -0.05, 0, 0.05, 0.10, 0.15

    // Respawn collectible with a new X position, ensuring it doesn't overlap with obstacles
    float newX;
    do {
        newX = 1.0f;  // Start collectibles from the right side
        attempts++;
    } while ((fabs(newX - groundObstacleX) < 0.2f || fabs(newX - aboveObstacleX) < 0.2f) && attempts < maxAttempts);

    // If after max attempts we couldn't find a valid spot, just place it far enough
    if (attempts >= maxAttempts) {
        newX = 1.0f + 0.3f;  // Just place it slightly farther away
    }

    collectibleX = newX;
    collectibleY = newY;
}



void drawHUD() {
    char hud[50];
    sprintf(hud, "Score: %d", score);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(-0.9f, 0.85f);
    for (int i = 0; hud[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, hud[i]);
    }
}

// Function to draw one health unit (heart) using 2 primitives: quad and triangle
void drawHealthUnit(float x, float y) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);  // Position the health unit

    // Primitive 1: Bottom part (Triangle)
    glColor3f(1.0f, 0.0f, 0.0f);  // Red color
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.02f, 0.0f);   // Bottom left
    glVertex2f(0.02f, 0.0f);    // Bottom right
    glVertex2f(0.0f, 0.03f);    // Top
    glEnd();

    // Primitive 2: Top part (Quad for round top)
    glBegin(GL_QUADS);
    glVertex2f(-0.015f, 0.03f);  // Bottom left
    glVertex2f(0.015f, 0.03f);   // Bottom right
    glVertex2f(0.015f, 0.045f);  // Top right
    glVertex2f(-0.015f, 0.045f); // Top left
    glEnd();

    glPopMatrix();
}

// Function to draw the player's health with multiple health units (hearts)
void drawHealth() {
    for (int i = 0; i < health; i++) {
        drawHealthUnit(-0.7f + i * 0.08f, 0.75f);  // Spacing each heart by 0.08 units
    }
}

// Function to draw the remaining time on top of the screen
void drawTimer(int remainingTime) {
    char timeText[50];
    sprintf(timeText, "Time: %d", remainingTime);  // Display remaining time

    glColor3f(1.0f, 1.0f, 1.0f);  // White text
    glRasterPos2f(0.6f, 0.85f);  // Position text at the top-right corner
    for (int i = 0; timeText[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, timeText[i]);
    }
}

// Function to display Game Over message
void displayGameOver() {
    glColor3f(1.0f, 0.0f, 0.0f);
    glRasterPos2f(-0.2f, 0.0f);
    const char* message = "GAME LOSE!";
    for (int i = 0; message[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
    }

    char scoreMessage[50];
    sprintf(scoreMessage, "Final Score: %d", score);
    glRasterPos2f(-0.2f, -0.1f);
    for (int i = 0; scoreMessage[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, scoreMessage[i]);
    }

    glutSwapBuffers();  // Swap buffers to display message
}

// Function to display Game Over message
void displayGameEnd() {
    glColor3f(0.0f, 0.0f, 1.0f);
    glRasterPos2f(-0.2f, 0.0f);
    const char* message = "GAME END!";
    for (int i = 0; message[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
    }

    char scoreMessage[50];
    sprintf(scoreMessage, "Final Score: %d", score);
    glRasterPos2f(-0.2f, -0.1f);
    for (int i = 0; scoreMessage[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, scoreMessage[i]);
    }

    glutSwapBuffers();  // Swap buffers to display message
}

// Function to draw the ground line
void drawGroundLine() {
    glColor3f(1.0f, 1.0f, 1.0f);  // White line
    glBegin(GL_LINES);
    glVertex2f(-1.0f, obstacleY - 0.05f);  // Ground level where obstacles are
    glVertex2f(1.0f, obstacleY - 0.05f);
    glEnd();
}

// Function to draw the upper border with 4 different primitives
void drawUpperBorder() {
    glPushMatrix();

    // Primitive 1: Upper Line (Line strip)
    glColor3f(1.0f, 1.0f, 1.0f);  // White color
    glBegin(GL_LINE_STRIP);
    glVertex2f(-1.0f, 0.95f);  // Left end
    glVertex2f(1.0f, 0.95f);   // Right end
    glEnd();

    // Primitive 2: Small Quad blocks on the upper border
    glColor3f(0.5f, 0.5f, 0.5f);  // Gray color
    for (float x = -0.9f; x <= 0.9f; x += 0.2f) {
        glBegin(GL_QUADS);
        glVertex2f(x, 0.93f);      // Bottom left
        glVertex2f(x + 0.1f, 0.93f); // Bottom right
        glVertex2f(x + 0.1f, 0.95f); // Top right
        glVertex2f(x, 0.95f);      // Top left
        glEnd();
    }

    // Primitive 3: Triangular spikes on the upper border
    glColor3f(1.0f, 0.0f, 0.0f);  // Red color
    for (float x = -0.9f; x <= 0.9f; x += 0.2f) {
        glBegin(GL_TRIANGLES);
        glVertex2f(x + 0.05f, 0.95f); // Base center
        glVertex2f(x, 0.97f);        // Left spike
        glVertex2f(x + 0.1f, 0.97f); // Right spike
        glEnd();
    }

    // Primitive 4: Circle decorations on the upper border
    glColor3f(0.0f, 1.0f, 0.0f);  // Green color
    for (float x = -0.8f; x <= 0.8f; x += 0.4f) {
        glBegin(GL_POLYGON);  // Circle approximation using a polygon
        for (int i = 0; i < 20; i++) {
            float theta = 2.0f * 3.14159f * float(i) / 20.0f;
            float dx = 0.02f * cosf(theta);  // X component
            float dy = 0.02f * sinf(theta);  // Y component
            glVertex2f(x + dx, 0.96f + dy);
        }
        glEnd();
    }

    glPopMatrix();
}

// Function to draw the lower border with 4 different primitives
void drawLowerBorder() {
    glPushMatrix();

    // Primitive 1: Lower Line (Line strip)
    glColor3f(1.0f, 1.0f, 1.0f);  // White color
    glBegin(GL_LINE_STRIP);
    glVertex2f(-1.0f, -0.95f);  // Left end
    glVertex2f(1.0f, -0.95f);   // Right end
    glEnd();

    // Primitive 2: Small Quad blocks on the lower border
    glColor3f(0.5f, 0.5f, 0.5f);  // Gray color
    for (float x = -0.9f; x <= 0.9f; x += 0.2f) {
        glBegin(GL_QUADS);
        glVertex2f(x, -0.95f);     // Bottom left
        glVertex2f(x + 0.1f, -0.95f); // Bottom right
        glVertex2f(x + 0.1f, -0.93f); // Top right
        glVertex2f(x, -0.93f);     // Top left
        glEnd();
    }

    // Primitive 3: Triangular spikes on the lower border
    glColor3f(1.0f, 0.0f, 0.0f);  // Red color
    for (float x = -0.9f; x <= 0.9f; x += 0.2f) {
        glBegin(GL_TRIANGLES);
        glVertex2f(x + 0.05f, -0.95f); // Base center
        glVertex2f(x, -0.97f);        // Left spike
        glVertex2f(x + 0.1f, -0.97f); // Right spike
        glEnd();
    }

    // Primitive 4: Circle decorations on the lower border
    glColor3f(0.0f, 1.0f, 0.0f);  // Green color
    for (float x = -0.8f; x <= 0.8f; x += 0.4f) {
        glBegin(GL_POLYGON);  // Circle approximation using a polygon
        for (int i = 0; i < 20; i++) {
            float theta = 2.0f * 3.14159f * float(i) / 20.0f;
            float dx = 0.02f * cosf(theta);  // X component
            float dy = 0.02f * sinf(theta);  // Y component
            glVertex2f(x + dx, -0.96f + dy);
        }
        glEnd();
    }

    glPopMatrix();
}

// Function to draw a circle (background decoration)
void drawCircle(float x, float y, float radius) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);  // Move to the circle's position
    glColor3f(0.0f, 0.5f, 0.8f);  // Light blue color for the circle

    glBegin(GL_POLYGON);  // Approximate circle using a polygon
    for (int i = 0; i < 50; i++) {
        float theta = 2.0f * 3.14159f * float(i) / 50.0f;  // Angle for each segment
        float dx = radius * cosf(theta);  // X component
        float dy = radius * sinf(theta);  // Y component
        glVertex2f(dx, dy);
    }
    glEnd();

    glPopMatrix();
}

// Function to animate and draw the background circles
void updateBackground() {
    for (int i = 0; i < numCircles; i++) {
        // Move the circle to the left
        circleX[i] -= circleSpeed[i];

        // If the circle moves off the left side, respawn it on the right
        if (circleX[i] < -1.2f) {
            circleX[i] = 1.2f;  // Respawn on the right side
        }

        // Draw the circle
        drawCircle(circleX[i], circleY[i], 0.08f);  // Fixed radius for each circle
    }
}

// Function to draw Power-Up 1 (Invincibility) using 4 different primitives
void drawPowerup1(float x, float y) {
    float size = 0.1f;

    float animationOffset = 0.03f * sinf(glutGet(GLUT_ELAPSED_TIME) / 500.0f);
    glPushMatrix();
    glTranslatef(0.0f, animationOffset, 0.0f);

    // Amount to translate the red part to the right
    float rectWidth = size * 0.7f; // Width of the red and white rectangles (equal to diameter of semicircles)
    float rectHeight = size * 0.5f; // Height for the small squares

    // Draw the left part (red semicircle)
    glColor3f(1.0f, 0.5f, 0.0f); // Red color for the semicircle
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 20; i++) {
        // Adjust the angle for left-side semicircle (90 degrees to 270 degrees)
        float theta = 3.14159f * (float(i) / 20.0f + 0.5f); // Sweep from 90 to 270 degrees

        float cx = x; // Center of the left semicircle
        float cy = y + rectHeight * 0.5f; // Center vertically (aligned with the small squares)

        // Create vertices for the left semicircle
        glVertex2f(cx + (rectHeight * 0.5f) * cosf(theta), cy + (rectHeight * 0.5f) * sinf(theta));
    }
    glEnd();

    // Draw the red square (in between the semicircles)
    glColor3f(1.0f, 0.5f, 0.0f); // Red color for the square
    glBegin(GL_QUADS);
    glVertex2f(x, y);                    // Bottom-left
    glVertex2f(x + rectWidth, y);        // Bottom-right
    glVertex2f(x + rectWidth, y + rectHeight); // Top-right
    glVertex2f(x, y + rectHeight);       // Top-left
    glEnd();

    // Draw the white square (in between the semicircles)
    glColor3f(1.0, 1.0, 1.0); // White color for the square
    glBegin(GL_QUADS);
    glVertex2f(x + rectWidth, y);                    // Bottom-left
    glVertex2f(x + rectWidth * 2.0f, y);             // Bottom-right
    glVertex2f(x + rectWidth * 2.0f, y + rectHeight); // Top-right
    glVertex2f(x + rectWidth, y + rectHeight);       // Top-left
    glEnd();

    // Draw the right part (white semicircle)
    glColor3f(1.0, 1.0, 1.0); // White color for the semicircle
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 20; i++) {
        // Adjust the angle for right-side semicircle (270 degrees to 450 degrees)
        float theta = 3.14159f * (float(i) / 20.0f - 0.5f); // Sweep from 270 to 450 degrees

        float cx = x + rectWidth * 2.0f; // Center of the right semicircle
        float cy = y + rectHeight * 0.5f; // Center vertically (aligned with the small squares)

        // Create vertices for the right semicircle
        glVertex2f(cx + (rectHeight * 0.5f) * cosf(theta), cy + (rectHeight * 0.5f) * sinf(theta));
    }
    glEnd();

    glPopMatrix();
}

// Function to draw Power-Up 2 (Slow-Motion) using 4 different primitives
void drawPowerup2(float x, float y) {
    float animationOffset = 0.03f * sinf(glutGet(GLUT_ELAPSED_TIME) / 500.0f);
    glPushMatrix();
    glTranslatef(0.0f, animationOffset, 0.0f);

    float size = 0.1f;
    // Draw a yellow lightning bolt for speed power-up
    glColor3f(0.5f, 1.0f, 1.0f); // Cyan color for the lightning bolt

    // First right-angled triangle (top)
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y + size); // Top vertex (at the top of the bolt)
    glVertex2f(x + size * 0.5f, y + size * 0.5f); // Mid-right vertex
    glVertex2f(x, y + size * 0.5f); // Mid-left vertex
    glEnd();

    // Second right-angled triangle (bottom, flipped to the left)
    float shiftAmount = 0.025f; // Amount to shift the bottom triangle to the left
    glBegin(GL_TRIANGLES);
    glVertex2f(x + size * 0.5f - shiftAmount, y);           // Bottom-right vertex (shifted left)
    glVertex2f(x - shiftAmount, y + size * 0.5f);           // Tip of the bottom triangle (pointing left)
    glVertex2f(x + size * 0.5f - shiftAmount, y + size * 0.5f); // Base vertex (shared with the top triangle, shifted left)
    glEnd();

    // Draw a line between both triangles
    glColor3f(1.0, 1.0, 1.0); // White color for the line
    glBegin(GL_LINES);
    glVertex2f(x + size * 0.25f, y + size * 0.5f); // Start at the tip of the top triangle
    glVertex2f(x + size * 0.25f, y);         // Draw a line down to the base of the bottom triangle
    glEnd();

    // Draw outline for the top triangle
    glColor3f(1.0, 1.0, 1.0); // White color for outline
    glBegin(GL_LINE_LOOP); // Outline for the top triangle
    glVertex2f(x, y + size); // Top vertex
    glVertex2f(x + size * 0.5f, y + size * 0.5f); // Mid-right vertex
    glVertex2f(x, y + size * 0.5f); // Mid-left vertex
    glEnd();

    // Draw outline for the bottom triangle
    glBegin(GL_LINE_LOOP); // Outline for the bottom triangle
    glVertex2f(x + size * 0.5f - shiftAmount, y); // Bottom-right vertex (shifted left)
    glVertex2f(x - shiftAmount, y + size * 0.5f); // Tip of the bottom triangle (pointing left)
    glVertex2f(x + size * 0.5f - shiftAmount, y + size * 0.5f); // Base vertex
    glEnd();

    glPopMatrix();
}



// Function to update the display
void display() {
    glClear(GL_COLOR_BUFFER_BIT);  // Clear the screen

    playBackgroundMusic();

    if (isGameEnd) {
        displayGameEnd();  // Display Game Over message

        return;             // Stop drawing game elements
    }


    if (isGameOver) {
        displayGameOver();  // Display Game Over message
        return;             // Stop drawing game elements
    }

    // Draw and animate the background circles
    updateBackground();

    // Draw the upper and lower borders
    drawUpperBorder();
    drawLowerBorder();

    // Draw the ground line
    drawGroundLine();

    // Draw the player with 4 different primitives
    drawPlayer();

    // Draw the obstacles with 2 different types of primitives
    drawGroundObstacle(groundObstacleX);   // First obstacle on the ground
    drawAboveObstacle(aboveObstacleX);     // Second obstacle above player's height

    // Draw the collectible
    drawCollectible(collectibleX, collectibleY);  // Draw collectible at current position

    // Display "Health: " label before the health hearts
    glRasterPos2f(-0.9f, 0.75f);  // Position the label a bit lower
    const char* healthText = "Health: ";
    for (int i = 0; healthText[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, healthText[i]);
    }

    // Draw the player's health as hearts
    drawHealth();  // Replaces the numeric health display

    // Draw the remaining time at the top of the screen
    int remainingTime = totalTime - (int)elapsedTime;
    drawTimer(remainingTime);

    drawHUD();  // Display the health and score

    if (powerUp1Spawned) {
        drawPowerup1(powerUp1X, 0.2f);  // Draw Power-Up 1 at its position
    }

    if (powerUp2Spawned) {
        drawPowerup2(powerUp2X, 0.2f);  // Draw Power-Up 2 at its position
    }

    glutSwapBuffers();  // Swap buffers for animation
}

// Function to check for collisions with either the ground or above obstacle
bool checkCollision() {
    // Adjust player's height for ducking
    float effectivePlayerY = isDucking ? playerY - 0.05 : playerY;

    // Collision with ground obstacle (jump to dodge)
    float distanceXGround = fabs(-0.8f - groundObstacleX);
    float distanceYGround = fabs(effectivePlayerY - obstacleY);  // Ground level collision check
    groundCollision = (distanceXGround < 0.1f && distanceYGround < 0.1f);

    // Collision with above obstacle (duck to dodge)
    float distanceXAbove = fabs(-0.8f - aboveObstacleX);
    float distanceYAbove = fabs((effectivePlayerY + 0.13) - (obstacleY + 0.2f));  // Adjust for player's head collision
    aboveCollision = (distanceXAbove < 0.1f && distanceYAbove < 0.1f);

    // Return true if player hits either obstacle
    return (groundCollision || aboveCollision);
}

bool checkPowerup1Collision() {
    // Adjust player's height for ducking
    float effectivePlayerY = isDucking ? duckTargetY : playerY;

    float distanceX = fabs(-0.8f - powerUp1X);
    float distanceY = fabs(effectivePlayerY - 0.2);
    return (distanceX < 0.1f && distanceY < 0.1f);  // Check if player is close enough to the power-up
}

bool checkPowerup2Collision() {
    // Adjust player's height for ducking
    float effectivePlayerY = isDucking ? duckTargetY : playerY;

    float distanceX = fabs(-0.8f - powerUp2X);
    float distanceY = fabs(effectivePlayerY - 0.2);
    return (distanceX < 0.1f && distanceY < 0.1f);  // Check if player is close enough to the power-up
}


// Timer function to update the game state
void update(int value) {
    if (isGameOver) {
        return;  // Stop updating if the game is over
    }

    // Increment elapsed time
    elapsedTime += 0.016f;  // Assuming 60 FPS, so approx. 1/60th of a second per frame

    // Calculate remaining time
    int remainingTime = totalTime - (int)elapsedTime;
    if (remainingTime <= 0) {
        isGameEnd = true;  // End game when time is up
    }

    // Increase speed over time (speedMultiplier increases gradually)
    if (!powerUp2Active) {
        speedMultiplier = 1.0f + (elapsedTime / 30.0f);  // Speed increases gradually every 30 seconds
    }

    if (!powerUp1Active) {
        bool collided = checkCollision();
    }

    // Move both obstacles to the left at increasing speed
    if (!(groundCollision || aboveCollision)) {
        groundObstacleX -= baseSpeed * speedMultiplier;  // Move ground obstacle faster over time
        aboveObstacleX -= baseSpeed * speedMultiplier;   // Move above obstacle faster over time
        collectibleX -= baseSpeed * speedMultiplier;     // Move collectible to the left at increasing speed
    }


    // Move power-up to the left at increasing speed
    if (powerUp1Spawned) {
        powerUp1X -= baseSpeed * speedMultiplier;
    }

    if (powerUp2Spawned) {
        powerUp2X -= baseSpeed * speedMultiplier;
    }

    // Update rotation angle for collectible
    rotationAngle += 10.0f;  // Rotate by 2 degrees per frame
    if (rotationAngle > 360.0f) {
        rotationAngle -= 360.0f;  // Keep angle within 0 to 360 degrees
    }

    // Respawn both obstacles when the second (above) obstacle reaches the left end
    if (aboveObstacleX < -1.0f) {
        groundObstacleX = 1.0f;  // Reset ground obstacle to the right
        aboveObstacleX = 2.5f;   // Reset above obstacle to the right
    }

    // Respawn collectible when it goes off-screen or is collected
    if (collectibleX < -1.0f || checkCollectibleCollision()) {
        if (checkCollectibleCollision()) {
            score += 5;  // Increase score by 5 when a collectible is collected
        }
        // Respawn collectible safely
        respawnCollectible();
    }

    // Handle jump animation
    if (isJumping) {
        float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;  // Get time in seconds
        float elapsedTime = currentTime - jumpStartTime;

        if (elapsedTime < jumpDuration) {
            // Animate the jump (move up to jumpTargetY in half the time, and then back down)
            if (elapsedTime < jumpDuration / 2.0f) {
                playerY = (jumpTargetY / (jumpDuration / 2.0f)) * elapsedTime;
            }
            else {
                playerY = jumpTargetY - (jumpTargetY / (jumpDuration / 2.0f)) * (elapsedTime - jumpDuration / 2.0f);
            }
        }
        else {
            // End of jump, reset to initial position
            playerY = 0.0f;
            isJumping = false;
        }
    }

    // Check for collision and reduce health with cooldown
    if (!powerUp1Active) {
        // Handle collision with ground obstacle
        if (groundCollision) {
            // Stop player from passing through and lose lives
            if (!isJumping) {  // Player can escape only by jumping
                float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;  // Get time in seconds
                if (currentTime - lastCollisionTime >= collisionCooldown) {
                    health--;  // Decrease health if collision detected
                    lastCollisionTime = currentTime;  // Reset the cooldown timer

                    if (health <= 0) {
                        isGameOver = true;  // Trigger Game Over when health reaches 0
                    }
                }
            }
        }

        // Handle collision with above obstacle
        if (aboveCollision) {
            // Stop player from passing through and lose lives
            if (!isDucking) {  // Player can escape only by ducking
                float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;  // Get time in seconds
                if (currentTime - lastCollisionTime >= collisionCooldown) {
                    health--;  // Decrease health if collision detected
                    lastCollisionTime = currentTime;  // Reset the cooldown timer

                    if (health <= 0) {
                        isGameOver = true;  // Trigger Game Over when health reaches 0
                    }
                }
            }
        }
    }


    if (powerUp1Spawned) {
        if (checkPowerup1Collision()) {
            powerUp1X = -2.0f;
            powerUp1Spawned = false;
            powerUp1Active = true;
        }
    }

    if (powerUp2Spawned) {
        if (checkPowerup2Collision()) {
            powerUp2X = -2.0f;
            powerUp2Spawned = false;
            powerUp2Active = true;
            oldSpeedMultiplier = speedMultiplier;
            speedMultiplier = 1.0f;
        }
    }

    // Check if 15 seconds have passed to spawn Power-Up 1
    if (!powerUp1Spawned && elapsedTime >= 10.0f) {
        powerUp1Spawned = true;  // Spawn Power-Up 1 after 10 seconds
    }

    // Check if 40 seconds have passed to spawn Power-Up 2
    if (!powerUp2Spawned && elapsedTime >= 40.0f) {
        powerUp2Spawned = true;  // Spawn Power-Up 2 after 40 seconds
    }

    if (powerUp1Active && elapsedTime >= 20.0f) {
        powerUp1Active = false;
    }

    if (powerUp2Active && elapsedTime >= 50.0f) {
        powerUp2Active = false;
        speedMultiplier = oldSpeedMultiplier;
    }

    // Redraw the scene
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);  // 60 FPS update
}

// Input handling for special keys (Arrow keys)
void specialInput(int key, int x, int y) {
    if (key == GLUT_KEY_UP && !isJumping && !isGameOver) {
        // Start the jump
        isJumping = true;
        jumpStartTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;  // Get time in seconds
    }
    if (key == GLUT_KEY_DOWN && !isDucking && !isJumping && !isGameOver) {
        // Start ducking (but not fully low to avoid ground collision)
        //playerY = duckTargetY;  // Move player to duck position
        isDucking = true;
    }
}

// Handle key release for ducking
void specialInputUp(int key, int x, int y) {
    if (key == GLUT_KEY_DOWN && !isGameOver) {
        // Stop ducking, return to initial position
        //playerY = 0.0f;
        isDucking = false;
    }
}

// Main function
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("2D Infinite Runner");

    initOpenAL();
    std::thread soundThread(loadSoundInBackground);
    soundThread.join();
    cleanupOpenAL();


    init();

    glutDisplayFunc(display);
    glutSpecialFunc(specialInput);      // Handle key press events (Jump/Duck)
    glutSpecialUpFunc(specialInputUp);  // Handle key release events (Duck)
    glutTimerFunc(25, update, 0);



    glutMainLoop();
    return 0;
}
