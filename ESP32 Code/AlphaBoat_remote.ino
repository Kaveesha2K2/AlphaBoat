// ESP32: HC-12 + Joystick + Toggle Button + PID Params

const int JOY_X_PIN = 13;  // ADC pin
const int JOY_Y_PIN = 14;  // ADC pin
const int BTN_PIN = 27;    // Toggle button (to GND)

bool btnState = false;
bool lastBtnReading = false;
bool toggleState = false;  // 0 = auto, 1 = manual

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// PID parameters
float Kp = 0.0;
float Kd = 0.0;
int led = 15;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Ready. Send: kp:-1.00 or kd:0.50");

  // HC-12 UART
  Serial1.begin(115200, SERIAL_8N1, 16, 17);  // RX=16 TX=17

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(led, OUTPUT);
}

void loop() {

  // ---------- Joystick (Manual Mode) ----------
  if (toggleState) {
    int rawX = analogRead(JOY_X_PIN);
    int rawY = analogRead(JOY_Y_PIN);

    int joyX = map(rawX, 0, 4095, -255, 255);
    int joyY = map(rawY, 0, 4095, -255, 255);

    if (abs(joyX) < 23) joyX = 0;
    if (abs(joyY) < 23) joyY = 0;

    Serial1.print("X:");
    Serial1.print(joyX);
    Serial1.print(",Y:");
    Serial1.println(joyY);
  }

  // ---------- Toggle Button ----------
  bool reading = !digitalRead(BTN_PIN);  // active LOW

  if (reading != lastBtnReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != btnState) {
      btnState = reading;

      if (btnState) {
        toggleState = !toggleState;
        digitalWrite(led, toggleState);
        Serial1.print("BTN:");
        Serial1.println(toggleState);

        Serial.print("BTN TOGGLE: ");
        Serial.println(toggleState);
      }
    }
  }

  lastBtnReading = reading;

  // ---------- Serial Monitor PID Input ----------
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();  // remove \r and spaces

    if (cmd.startsWith("kp:")) {
      Kp = cmd.substring(3).toFloat();

      Serial.print("Kp set to: ");
      Serial.println(Kp);

      Serial1.print("kp:");
      Serial1.println(Kp, 2);  // send via HC-12
    } else if (cmd.startsWith("kd:")) {
      Kd = cmd.substring(3).toFloat();

      Serial.print("Kd set to: ");
      Serial.println(Kd);

      Serial1.print("kd:");
      Serial1.println(Kd, 2);
    } else {
      Serial.println("Invalid command");
    }
  }

  delay(20);
}
