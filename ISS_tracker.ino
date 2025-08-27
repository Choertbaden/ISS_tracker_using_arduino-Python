/*
ISS Tracker Arduino Sketch
--------------------------
This sketch receives ISS latitude, longitude, and elevation data from Python
over serial and moves three servos (pan, tilt, elevation) to track the ISS.

Features:
- Handshake for "home" coordinates: Arduino ignores ISS data until home is set.
- Smooth movement for pan, tilt, and elevation.
- Debug output sent back over serial: pan, tilt, elevation, reverse flag.
*/

#include <Servo.h>

// ============================================================
// SERVO CONFIGURATION
// ============================================================
const int PAN_PIN      = 10;   // Horizontal rotation
const int TILT_PIN     = 6;    // Tilt (latitude)
const int ELEV_PIN     = 5;    // Elevation

Servo panServo;
Servo tiltServo;
Servo elevServo;

// ============================================================
// HOME COORDINATES (initialized to zero, set by Python handshake)
// ============================================================
float LON0 = 0.0;
float LAT0 = 0.0;

// ============================================================
// SERVO LIMITS
// ============================================================
const float LAT_MIN = -51.6;
const float LAT_MAX =  51.6;
const float TILT_MIN = 0;
const float TILT_MAX = 180;

const float PAN_MIN = 0;
const float PAN_MAX = 180;

const float ELEV_MIN = 0;
const float ELEV_MAX = 180;

// ============================================================
// SMOOTHING PARAMETERS
// ============================================================
float panSmooth  = 90.0;
float tiltSmooth = 90.0;
float elevSmooth = 0.0;  // start at 0
const float alpha = 0.12;

// ============================================================
// FLAGS FOR FIRST READ
// ============================================================
bool firstPanRead  = true;
bool firstTiltRead = true;
bool firstElevRead = true;

// ============================================================
// PREVIOUS STATE VARIABLES
// ============================================================
float prevDeltaLon = 0.0;
unsigned long prevTime = 0;

// ============================================================
// HOME HANDSHAKE FLAG
// ============================================================
bool homeSet = false;

// ============================================================
// HELPER FUNCTIONS
// ============================================================

// Clamp a float value between min and max
float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Map a value from one range to another
float fmap(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Convert latitude to servo tilt value
float latToServo(float lat) {
  float servoTarget = fmap(lat, LAT_MIN, LAT_MAX, 64.0, 167.0);
  return clampf(servoTarget, TILT_MIN, TILT_MAX);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);         // Start serial communication
  panServo.attach(PAN_PIN);   // Attach servos
  tiltServo.attach(TILT_PIN);
  elevServo.attach(ELEV_PIN);

  // Initialize servos to starting positions
  panServo.write(panSmooth);
  tiltServo.write(tiltSmooth);
  elevServo.write(elevSmooth);
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');  // Read incoming serial line
    line.trim();

    // ------------------------
    // HOME HANDSHAKE
    // ------------------------
    if (!homeSet && line.endsWith(",HOME")) {
      // Parse latitude and longitude from the "home" message
      int idx1 = line.indexOf(',');
      int idx2 = line.indexOf(',', idx1 + 1);
      LAT0 = line.substring(0, idx1).toFloat();
      LON0 = line.substring(idx1 + 1, idx2).toFloat();

      homeSet = true;                // Mark home as set
      Serial.println("HOME_OK");     // Confirm to Python script
      return;                        // Wait for next message
    }

    // Ignore ISS data until home is set
    if (!homeSet) return;

    // ------------------------
    // PARSE ISS DATA
    // ------------------------
    int firstComma  = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma  = line.indexOf(',', secondComma + 1);

    if (firstComma > 0 && secondComma > firstComma && thirdComma > secondComma) {
      float lat = line.substring(0, firstComma).toFloat();
      float lon = line.substring(firstComma + 1, secondComma).toFloat();
      float el  = line.substring(secondComma + 1, thirdComma).toFloat();
      unsigned long timestamp = line.substring(thirdComma + 1).toInt();

      // ------------------------
      // PAN (longitude) CALCULATION
      // ------------------------
      float deltaLon = lon - LON0;
      if(deltaLon > 180) deltaLon -= 360;
      if(deltaLon < -180) deltaLon += 360;

      float panTargetRaw;
      bool reverseMode = false;
      if(deltaLon >= -90 && deltaLon <= 90){
        panTargetRaw = 180 - deltaLon;
      } else {
        float mirroredLon;
        if(deltaLon < -90) mirroredLon = -90 - (deltaLon + 90);
        else mirroredLon = 90 - (deltaLon - 90);
        panTargetRaw = 180 - mirroredLon;
        reverseMode = true;
      }

      float panTarget = fmap(panTargetRaw, 90, 270, 0, 180);
      panTarget = clampf(panTarget, PAN_MIN, PAN_MAX);

      if(firstPanRead){
        panSmooth = panTarget;
        panServo.write(panTarget);
        firstPanRead = false;
      } else {
        panSmooth = alpha * panTarget + (1.0 - alpha) * panSmooth;
        if(reverseMode){
          panServo.write(panSmooth);
        } else {
          float currentPan = panServo.read();
          float dt = (prevTime > 0) ? (timestamp - prevTime) : 1.0;
          dt = max(dt, 0.001);
          float angularSpeed = abs(deltaLon - prevDeltaLon) / dt;
          float maxStep = angularSpeed * dt;
          float panStep = clampf(panSmooth - currentPan, -maxStep, maxStep);
          panServo.write(currentPan + panStep);
        }
      }
      prevDeltaLon = deltaLon;

      // ------------------------
      // TILT (latitude) CALCULATION
      // ------------------------
      float tiltTarget = latToServo(lat);
      if(firstTiltRead){
        tiltSmooth = tiltTarget;
        tiltServo.write(tiltSmooth);
        firstTiltRead = false;
      } else {
        tiltSmooth = alpha * tiltTarget + (1.0 - alpha) * tiltSmooth;
        tiltServo.write(tiltSmooth);
      }

      // ------------------------
      // ELEVATION CALCULATION
      // ------------------------
      if(firstElevRead){
        elevSmooth = clampf(el, ELEV_MIN, ELEV_MAX);
        elevServo.write(elevSmooth);
        firstElevRead = false;
      } else if(el > 0){
        float target = clampf(el, ELEV_MIN, ELEV_MAX);
        elevSmooth = alpha * target + (1.0 - alpha) * elevSmooth;
        elevServo.write(elevSmooth);
      } else {
        elevSmooth = 0;
        elevServo.write(elevSmooth);
      }

      prevTime = timestamp;

      // ------------------------
      // DEBUG OUTPUT TO PYTHON
      // ------------------------
      Serial.print(panServo.read());
      Serial.print(",");
      Serial.print(tiltServo.read());
      Serial.print(",");
      Serial.print(elevServo.read());
      Serial.print(",");
      Serial.println(reverseMode ? 1 : 0);
    }
  }
}
