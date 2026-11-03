/*
  Arduino LDR Light Tracker with Servo Control (Smooth Continuous Tracking with Cooldown)
  4 LDRs detect light direction and control 2 servos
  Smoothly transitions when hitting horizontal limits for continuous tracking
  5-second cooldown between flips
  
  LDR Connections (with 10k resistor to ground):
  - Top Left (TL):     A0
  - Bottom Left (BL):  A1
  - Top Right (TR):    A2
  - Bottom Right (BR): A3
  
  Servo Connections:
  - Vertical Servo:    Pin 9
  - Horizontal Servo:  Pin 10
*/

#include <Servo.h>  // Include Servo library for controlling servo motors

// Define analog input pins for the four LDR sensors
#define TL_PIN A0   // Top Left LDR sensor
#define BL_PIN A1   // Bottom Left LDR sensor
#define TR_PIN A2   // Top Right LDR sensor
#define BR_PIN A3   // Bottom Right LDR sensor

Servo VERTservo;  // Create vertical servo object
Servo HORservo;   // Create horizontal servo object

// System configuration constants
#define THRESHOLD_PERCENT 10.0  // Minimum percent difference to trigger servo movement
#define STEP_SIZE 1             // Servo movement increment in degrees
#define DELAY_TIME 50           // Loop delay in milliseconds
#define INVERT_THRESHOLD 90     // Vertical angle threshold to detect upside-down orientation
#define HOR_MIN 5               // Horizontal servo minimum angle limit
#define HOR_MAX 175             // Horizontal servo maximum angle limit
#define FLIP_STEP 2             // Servo movement increment during flip animation
#define FLIP_COOLDOWN 5000      // Cooldown period between flips in milliseconds

int vertPos = 90;  // Current vertical servo position in degrees
int horPos = 90;   // Current horizontal servo position in degrees

unsigned long lastFlipTime = 0;  // Timestamp of the last flip operation

// Calibration multipliers for each LDR sensor
float top_left_cal = 1.0;      // Top left LDR calibration factor
float bottom_left_cal = 1.0;   // Bottom left LDR calibration factor
float top_right_cal = 1.0;     // Top right LDR calibration factor
float bottom_right_cal = 1.0;  // Bottom right LDR calibration factor

void setup() {
  VERTservo.attach(9);   // Attach vertical servo to pin 9
  HORservo.attach(10);   // Attach horizontal servo to pin 10
  
  VERTservo.write(90);  // Initialize vertical servo to center position
  HORservo.write(90);   // Initialize horizontal servo to center position
  
  delay(1000);  // Wait for servos to reach initial position
}

void smoothFlipServos() {
  int targetVert = 180 - vertPos;  // Calculate target vertical position (180-degree flip)
  int targetHor = 180 - horPos;    // Calculate target horizontal position (180-degree flip)
  
  // Smoothly move both servos to their target positions
  while (horPos != targetHor || vertPos != targetVert) {
    bool moved = false;  // Track if any servo moved this iteration
    
    // Increment horizontal servo toward target position
    if (horPos < targetHor) {
      horPos += FLIP_STEP;              // Move servo clockwise
      if (horPos > targetHor) horPos = targetHor;  // Prevent overshoot
      moved = true;  // Mark that movement occurred
    } else if (horPos > targetHor) {
      horPos -= FLIP_STEP;              // Move servo counterclockwise
      if (horPos < targetHor) horPos = targetHor;  // Prevent overshoot
      moved = true;  // Mark that movement occurred
    }
    
    // Increment vertical servo toward target position
    if (vertPos < targetVert) {
      vertPos += FLIP_STEP;              // Move servo up
      if (vertPos > targetVert) vertPos = targetVert;  // Prevent overshoot
      moved = true;  // Mark that movement occurred
    } else if (vertPos > targetVert) {
      vertPos -= FLIP_STEP;              // Move servo down
      if (vertPos < targetVert) vertPos = targetVert;  // Prevent overshoot
      moved = true;  // Mark that movement occurred
    }
    
    HORservo.write(horPos);   // Update horizontal servo position
    VERTservo.write(vertPos); // Update vertical servo position
    
    delay(20);  // Delay for smooth animation
  }
  
  lastFlipTime = millis();  // Record timestamp of completed flip
  
  delay(100);  // Brief pause after flip completes
}

bool canFlip() {
  unsigned long currentTime = millis();  // Get current time
  unsigned long timeSinceLastFlip = currentTime - lastFlipTime;  // Calculate elapsed time since last flip
  
  if (timeSinceLastFlip >= FLIP_COOLDOWN) {
    return true;  // Cooldown period has elapsed, flip allowed
  } else {
    return false;  // Still in cooldown period, flip not allowed
  }
}

void loop() {
  float top_left = analogRead(TL_PIN) * top_left_cal;        // Read and calibrate top left LDR
  float bottom_left = analogRead(BL_PIN) * bottom_left_cal;  // Read and calibrate bottom left LDR
  float top_right = analogRead(TR_PIN) * top_right_cal;      // Read and calibrate top right LDR
  float bottom_right = analogRead(BR_PIN) * bottom_right_cal;// Read and calibrate bottom right LDR
  
  float left_sum = top_left + bottom_left;    // Sum of left side LDR values
  float right_sum = top_right + bottom_right; // Sum of right side LDR values
  
  float top_sum = top_left + top_right;       // Sum of top LDR values
  float bottom_sum = bottom_left + bottom_right;  // Sum of bottom LDR values
  
  bool isInverted = (vertPos > INVERT_THRESHOLD);  // Check if tracker is upside-down
  
  // ===== HORIZONTAL SERVO CONTROL =====
  float x_avg = (left_sum + right_sum) / 2.0;  // Calculate average horizontal light intensity
  if (x_avg > 0) {  // Ensure non-zero to avoid division by zero
    float x_diff_percent = (abs(left_sum - right_sum) / x_avg) * 100.0;  // Calculate percent difference between left and right
    
    if (x_diff_percent > THRESHOLD_PERCENT) {  // Check if difference exceeds threshold
      bool moveRight = (right_sum > left_sum);  // Determine base movement direction
      
      if (isInverted) {
        moveRight = !moveRight;  // Reverse direction if tracker is inverted
      }
      
      if (moveRight) {
        horPos += STEP_SIZE;  // Increment horizontal position to move right
      } else {
        horPos -= STEP_SIZE;  // Decrement horizontal position to move left
      }
      
      // Check if horizontal servo has reached physical limits
      if (horPos >= HOR_MAX || horPos <= HOR_MIN) {
        horPos = constrain(horPos, HOR_MIN, HOR_MAX);  // Constrain position to limits
        HORservo.write(horPos);  // Update servo position
        
        if (canFlip()) {  // Check if cooldown period has elapsed
          smoothFlipServos();  // Execute smooth flip to continue tracking
        }
      } else {
        HORservo.write(horPos);  // Update horizontal servo position normally
      }
    }
  }
  
  // ===== VERTICAL SERVO CONTROL =====
  float y_avg = (top_sum + bottom_sum) / 2.0;  // Calculate average vertical light intensity
  if (y_avg > 0) {  // Ensure non-zero to avoid division by zero
    float y_diff_percent = (abs(top_sum - bottom_sum) / y_avg) * 100.0;  // Calculate percent difference between top and bottom
    
    if (y_diff_percent > THRESHOLD_PERCENT) {  // Check if difference exceeds threshold
      if (top_sum > bottom_sum) {
        vertPos += STEP_SIZE;  // Increment vertical position to move up
      } else {
        vertPos -= STEP_SIZE;  // Decrement vertical position to move down
      }
      vertPos = constrain(vertPos, 50, 140);  // Constrain vertical position to safe operating range
      VERTservo.write(vertPos);  // Update vertical servo position
    }
  }
  
  delay(DELAY_TIME);  // Wait before next loop iteration
}
