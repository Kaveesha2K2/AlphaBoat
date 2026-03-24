// ===== L298 Motor Pins =====
// Left Motor
#include <Wire.h>
#include <MPU6050_light.h>

#include <VL53L0X.h>
#include <ESP32Servo.h>

#define SHT_LOX_LEFT 5
#define SHT_LOX_RIGHT 4
#define SHT_LOX_FRONT 33

struct SmoothServo {
  Servo servo;
  int pin;
  int currentAngle;
  int stepSize;
  int stepDelay;
};
SmoothServo Gateservo = {
  .pin = 2,
  .currentAngle = 0,
  .stepSize = 5,
  .stepDelay = 20
};

SmoothServo camservo = {
  .pin = 15,
  .currentAngle = 90,
  .stepSize = 2,
  .stepDelay = 3
};

VL53L0X sensorLeft;
VL53L0X sensorRight;
VL53L0X sensorFront;

MPU6050 mpu(Wire);
float currentZ;  // Current Yaw/Heading
float gyroZ;     // Current rotation speed

const int IN1 = 12;
const int IN2 = 14;
const int ENA = 13;  // PWM

// Right Motor
const int IN3 = 26;
const int IN4 = 25;
const int ENB = 27;  // PWM

int mode = 0;       // 0 = other mode, 1 = RC mode
int lastMode = -1;  // force first print
// Color pin mapping
#define RED_LED 19
#define GREEN_LED 18
#define BLUE_LED 23

#define BASE_SPEED 150  // forward speed
#define MAX_TURN 100    // limit steering correction

float Kp = 0.65;  // default
float Kd = 0.1;

TaskHandle_t sensorTaskHandle = NULL;

volatile int distLeft = 0;
volatile int distRight = 0;
volatile int distFront = 0;

SemaphoreHandle_t distMutex = NULL;

void rotateBoatByAngle(float angleDeg) {

  // Update IMU once before starting
  mpu.update();
  float startYaw = mpu.getAngleZ();

  int turnSpeed = 180;
  int direction = (angleDeg > 0) ? 1 : -1;

  // Start rotation
  driveMotors(-direction * turnSpeed, direction * turnSpeed);
  unsigned long startTime = millis();
  while (true) {
    mpu.update();

    float currentYaw = mpu.getAngleZ();
    float deltaYaw = currentYaw - startYaw;

    if (abs(deltaYaw) >= abs(angleDeg)) {
      break;
    }
    if (millis() - startTime > 4000) break;
    vTaskDelay(10 / portTICK_PERIOD_MS);  // small yield
  }

  driveMotors(0, 0);  // stop motors
}
void moveSmooth(SmoothServo& s, int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 180);

  if (!s.servo.attached()) {
    s.servo.attach(s.pin);
  }

  if (targetAngle > s.currentAngle) {
    for (int a = s.currentAngle; a <= targetAngle; a += s.stepSize) {
      s.servo.write(a);
      delay(s.stepDelay);
    }
  } else if (targetAngle < s.currentAngle) {
    for (int a = s.currentAngle; a >= targetAngle; a -= s.stepSize) {
      s.servo.write(a);
      delay(s.stepDelay);
    }
  }

  s.currentAngle = targetAngle;

  delay(100);
  s.servo.detach();  // ⭐ important
}
// ─── Configure Long Range ────────────────────────────────────────────────────
void configureLongRange(VL53L0X& sensor) {
  sensor.setTimeout(500);
  sensor.startContinuous();
}

// ─── TOF Initialization (separate function) ──────────────────────────────────
bool initTOFSensors() {
  Wire.begin();

  pinMode(SHT_LOX_LEFT, OUTPUT);
  pinMode(SHT_LOX_RIGHT, OUTPUT);
  pinMode(SHT_LOX_FRONT, OUTPUT);

  // Reset all sensors
  digitalWrite(SHT_LOX_LEFT, LOW);
  digitalWrite(SHT_LOX_RIGHT, LOW);
  digitalWrite(SHT_LOX_FRONT, LOW);
  delay(10);

  bool allOk = true;

  // --- Init Left Sensor ---
  digitalWrite(SHT_LOX_LEFT, HIGH);
  delay(10);
  if (sensorLeft.init()) {
    sensorLeft.setAddress(0x30);
    configureLongRange(sensorLeft);
    sensorLeft.startContinuous();
    Serial.println("Left sensor initialized");
  } else {
    Serial.println("Failed to detect Left sensor");
    allOk = false;
  }

  // --- Init Right Sensor ---
  digitalWrite(SHT_LOX_RIGHT, HIGH);
  delay(10);
  if (sensorRight.init()) {
    sensorRight.setAddress(0x31);
    configureLongRange(sensorRight);
    sensorRight.startContinuous();
    Serial.println("Right sensor initialized");
  } else {
    Serial.println("Failed to detect Right sensor");
    allOk = false;
  }

  // --- Init Front Sensor ---
  digitalWrite(SHT_LOX_FRONT, HIGH);
  delay(10);
  if (sensorFront.init()) {
    sensorFront.setAddress(0x32);
    configureLongRange(sensorFront);
    sensorFront.startContinuous();
    Serial.println("Front sensor initialized");
  } else {
    Serial.println("Failed to detect Front sensor");
    allOk = false;
  }

  return allOk;  // true = all 3 sensors OK, false = at least one failed
}

// ─── Safe Getter ─────────────────────────────────────────────────────────────
bool getDistances(int& left, int& right, int& front) {
  if (xSemaphoreTake(distMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    left = distLeft;
    right = distRight;
    front = distFront;
    xSemaphoreGive(distMutex);
    return true;
  }
  return false;
}

// ─── Sensor Task (Core 0) ────────────────────────────────────────────────────
void sensorTask(void* pvParameters) {

  // Initialize TOF sensors inside Core 0 task
  if (!initTOFSensors()) {
    Serial.println("WARNING: One or more TOF sensors failed to initialize!");
  }

  while (true) {
    int newLeft = sensorLeft.readRangeContinuousMillimeters();
    int newRight = sensorRight.readRangeContinuousMillimeters();
    int newFront = sensorFront.readRangeContinuousMillimeters();

    bool leftOk = !sensorLeft.timeoutOccurred();
    bool rightOk = !sensorRight.timeoutOccurred();
    bool frontOk = !sensorFront.timeoutOccurred();

    if (xSemaphoreTake(distMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (leftOk) distLeft = newLeft;
      if (rightOk) distRight = newRight;
      if (frontOk) distFront = newFront;
      xSemaphoreGive(distMutex);
    }

    if (!leftOk) Serial.println("Left Timeout!");
    if (!rightOk) Serial.println("Right Timeout!");
    if (!frontOk) Serial.println("Front Timeout!");

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
void setup() {
  Serial.begin(115200);
  delay(2000);

  // HC-12 on Serial2
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Serial2 initialized");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  Gateservo.servo.attach(Gateservo.pin);
  camservo.servo.attach(camservo.pin);

  Gateservo.servo.write(Gateservo.currentAngle);
  camservo.servo.write(camservo.currentAngle);
  Wire.begin();

  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  mpu.setAccOffsets(-0.04, -0.11, -0.12);
  mpu.setGyroOffsets(-0.45, -1.63, -0.55);

  distMutex = xSemaphoreCreateMutex();
  if (distMutex == NULL) {
    Serial.println("FATAL: Failed to create mutex!");
    while (true)
      ;
  }

  xTaskCreatePinnedToCore(
    sensorTask,
    "SensorTask",
    4096,
    NULL,
    1,
    &sensorTaskHandle,
    0  // Core 0
  );
}

void loop() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    data.trim();

    // ---------- MODE UPDATE ----------
    if (data.startsWith("BTN:")) {
      mode = data.substring(4).toInt();

      // In sensorTask - BTN handler
      if (mode != lastMode) {
        if (mode == 1) {
          Serial.println("MODE:RC mode");  // ← NEW
        } else {
          Serial.println("MODE:Auto mode");  // ← NEW
        }
        lastMode = mode;
      }
      return;
    }

    // ---------- PD TUNING (ANY MODE) ----------
    if (data.startsWith("kp:") || data.startsWith("kd:")) {
      handlePD(data);
      return;
    }

    // ---------- RC MODE ----------
    if (mode == 1) {
      handleRC(data);
    }
  }
  // ---------- OTHER MODE ----------
  if (mode == 0) {
    otherModeLoop();
  }
}
int lastPixelError = 0;
unsigned long lastPDTime = 0;
// ===== RC CONTROL =====
void handleRC(String data) {
  int xVal = 0, yVal = 0;
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  if (sscanf(data.c_str(), "X:%d,Y:%d", &xVal, &yVal) == 2) {
    digitalWrite(RED_LED, HIGH);
    int leftSpeed = xVal + yVal;
    int rightSpeed = xVal - yVal;

    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);
    Serial.print("Left: ");
    Serial.print(leftSpeed);
    Serial.print(" | Right: ");
    Serial.println(rightSpeed);
    driveMotors(leftSpeed, rightSpeed);
  }
}

// ===== PLACEHOLDER FOR OTHER MODE =====
int serial_Timout = 4000;
unsigned long last_Serial_Time = 0;
void otherModeLoop() {

  digitalWrite(RED_LED, LOW);
  digitalWrite(BLUE_LED, HIGH);

  mpu.update();
  gyroZ = mpu.getGyroZ();
  if (getDistances(left, right, front)) {

    Serial.print("Left: ");
    Serial.print(left);
    Serial.print("mm\tRight: ");
    Serial.print(right);
    Serial.print("mm\tFront: ");
    Serial.print(front);
    Serial.println("mm");

    // Example thresholds
    // Side threshold -> distance to detect wall too close
    // Front threshold -> larger because collision risk is higher
    // (Front threshold usually > side threshold)
    int SIDE_THRESHOLD = 120;   // mm
    int FRONT_THRESHOLD = 200;  // mm


    if (left < SIDE_THRESHOLD && right > SIDE_THRESHOLD) {  // -------------------------------
      // CASE 1: Only LEFT sensor below threshold
      // Robot should rotate slightly to the RIGHT to move away
      rotateBoatByAngle(-30);  //+ means ccw rotation
    }
    if (right < SIDE_THRESHOLD && left > SIDE_THRESHOLD) {  // -------------------------------
                                                            // CASE 2: Only RIGHT sensor below threshold
                                                            // Robot should rotate slightly to the LEFT
      rotateBoatByAngle(30);                                //+ means ccw rotation
    }
    if (left < SIDE_THRESHOLD && right < SIDE_THRESHOLD) {  // -------------------------------
      // CASE 3: BOTH SIDE sensors below threshold
      // -------------------------------
      // Robot is between two close walls.
      // This likely indicates a very narrow corridor
      // Possible actions:
      // - stop
      // - reverse
      // - rotate and find another path
      driveMotors(-150, -150);
      delay(1000);
      rotateBoatByAngle(180);
    }


    if (front < FRONT_THRESHOLD) {  // -------------------------------
      // CASE 4: FRONT sensor below threshold
      // -------------------------------
      // There is an obstacle directly ahead.
      // Robot must avoid collision.
      //
      // Possible actions:
      // - stop immediately
      // - turn toward the side with larger distance
      // - rotate until a clear path is found
      driveMotors(-150, -150);
      delay(2500);
      if (right < left) rotateBoatByAngle(90);
      else rotateBoatByAngle(-90);
    }
  }
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();

    if (msg == "No target") {
      digitalWrite(GREEN_LED, LOW);
      driveMotors(0, 0);
      Serial.println("No target Found");
      return;
    } else if (msg.startsWith("Pi Started")) {
      driveMotors(0, 0);
      if (mode) Serial.println("MODE:RC mode");  // ← NEW
      else Serial.println("MODE:Auto mode");     // ← NEW
    } else if (msg == "Close Gate") {
      if (Gateservo.currentAngle != 0) moveSmooth(Gateservo, 0);
      Serial.println("Gate Closed");
      return;
    } else if (msg.startsWith("Rotate Cam:")) {
      int targetAngle = 0;
      int prevAngle = 0;
      if (msg.indexOf('&') != -1) {
        sscanf(msg.c_str(), "Rotate Cam:%d &%d", &targetAngle, &prevAngle);
        moveSmooth(camservo, 90 + targetAngle);
        Serial.println("Cam rotated to: " + String(targetAngle));
        // Rotate boat using IMU
        rotateBoatByAngle(prevAngle - (prevAngle / abs(prevAngle)) * 45);

        Serial.println("Boat rotated by " + String(prevAngle));
      } else {
        sscanf(msg.c_str(), "Rotate Cam:%d", &targetAngle);

        //rotateBoatByAngle(targetAngle);
        moveSmooth(camservo, 90 + targetAngle);
        Serial.println("Cam rotated to: " + String(targetAngle));
      }
    } else if (msg == "Move Forward") {
      driveMotors(150, 150);
      Serial.println("Moved Forward");
      delay(1000);
    }
    if (msg.startsWith("Target Acquired!")) {
      moveSmooth(Gateservo, 100);
      digitalWrite(GREEN_LED, HIGH);
      Serial.println("Gate Opened");
      int pixelError = 0;
      // Add these globals at the top

      // Inside otherModeLoop(), replace the PD block:
      if (sscanf(msg.c_str(), "Target Acquired!: %d", &pixelError) == 1) {

        unsigned long now = millis();
        float dt = (now - lastPDTime) / 1000.0f;  // convert to seconds
        if (dt <= 0 || dt > 0.5f) dt = 0.05f;     // clamp on first run or lag spike

        float pTerm = pixelError * Kp;
        float dTerm = ((pixelError - lastPixelError) / dt) * Kd;

        int turn = pTerm + dTerm;  // ← note: + not - now

        // ... rest of motor code
        int leftSpeed = BASE_SPEED - turn;
        int rightSpeed = BASE_SPEED + turn;
        int minspeed = 120;
        leftSpeed = constrain(leftSpeed, -255, 255);
        rightSpeed = constrain(rightSpeed, -255, 255);
        if (leftSpeed != 0 && abs(leftSpeed) < minspeed) {
          leftSpeed = minspeed * (leftSpeed > 0 ? 1 : -1);
        }

        if (rightSpeed != 0 && abs(rightSpeed) < minspeed) {
          rightSpeed = minspeed * (rightSpeed > 0 ? 1 : -1);
        }
        Serial.println("Pixel error: " + String(pixelError) + " | L: " + String(leftSpeed) + " | R: " + String(rightSpeed));
        driveMotors(leftSpeed, rightSpeed);
        lastPixelError = pixelError;
        lastPDTime = now;
      }
    }
    last_Serial_Time = millis();
  } else {
    if (!mode && millis() - last_Serial_Time >= serial_Timout) {
      driveMotors(0, 0);
      if (camservo.currentAngle != 90) {
        moveSmooth(camservo, 90);
      }
      if (Gateservo.currentAngle != 0) {
        moveSmooth(Gateservo, 0);
      }
      digitalWrite(GREEN_LED, LOW);
    }
  }
}

// ===== MOTOR CONTROL =====
void driveMotors(int left, int right) {

  // Left Motor
  digitalWrite(IN1, left > 0);
  digitalWrite(IN2, left < 0);

  // Right Motor
  digitalWrite(IN3, right > 0);
  digitalWrite(IN4, right < 0);

  analogWrite(ENA, abs(left));
  analogWrite(ENB, abs(right));
}
void handlePD(String data) {
  if (data.startsWith("kp:")) {
    Kp = data.substring(3).toFloat();
    Serial.print("Kp updated: ");
    Serial.println(Kp);
    return;
  }

  if (data.startsWith("kd:")) {
    Kd = data.substring(3).toFloat();
    Serial.print("Kd updated: ");
    Serial.println(Kd);
    return;
  }
}
