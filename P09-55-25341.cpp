#include <cstdio>
#include <glut.h>
#include <cmath>
#include <cstdlib>  // For rand()
#include <ctime>
#include <vector>
#include <string>
#include <al.h>
#include <alc.h>
#include <iostream>
#include <fstream>
#include <thread>


// Function to initialize OpenAL
ALCdevice* device;
ALCcontext* context;
ALuint buffer, source;
ALuint bufferBackground, bufferYouDied, bufferYouWin, bufferCollision;
ALuint sourceBackground, sourceYouDied, sourceYouWin, sourceCollision;

void initOpenAL() {
	device = alcOpenDevice(NULL); // Open default device
	std::cerr << device << std::endl;
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
	loadWAVFile("Stereo Madness.wav", bufferBackground);
	loadWAVFile("Dark Souls.wav", bufferYouDied);
	loadWAVFile("Super Mario Win.wav", bufferYouWin);
	loadWAVFile("Super Mario Down.wav", bufferCollision);

}
// Function to play the "YOU DIED" sound
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

// Play the "You Died" sound
void playYouDiedSound() {
	alSourcei(sourceYouDied, AL_BUFFER, bufferYouDied);
	alSourcePlay(sourceYouDied);
}

// Play the "You Win" sound
void playYouWinSound() {
	alSourcei(sourceYouWin, AL_BUFFER, bufferYouWin);
	alSourcePlay(sourceYouWin);
}

// Play the obstacle collision sound
void playCollisionSound() {
	alSourcei(sourceCollision, AL_BUFFER, bufferCollision);
	alSourcePlay(sourceCollision);
}


// Cleanup OpenAL
void cleanupOpenAL() {
	alDeleteSources(1, &source);
	alDeleteBuffers(1, &buffer);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);
}

// Example usage in your game code



float speed = 0.01f; // Control the speed of movement (adjust as needed)



class Player {
public:
	float x, y; // Player position
	float width, height; // Player size
	float jumpHeight; // Height for jump
	bool isJumping; // Flag for jump state
	bool isDucking; // Flag for duck state
	float duckStartTime; // Time when ducking started
	float jumpStartTime; // Time when jumping started
	float maxJumpHeight = 0.3f; // Maximum height during the jump
	bool invincible; // New attribute for invincibility

	Player(float initX, float initY, float w, float h);
	void draw(); // Render the player
	void jump(); // Handle jumping
	void duck(); // Handle ducking
	void update(); // Update player state
};

Player::Player(float initX, float initY, float w, float h)
	: x(initX), y(initY), width(w), height(h), jumpHeight(0.0f),
	isJumping(false), isDucking(false), duckStartTime(0.0f), jumpStartTime(0.0f), invincible(false) {}


void Player::draw() {
	// Body (a simple square or rectangle depending on state)
	if (isDucking) {
		height = 0.05f; // Height when ducking
		width = 0.1f; // Wider when ducking
		glColor3f(0.50, 1.0, 0.50); // Different color for ducking
	}
	else if (isJumping) {
		height = 0.1f; // Maintain constant height while jumping
		glColor3f(1.0, 0.50, 0.50); // Color while jumping
	}
	else {
		height = 0.1f; // Normal height
		glColor3f(1.0, 0.50, 0.50); // Color while idle
	}

	glBegin(GL_POLYGON);
	glVertex2f(x, y); // Bottom-left
	glVertex2f(x + width, y); // Bottom-right
	glVertex2f(x + width, y + height); // Top-right
	glVertex2f(x, y + height); // Top-left
	glEnd();

	// Eyes (two small squares)
	glColor3f(0.0, 1.0, 1.0); // White color for the eyes
	float eyeSize = 0.02f; // Size of the eyes
	float eyeY = y + height * 0.60f; // Y position for the eyes
	glLineWidth(3.0f);
	// Left eye
	glBegin(GL_LINES);
	glVertex2f(x + width * 0.25f - eyeSize / 2, eyeY);

	glVertex2f(x + width * 0.25f + eyeSize / 2, eyeY + eyeSize); // Top-right

	glEnd();
	glBegin(GL_LINES);
	glVertex2f(x + width * 0.25f - eyeSize / 2, eyeY + eyeSize);
	glVertex2f(x + width * 0.25f + eyeSize / 2, eyeY);

	glEnd();
	glLineWidth(1.0f);
	// Right eye
	glBegin(GL_QUADS);
	glVertex2f(x + width * 0.75f - eyeSize / 2, eyeY); // Bottom-left
	glVertex2f(x + width * 0.75f + eyeSize / 2, eyeY); // Bottom-right
	glVertex2f(x + width * 0.75f + eyeSize / 2, eyeY + eyeSize); // Top-right
	glVertex2f(x + width * 0.75f - eyeSize / 2, eyeY + eyeSize); // Top-left
	glEnd();

	// Mouth (a simple rectangle)
	glBegin(GL_LINE_LOOP);
	glVertex2f(x + width * 0.25f, y + height * 0.3f); // Bottom-left
	glVertex2f(x + width * 0.75f, y + height * 0.3f); // Bottom-right
	glVertex2f(x + width * 0.75f, y + height * 0.45f); // Top-right
	glVertex2f(x + width * 0.25f, y + height * 0.45f); // Top-left
	glEnd();
}

void Player::jump() {
	if (!isJumping) {
		isJumping = true; // Start jumping
		jumpStartTime = glutGet(GLUT_ELAPSED_TIME) / 500.0f; // Record the time the jump starts
	}
}

void Player::duck() {
	if (!isDucking) {
		isDucking = true; // Start ducking
		duckStartTime = glutGet(GLUT_ELAPSED_TIME) / 500.0f; // Record the current time in seconds
	}
}

void Player::update() {
	if (isJumping) {
		// Calculate elapsed time since the jump started
		float currentTime = glutGet(GLUT_ELAPSED_TIME) / 500.0f;
		float elapsedTime = currentTime - jumpStartTime;

		if (elapsedTime <= 1.5f) {
			// Phase 1: Ascend smoothly for the first 1.5 seconds
			float progress = elapsedTime / 1.5f; // Progress from 0 to 1 over 1.5 seconds
			y = 0.05f + progress * maxJumpHeight; // Gradually increase y (smooth ascent)
		}
		else if (elapsedTime <= 3.0f) {
			// Phase 2: Descend smoothly for the next 1.5 seconds
			float progress = (elapsedTime - 1.5f) / 1.5f; // Progress from 0 to 1 over the second 1.5 seconds
			y = 0.05f + (1.0f - progress) * maxJumpHeight; // Gradually decrease y (smooth descent)
		}
		else {
			// Jump complete: reset jumping state and position
			isJumping = false;
			y = 0.05f; // Reset to ground level
		}
	}

	// Stop ducking after 1 second
	if (isDucking) {
		float currentTime = glutGet(GLUT_ELAPSED_TIME) / 500.0f; // Current time in seconds
		if (currentTime - duckStartTime >= 1.0f) {
			isDucking = false; // Stop ducking
			width = 0.1f; // Reset width to original
			height = 0.1f; // Reset height to original
		}
	}
}






class Obstacle {
public:
	float x, y;
	float width, height;

	// Variables for the animation
	bool isAnimating = false; // Animation state
	float initialX = 0.0f;            // Initial X position
	float targetX = 0.0f;             // Target X position
	float animationStartTime = 0.0f;  // Start time for animation
	bool hitPlayer = false; // To track recent collision
	float hitTime = 0.0f;

	Obstacle(float initX, float initY, float w, float h);
	void draw();
	void move(float speed);
	void startMoveBackAnimation(); // Initiate the animation
	void updateAnimation(float currentTime); // Update the animation state
	void updateHitStatus(float currentTime); // Reset hitPlayer after 0.5 seconds
};


Obstacle::Obstacle(float initX, float initY, float w, float h) : x(initX), y(initY), width(w), height(h) {}


void Obstacle::draw() {
	// Draw the screw head (shorter height)
	glColor3f(0.5, 0.5, 0.5); // Gray color for the screw head
	glBegin(GL_QUADS);
	glVertex2f(x, y - height * 0.3f);                          // Bottom-left of the screw head
	glVertex2f(x + width * 0.2f, y - height * 0.3f);          // Bottom-right of the screw head
	glVertex2f(x + width * 0.2f, y + height * 0.5f); // Top-right of the screw head (shorter height)
	glVertex2f(x, y + height * 0.5f);                 // Top-left of the screw head
	glEnd();

	// Draw the screw body (wider width, same height)
	glColor3f(0.3, 0.3, 0.3); // Darker gray color for the screw body
	glBegin(GL_POLYGON);
	glVertex2f(x - width * 1.5f, y + height * 0.2f);                 // Bottom-left of the screw body (wider)
	glVertex2f(x, y + height * 0.2f);                                 // Bottom-right of the screw body
	glVertex2f(x, y + height * 0.05f);                 // Top-right of the screw body (same height)
	glVertex2f(x - width * 1.5f, y + height * 0.05f); // Top-left of the screw body
	glEnd();
}

void Obstacle::startMoveBackAnimation() {
	isAnimating = true;
	initialX = x;               // Save the current position
	targetX = x + 0.5f;         // Set the target position
	animationStartTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f; // Start time in seconds
}

// Update the animation state
void Obstacle::updateAnimation(float currentTime) {
	if (isAnimating) {
		float elapsed = currentTime - animationStartTime; // Elapsed time

		if (elapsed >= 1.0f) { // Animation complete after 0.5 seconds
			x = targetX; // Ensure the obstacle reaches the target position
			isAnimating = false; // Stop animation
		}
		else {
			// Interpolate the X position based on elapsed time (smooth movement)
			float t = elapsed / 1.0f; // Normalize time between 0 and 1
			x = initialX + t * (targetX - initialX); // Linear interpolation
		}
	}
}
void Obstacle::updateHitStatus(float currentTime) {
	if (hitPlayer && (currentTime - hitTime >= 0.5f)) {
		hitPlayer = false; // Reset hitPlayer after 0.5 seconds
	}
}





void Obstacle::move(float speed) {
	// Move the obstacle leftwards towards the player
	x -= speed;
}
int i = 0;

class Collectable {
public:
	float x, y;
	float radius;

	Collectable(float initX, float initY, float r);
	void draw();
	void move(float speed);
};

Collectable::Collectable(float initX, float initY, float r) : x(initX), y(initY), radius(r) {}

void Collectable::draw() {
	int sides = 20; // Use 6 for hexagon or 8 for octagon
	// Yellow color for the coin
	glColor3f(0.9, 0.9, 0.0);

	// Save the current transformation matrix
	glPushMatrix();

	// Move to the center of the collectable
	glTranslatef(x, y, 0.0f);

	// Rotate around the Y-axis for a horizontal spin
	static float angle = 0.0f; // Static to accumulate the rotation angle over time
	glRotatef(angle, 0.0f, 1.0f, 0.0f); // Rotation around Y-axis

	// Increment the angle to make it spin (adjust the value for speed control)
	angle += 1.0f;

	// Draw the outer shape (hexagon or octagon)
	glBegin(GL_POLYGON);
	for (int i = 0; i < sides; i++) {
		float theta = 2.0f * 3.14159f * float(i) / float(sides); // Angle for each vertex
		glVertex3f((radius * 1.5f) * 0.7 * cosf(theta), (radius * 1.5f) * 0.5 * sinf(theta), 0.0f); // Calculate vertex position
	}
	glEnd();

	// Shiny white lines inside (creating a simple shine effect)
	glColor3f(1.0, 1.0, 1.0); // White color for the shine lines
	glLineWidth(2.0f);

	// Draw a set of lines radiating from the center
	glBegin(GL_LINE_LOOP); // White outline inside the shape
	for (int i = 0; i < sides; i++) {
		float theta = 2.0f * 3.14159f * float(i) / float(sides);
		glVertex3f((radius * 1.5f) * 0.7 * cosf(theta), (radius * 1.5f) * 0.5 * sinf(theta), 0.0f); // Inner polygon
	}
	glEnd();

	glLineWidth(1.0f);

	// Draw the slot (white rectangle in the center)
	glColor3f(1.0, 1.0, 0.0); // Yellow color for the slot
	glBegin(GL_QUADS);
	glVertex3f(-radius * 0.1f, -radius * 0.5f, 0.0f); // Bottom-left
	glVertex3f(radius * 0.1f, -radius * 0.5f, 0.0f);  // Bottom-right
	glVertex3f(radius * 0.1f, radius * 0.5f, 0.0f);   // Top-right
	glVertex3f(-radius * 0.1f, radius * 0.5f, 0.0f);  // Top-left
	glEnd();

	// Restore the original transformation matrix
	glPopMatrix();
}

void Collectable::move(float speed) {
	x -= speed; // Move the collectible to the left at the given speed
}



class PowerUp {
public:
	float x, y;
	float size;
	bool isSpeedPowerUp; // Flag to distinguish between power-up types

	PowerUp(float initX, float initY, float s, bool speedPowerUp);
	void draw();
	void move(float speed);
};

PowerUp::PowerUp(float initX, float initY, float s, bool speedPowerUp) : x(initX), y(initY), size(s), isSpeedPowerUp(speedPowerUp) {}

void PowerUp::draw() {

	float animationOffset = 0.01f * sinf(glutGet(GLUT_ELAPSED_TIME) / 500.0f);
	glPushMatrix();
	glTranslatef(0.0f, animationOffset, 0.0f);

	if (isSpeedPowerUp) {
		// Draw a yellow lightning bolt for speed power-up
		glColor3f(1.0, 1.0, 0.0); // Yellow color for the lightning bolt

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
	}
	else {
		// Amount to translate the red part to the right
		float rectWidth = size * 0.7f; // Width of the red and white rectangles (equal to diameter of semicircles)
		float rectHeight = size * 0.5f; // Height for the small squares

		// Draw the left part (red semicircle)
		glColor3f(1.0, 0.0, 0.0); // Red color for the semicircle
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
		glColor3f(1.0, 0.0, 0.0); // Red color for the square
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
	}
	glPopMatrix();


}

void PowerUp::move(float speed) {
	x -= speed; // Move the power-up to the left at the given speed
}
std::vector<Collectable> collectables;
std::vector<PowerUp> powerups;
std::vector<Obstacle> obstacles;

float randomFloat(float min, float max) {
	return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

// Random boolean value to determine power-up type (lightning or pill)
bool randomPowerUpType() {
	return rand() % 2 == 0; // Returns true for lightning, false for pill
}

float spawnCollectableTime = 11.0f; // Collectables every 10 seconds
float spawnPowerUpTime = 13.0f;     // Power-ups every 15 seconds
float spawnObstacleTime = 3.0f;     // Obstacles every 5 seconds

// Timers to keep track of spawning time
float collectableTimer = 0.0f;
float powerUpTimer = 0.0f;
float obstacleTimer = 0.0f;

// Spawning logic
void spawnCollectable() {
	// Random y position between 0.1 and 1.0
	float y = randomFloat(0.1f, 0.3f);
	float radius = 0.05f; // Fixed radius for the collectable
	collectables.push_back(Collectable(3.0f, y, radius));  // Add new collectable to vector
	i++;
}

void spawnPowerUp() {
	// Random y position between 0.1 and 1.0
	float y = randomFloat(0.1f, 0.3f);
	float size = 0.1f; // Fixed size for the power-up
	bool isLightning = randomPowerUpType(); // Randomly decide power-up type
	powerups.push_back(PowerUp(3.0f, y, size, isLightning));  // Add new power-up to vector
}

void spawnObstacle() {
	// Random height and width for the obstacle
	float w = randomFloat(0.05f, 0.15f); // Width between 0.1 and 0.3
	float h = randomFloat(0.05f, 0.15f); // Height between 0.1 and 0.2
	float y = randomFloat(0.1f, 0.3f); // Random y position between 0.1 and 0.6
	obstacles.push_back(Obstacle(3.0f, y, w, h));  // Add new obstacle to vector
}

void updateTimers(float deltaTime) {
	// Update timers
	collectableTimer += deltaTime;
	powerUpTimer += deltaTime;
	obstacleTimer += deltaTime;

	// Check for overlapping spawns (within a small delta, e.g., 0.2 seconds)
	float delta = 0.2f;

	// Prioritize obstacle, then collectable, then powerup
	if (abs(obstacleTimer - collectableTimer) < delta) {
		collectableTimer -= 1.3f; // Adjust collectable timer
	}
	if (abs(obstacleTimer - powerUpTimer) < delta) {
		powerUpTimer -= 1.3f; // Adjust powerup timer
	}
	if (abs(collectableTimer - powerUpTimer) < delta) {
		powerUpTimer -= 1.3f; // Adjust powerup timer
	}
	if (obstacleTimer >= spawnObstacleTime) {
		spawnObstacle();
		obstacleTimer = 0.0f; // Reset obstacle timer
	}

	if (collectableTimer >= spawnCollectableTime) {


		spawnCollectable();
		collectableTimer = 0.0f; // Reset collectable timer
	}

	if (powerUpTimer >= spawnPowerUpTime) {

		spawnPowerUp();
		powerUpTimer = 0.0f; // Reset powerup timer
	}
}


void drawBoundaries() {
	// Draw upper boundary
	glColor3f(1.0, 1.0, 1.0); // Set color to white
	glBegin(GL_QUADS); // Upper boundary (4 primitives)
	glVertex2f(0.0, 0.95);
	glVertex2f(3.0, 0.95);
	glVertex2f(3.0, 0.98);
	glVertex2f(0.0, 0.98);
	glEnd();

	// Add symmetrical squares to upper boundary (total 4 squares)
	glColor3f(0.0, 0.0, 0.0); // Red squares
	float squareWidth = 0.07f; // Consistent square width

	// First square (left side)
	glBegin(GL_QUADS);
	glVertex2f(0.15, 0.96);
	glVertex2f(0.15 + squareWidth, 0.96);
	glVertex2f(0.15 + squareWidth, 0.97);
	glVertex2f(0.15, 0.97);
	glEnd();

	// Second square
	glBegin(GL_QUADS);
	glVertex2f(0.85, 0.96);
	glVertex2f(0.85 + squareWidth, 0.96);
	glVertex2f(0.85 + squareWidth, 0.97);
	glVertex2f(0.85, 0.97);
	glEnd();

	// Third square (right side)
	glBegin(GL_QUADS);
	glVertex2f(1.55, 0.96);
	glVertex2f(1.55 + squareWidth, 0.96);
	glVertex2f(1.55 + squareWidth, 0.97);
	glVertex2f(1.55, 0.97);
	glEnd();

	// Fourth square
	glBegin(GL_QUADS);
	glVertex2f(2.25, 0.96);
	glVertex2f(2.25 + squareWidth, 0.96);
	glVertex2f(2.25 + squareWidth, 0.97);
	glVertex2f(2.25, 0.97);
	glEnd();

	glBegin(GL_QUADS);
	glVertex2f(2.85, 0.96);
	glVertex2f(2.85 + squareWidth, 0.96);
	glVertex2f(2.85 + squareWidth, 0.97);
	glVertex2f(2.85, 0.97);
	glEnd();

	// Draw lower boundary
	glColor3f(1.0, 1.0, 1.0); // Set color to white
	glBegin(GL_QUADS); // Lower boundary (4 primitives)
	glVertex2f(0.0, 0.02);
	glVertex2f(3.0, 0.02);
	glVertex2f(3.0, 0.05);
	glVertex2f(0.0, 0.05);
	glEnd();

	// Add symmetrical black squares to lower boundary
	glColor3f(0.0, 0.0, 0.0); // Black squares

	// First square (left side)
	glBegin(GL_QUADS);
	glVertex2f(0.15, 0.03);
	glVertex2f(0.15 + squareWidth, 0.03);
	glVertex2f(0.15 + squareWidth, 0.04);
	glVertex2f(0.15, 0.04);
	glEnd();

	// Second square
	glBegin(GL_QUADS);
	glVertex2f(0.85, 0.03);
	glVertex2f(0.85 + squareWidth, 0.03);
	glVertex2f(0.85 + squareWidth, 0.04);
	glVertex2f(0.85, 0.04);
	glEnd();

	// Third square (right side)
	glBegin(GL_QUADS);
	glVertex2f(1.55, 0.03);
	glVertex2f(1.55 + squareWidth, 0.03);
	glVertex2f(1.55 + squareWidth, 0.04);
	glVertex2f(1.55, 0.04);
	glEnd();

	// Fourth square
	glBegin(GL_QUADS);
	glVertex2f(2.25, 0.03);
	glVertex2f(2.25 + squareWidth, 0.03);
	glVertex2f(2.25 + squareWidth, 0.04);
	glVertex2f(2.25, 0.04);
	glEnd();

	glBegin(GL_QUADS);
	glVertex2f(2.85, 0.03);
	glVertex2f(2.85 + squareWidth, 0.03);
	glVertex2f(2.85 + squareWidth, 0.04);
	glVertex2f(2.85, 0.04);
	glEnd();
}

void drawPyramid(float xBaseLeft, float yBaseLeft, float xBaseRight, float yBaseRight, float xPeak, float yPeak, float r, float g, float b, float alpha) {
	glColor4f(r, g, b, alpha); // Color with transparency
	glBegin(GL_TRIANGLES);

	// Draw the triangle using the passed coordinates
	glVertex2f(xBaseLeft, yBaseLeft);  // Base left
	glVertex2f(xBaseRight, yBaseRight); // Base right
	glVertex2f(xPeak, yPeak);  // Top peak

	glEnd();
}


void drawBoundariesAndDecorations() {
	// Enable blending for transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Draw boundaries
	drawBoundaries();

	// Calculate time-based shift for dancing peaks
	float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f; // Get time in seconds
	float peakShift = 0.1f * sinf(time * 2.0f); // Calculate shift based on sine wave

	// Adjust the size and make peaks "dance"
	drawPyramid(0.1, 0.05, 1.2, 0.05, 0.65 + peakShift, 0.7, 0.5, 0.5, 0.5, 0.3); // First larger pyramid
	drawPyramid(0.7, 0.05, 1.9, 0.05, 1.3 + peakShift, 0.6, 0.4, 0.4, 0.4, 0.2); // Second larger pyramid
	drawPyramid(1.4, 0.05, 2.8, 0.05, 2.1 + peakShift, 0.65, 0.6, 0.6, 0.6, 0.2); // Third larger pyramid

	// Disable blending after decorations
	glDisable(GL_BLEND);
}




void drawHearts(int health) {
	for (int i = 0; i < health; ++i) {
		float x_offset = 0.11 * i; // Increased spacing between hearts

		// Draw the white outline first (same vertices as the heart)
		glColor3f(1.0, 1.0, 1.0); // White outline
		glBegin(GL_LINE_LOOP);

		glVertex2f(0.085 + x_offset, 0.83);  // Bottom middle vertex (the point of the heart)
		glVertex2f(0.04 + x_offset, 0.87);   // Middle left
		glVertex2f(0.055 + x_offset, 0.89);  // Upper left bump
		glVertex2f(0.07 + x_offset, 0.89);   // Top left
		glVertex2f(0.085 + x_offset, 0.88);  // Middle vertex (slightly lower than the top)
		glVertex2f(0.10 + x_offset, 0.89);   // Top right
		glVertex2f(0.115 + x_offset, 0.89);  // Upper right bump
		glVertex2f(0.13 + x_offset, 0.87);   // Middle right

		glEnd();

		// Now draw the red heart using the same vertices
		glColor3f(1.0, 0.0, 0.0); // Red heart
		glBegin(GL_POLYGON);

		// Define the vertices to form the red heart shape
		glVertex2f(0.085 + x_offset, 0.83);  // Bottom middle vertex (the point of the heart)
		glVertex2f(0.04 + x_offset, 0.87);   // Middle left
		glVertex2f(0.055 + x_offset, 0.89);  // Upper left bump
		glVertex2f(0.07 + x_offset, 0.89);   // Top left
		glVertex2f(0.085 + x_offset, 0.88);  // Middle vertex (slightly lower than the top)
		glVertex2f(0.10 + x_offset, 0.89);   // Top right
		glVertex2f(0.115 + x_offset, 0.89);  // Upper right bump
		glVertex2f(0.13 + x_offset, 0.87);   // Middle right

		glEnd();
	}
}




Player player(0.7f, 0.05f, 0.1f, 0.1f);



void drawText(const char* text, float x, float y) {
	glColor3f(1.0, 1.0, 1.0); // White text
	glRasterPos2f(x, y);
	for (const char* c = text; *c != '\0'; ++c) {
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
	}
}

void drawScoreAndTime(int score, int timeLeft) {
	// Draw Score in the top middle
	char scoreStr[50];
	sprintf(scoreStr, "Score: %d", score);
	drawText(scoreStr, 1.5f, 0.85f);  // Adjusted position for visibility

	// Draw Time in the top right
	char timeStr[50];
	sprintf(timeStr, "Time: %d", timeLeft);
	drawText(timeStr, 2.8f, 0.85f);  // Adjusted position for visibility
}

int gameTime = 90; // Global variable for game time (2 minutes = 120 seconds)
int gameScore = 0;
float lastUpdateTime = 0.0f; // Global variable to track the last update time

int speedIncreaseInterval = 30; // Speed increases every 30 seconds
int lastSpeedIncreaseTime = 0; // Time when the speed was last increased



int gameState = 0;
int hearts = 5;
float speedTimer = 10.0f;

float invincibilityTimer = 10.0f; // Invincible for 10 seconds
float originalSpeed = speed;  // Save the original game speed
float speedRestoreTime = 0.0f; // Track when to restore the speed

void checkCollisionWithObstacle(Player& player, Obstacle& obstacle, int& hearts) {
	// Get the current time
	float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

	// Check if the player is invincible or if the obstacle recently hit the player
	if (player.invincible || obstacle.hitPlayer || hearts == 0 || gameState == 2) {
		return; // No collision if invincible or obstacle hitPlayer is active
	}

	// Define hitboxes for the screw head and body
	float headLeft = obstacle.x;
	float headRight = obstacle.x + obstacle.width * 0.2f;
	float headTop = obstacle.y + obstacle.height * 0.5f;
	float headBottom = obstacle.y - obstacle.height * 0.3f;

	float bodyLeft = obstacle.x - obstacle.width * 1.5f;
	float bodyRight = obstacle.x;
	float bodyTop = obstacle.y + obstacle.height * 0.2f;
	float bodyBottom = obstacle.y + obstacle.height * 0.05f;

	// Check collision for screw head
	bool headCollision = (player.x < headRight && player.x + player.width > headLeft &&
		player.y < headTop && player.y + player.height > headBottom);

	// Check collision for screw body
	bool bodyCollision = (player.x < bodyRight && player.x + player.width > bodyLeft &&
		player.y < bodyTop && player.y + player.height > bodyBottom);

	// If either part collides, it's considered a hit
	if (headCollision || bodyCollision) {
		hearts--; // Reduce hearts
		playCollisionSound(); // Play collision sound

		// Mark the obstacle as recently hit and store the timestamp
		obstacle.hitPlayer = true;
		obstacle.hitTime = currentTime;

		speed = 0.0f;
		speedRestoreTime = currentTime + 1.0f;  // Set the time to restore speed

		collectableTimer -= 1.0f;
		obstacleTimer -= 1.0f;
		powerUpTimer -= 1.0f;


		// Push the player back to the start of the obstacle
		player.x = bodyLeft - 0.1f;
		// Start the animation to move the obstacle back

	}
}




void checkCollisionWithCollectable(Player& player, Collectable& collectable, int& score, bool& shouldRemove) {
	// Check if the player's bounding box overlaps with the collectable (treat it as a circular area)
	float dx = (player.x + player.width * 0.5f) - collectable.x; // Center x-axis
	float dy = (player.y + player.height * 0.5f) - collectable.y; // Center y-axis
	float distance = sqrt(dx * dx + dy * dy); // Euclidean distance

	if (distance < player.width * 0.5f + collectable.radius) {
		// Collision detected
		score += 10000; // Increase score
		shouldRemove = true; // Mark collectable for removal
	}
}


void checkCollisionWithSpeedPowerUp(Player& player, PowerUp& powerup, float& speed, float& speedTimerStart, bool& shouldRemove) {
	if (powerup.isSpeedPowerUp) {
		// Check bounding box overlap with player
		if (player.x < powerup.x + powerup.size && player.x + player.width > powerup.x &&
			player.y < powerup.y + powerup.size && player.y + player.height > powerup.y) {
			// Collision detected
			speed *= 0.5f; // Cut speed by half
			speedTimerStart = glutGet(GLUT_ELAPSED_TIME) / 1000.0f; // Set timer for 15 seconds

			shouldRemove = true; // Mark power-up for removal
		}
	}
}



void checkCollisionWithInvincibilityPowerUp(Player& player, PowerUp& powerup, bool& invincible, float& invincibilityTimerStart, bool& shouldRemove) {
	if (!powerup.isSpeedPowerUp) {
		// Check bounding box overlap with player
		if (player.x < powerup.x + powerup.size && player.x + player.width > powerup.x &&
			player.y < powerup.y + powerup.size && player.y + player.height > powerup.y) {
			// Collision detected
			invincible = true; // Set invincible flag
			invincibilityTimerStart = glutGet(GLUT_ELAPSED_TIME) / 1000.0f; // Start the timer
			shouldRemove = true; // Mark power-up for removal
		}
	}
}






float speedTimerStart = 0.0f;         // Store the time when the speed power-up starts
float invincibilityTimerStart = 0.0f; // Store the time when the invincibility power-up starts


void updateTimer() {
	// Get the current time in milliseconds since the program started
	float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;  // Convert to seconds

	// Check if 1 second has passed since the last update
	if (currentTime - lastUpdateTime >= 1.0f) {
		if (gameTime > 0 && !player.invincible) {
			gameTime--;  // Decrement the game time by 1 second
		}
		// Increment the score

	  // Check if it's time to increase the speed
		if (gameTime % speedIncreaseInterval == 0 && gameTime != lastSpeedIncreaseTime) {
			speed += 0.005f;  // Increase the speed
			originalSpeed += 0.005f;
			lastSpeedIncreaseTime = gameTime;  // Update the last time speed was increased
		}

		lastUpdateTime = currentTime;  // Reset the last update time to the current time
	}

	if (hearts <= 0) {
		gameState = 1;  // You Lose
	}

	// Check if player wins (time is 0)
	if (gameTime == 0) {
		gameState = 2;  // You Win
	}


	// Handle the real-time invincibility power-up timer
	if (player.invincible) {
		if (currentTime - invincibilityTimerStart >= 10.0f) { // Check if 10 seconds have passed
			player.invincible = false;  // Remove invincibility
		}
	}

	// Collision detection for each obstacle
	for (auto& obstacle : obstacles) {
		checkCollisionWithObstacle(player, obstacle, hearts);
	}

	// Collision detection for each collectable
	for (auto& collectable : collectables) {
		bool shouldRemove = false;
		checkCollisionWithCollectable(player, collectable, gameScore, shouldRemove);
	}

	// Collision detection for each power-up
	for (auto& powerup : powerups) {
		bool shouldRemove = false;
		checkCollisionWithSpeedPowerUp(player, powerup, speed, speedTimerStart, shouldRemove);
		checkCollisionWithInvincibilityPowerUp(player, powerup, player.invincible, invincibilityTimerStart, shouldRemove);
	}
}







bool backgroundPlaying = false;
bool youDiedPlayed = false;
bool youWinPlayed = false;

void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	drawBoundariesAndDecorations();

	if (gameState == 0) { // Game is still playing
		// Draw player
		// Play background music if not already playing
		if (!backgroundPlaying) {
			playBackgroundMusic();
			backgroundPlaying = true;
		}
		player.draw();

		// Draw all obstacles
		for (auto& obs : obstacles) {
			obs.draw();
		}

		// Draw all collectables
		for (auto& col : collectables) {
			col.draw();
		}

		// Draw all power-ups
		for (auto& pwr : powerups) {
			pwr.draw();
		}

		player.update();

		// Draw health (hearts), score, and time
		drawHearts(hearts);
		drawScoreAndTime(gameScore, gameTime);  // Display score and time
	}
	else if (gameState == 1) { // You lose (YOU DIED)
		glColor3f(0.7, 0.0, 0.0); // Dark red color
		glRasterPos2f(1.5f, 0.5f);
		int finalScore = gameScore;
		char text[50] = "YOU DIED";
		for (const char* c = text; *c != '\0'; ++c) {
			glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);

		}
		if (backgroundPlaying) {
			stopBackgroundMusic();
			backgroundPlaying = false;
		}

		// Play "You Died" sound once
		if (!youDiedPlayed) {
			playYouDiedSound();
			youDiedPlayed = true;
		}

		char scoreStr[50];
		sprintf(scoreStr, "Score: %d", finalScore);
		drawText(scoreStr, 1.5f, 0.85f);

	}
	else if (gameState == 2) { // You win
		glColor3f(0.1, 0.4, 0.7); // Dark red color
		glRasterPos2f(1.5f, 0.5f);
		int finalScore = gameScore;
		char text[50] = "YOU WIN";
		for (const char* c = text; *c != '\0'; ++c) {
			glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);


		} // Centered "You Win" message
		char scoreStr[50];
		sprintf(scoreStr, "Score: %d", finalScore);
		drawText(scoreStr, 1.5f, 0.85f);
		if (backgroundPlaying) {
			stopBackgroundMusic();
			backgroundPlaying = false;
		}

		// Play "You Win" sound once
		if (!youWinPlayed) {
			playYouWinSound();
			youWinPlayed = true;
		}

	}

	glutSwapBuffers();
}



void init() {
	glClearColor(0.0, 0.0, 0.0, 1.0); // Set background color to black
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 3.0, 0.0, 1.0, -1.0, 1.0); // Set the orthographic projection
}

void update(int value) {
	player.update(); // Update the player state (jumping, ducking)

	// Redraw the scene
	glutPostRedisplay();

	// Call update again after 16 ms (~60 frames per second)
	glutTimerFunc(16, update, 0);
}

void updateGameObjects() {

	float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

	if (speed == 0.0f && currentTime >= speedRestoreTime) {
		speed = originalSpeed;  // Restore the speed
	}

	for (auto& obs : obstacles) {
		if (!obs.isAnimating)
			obs.move(speed); // Regular movement

		obs.updateHitStatus(currentTime); // Check and reset hitPlayer flag
	}
	// Move all power-ups
	for (auto& pwr : powerups) {
		pwr.move(speed);
	}

	// Move all collectables
	for (auto& col : collectables) {
		col.move(speed);
	}

	// Move all obstacles


	// Remove collided collectables
	collectables.erase(std::remove_if(collectables.begin(), collectables.end(), [&](Collectable& col) {
		bool shouldRemove = false;
		checkCollisionWithCollectable(player, col, gameScore, shouldRemove);
		return shouldRemove;  // If collision, remove this collectable
		}), collectables.end());

	// Remove collided power-ups
	powerups.erase(std::remove_if(powerups.begin(), powerups.end(), [&](PowerUp& pwr) {
		bool shouldRemove = false;
		checkCollisionWithSpeedPowerUp(player, pwr, speed, speedTimerStart, shouldRemove);
		checkCollisionWithInvincibilityPowerUp(player, pwr, player.invincible, invincibilityTimerStart, shouldRemove);
		return shouldRemove;  // If collision, remove this power-up
		}), powerups.end());

	updateTimers(0.016f); // Call timer for spawning logic
}




void timer(int) {
	updateGameObjects();  // Update the positions of all objects
	updateTimer();

	glutPostRedisplay();  // Request a redraw of the screen

	glutTimerFunc(16, timer, 0);  // Set up the next timer callback (16 ms for ~60 FPS)
}


void handleSpecialKeypress(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP: // Up arrow key
		player.jump(); // Call the jump method when up arrow is pressed
		break;
	case GLUT_KEY_DOWN: // Down arrow key
		player.duck(); // Call the duck method when down arrow is pressed
		break;
	default:
		break;
	}
}


int main(int argc, char** argv) {


	srand(static_cast<unsigned int>(time(0)));
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(1200, 800);
	glutInitWindowPosition(250, 0);

	glutCreateWindow("Geometry Dash el 8alaba");
	initOpenAL();
	std::thread soundThread(loadSoundInBackground);
	init();
	glutDisplayFunc(display);
	// Set the special keypress handler (for arrow keys)
	glutSpecialFunc(handleSpecialKeypress);

	// Start the update loop
	glutTimerFunc(16, update, 0);
	glutTimerFunc(16, timer, 0);
	glutMainLoop();
	soundThread.join();
	cleanupOpenAL();

}



