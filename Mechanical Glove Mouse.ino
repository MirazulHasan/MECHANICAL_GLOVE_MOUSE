#include "BleMouse.h"    // Include the Bluetooth Mouse library
#include <Wire.h>
#include <MPU6050.h>

// Create a BluetoothMouse object
BleMouse bleMouse;

// MPU6050 object
MPU6050 gyro;

// Pins for buttons
const int buttonLeftPin = 4;  // GPIO pin for left-click button
const int buttonRightPin = 5; // GPIO pin for right-click button
const int buttonDisablePin = 6; // GPIO pin for disabling movement while held

// Sensitivity for cursor movement
float sensitivity = 0.5;  // Adjust this value to control cursor speed

// Time tracking for integration
unsigned long lastUpdateTime = 0;

// Variables to store gyroscope offsets (calibration)
float gyroOffsetX = 0, gyroOffsetY = 0, gyroOffsetZ = 0;

// Variables to store smoothed gyroscope data
float smoothedGyr = 0;
float smoothedGzr = 0;

// Weight for weighted average (between 0 and 1)
const float weight = 0.7;  // Higher value gives more importance to the latest data

// Variables to track button hold states
bool leftButtonHeld = false;
bool rightButtonHeld = false;

void setup() {
  // Initialize I2C and MPU6050
  Wire.begin();
  gyro.initialize();

  // Check MPU6050 connection
  if (!gyro.testConnection()) {
    while (1);  // Halt execution if MPU6050 is not connected
  }

  // Calibrate gyroscope to eliminate drift
  calibrateGyroscope();

  // Begin Bluetooth Mouse
  bleMouse.begin();

  // Initialize button pins with pull-up resistors
  pinMode(buttonLeftPin, INPUT_PULLUP);
  pinMode(buttonRightPin, INPUT_PULLUP);
  pinMode(buttonDisablePin, INPUT_PULLUP);
}

void loop() {
  if (bleMouse.isConnected()) {
    // Read button states
    bool leftButtonPressed = digitalRead(buttonLeftPin) == LOW;
    bool rightButtonPressed = digitalRead(buttonRightPin) == LOW;
    bool disableActive = digitalRead(buttonDisablePin) == LOW;

    // Handle left button hold for selection
    if (leftButtonPressed) {
      if (!leftButtonHeld) {
        bleMouse.press(MOUSE_LEFT); // Start holding the left button
        leftButtonHeld = true;
      }
    } else if (leftButtonHeld) {
      bleMouse.release(MOUSE_LEFT); // Release the left button
      leftButtonHeld = false;
    }

    // Handle right button hold for context menu
    if (rightButtonPressed) {
      if (!rightButtonHeld) {
        bleMouse.press(MOUSE_RIGHT); // Start holding the right button
        rightButtonHeld = true;
      }
    } else if (rightButtonHeld) {
      bleMouse.release(MOUSE_RIGHT); // Release the right button
      rightButtonHeld = false;
    }

    // If disable button is pressed, skip movement
    if (disableActive) {
      resetGyroData(); // Reset gyroscope data while disabling movement
    } else {
      // Handle cursor motion if disable button is NOT pressed
      unsigned long currentTime = millis();
      float deltaTime = (currentTime - lastUpdateTime) / 1000.0; // Convert ms to seconds
      lastUpdateTime = currentTime;

      // Read gyroscope data
      int16_t gx, gy, gz;
      gyro.getRotation(&gx, &gy, &gz);

      // Apply calibration offsets
      float gyr = (gy / 131.0) - gyroOffsetY;  // Pitch (rotation around Y-axis)
      float gzr = (gz / 131.0) - gyroOffsetZ;  // Yaw (rotation around Z-axis)

      // Apply weighted average for smoothing
      smoothedGyr = (weight * gyr) + ((1 - weight) * smoothedGyr);
      smoothedGzr = (weight * gzr) + ((1 - weight) * smoothedGzr);

      // For 90-degree horizontal orientation:
      // - Use `smoothedGyr` for horizontal cursor movement
      // - Use `smoothedGzr` for vertical cursor movement
      int deltaX = smoothedGyr * sensitivity * deltaTime * 100; // Pitch controls horizontal movement
      int deltaY = smoothedGzr * sensitivity * deltaTime * 100; // Yaw controls vertical movement

      // Apply thresholds to filter small movements
      if (abs(deltaX) < 1) deltaX = 0;
      if (abs(deltaY) < 1) deltaY = 0;

      // Only move the cursor if there is significant motion
      if (deltaX != 0 || deltaY != 0) {
        bleMouse.move(-deltaX, -deltaY);  // Invert deltaY for natural movement
      }
    }

    delay(10);  // Small delay for smoother operation
  } else {
    delay(1000);  // Check every second if the Bluetooth mouse is connected
  }
}

// Function to calibrate gyroscope
void calibrateGyroscope() {
  const int calibrationSamples = 100;
  long sumX = 0, sumY = 0, sumZ = 0;

  for (int i = 0; i < calibrationSamples; i++) {
    int16_t gx, gy, gz;
    gyro.getRotation(&gx, &gy, &gz);
    sumX += gx;
    sumY += gy;
    sumZ += gz;
    delay(10);
  }

  // Calculate average offsets
  gyroOffsetX = sumX / calibrationSamples / 131.0;
  gyroOffsetY = sumY / calibrationSamples / 131.0;
  gyroOffsetZ = sumZ / calibrationSamples / 131.0;
}

// Function to reset gyroscope data
void resetGyroData() {
  smoothedGyr = 0;
  smoothedGzr = 0;
}
