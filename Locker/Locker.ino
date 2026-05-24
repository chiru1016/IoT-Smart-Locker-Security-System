#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <ESP_Mail_Client.h>

// ================= WIFI DETAILS =================
#define WIFI_SSID "Chiru"
#define WIFI_PASSWORD "Shri1016"

// ================= FIREBASE URL =================
// Do not put / at the end
#define FIREBASE_URL "https://smartlocker-d62fd-default-rtdb.asia-southeast1.firebasedatabase.app"

// ================= EMAIL DETAILS =================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

// Sender email details
#define AUTHOR_EMAIL "goophish2005@gmail.com"#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <ESP_Mail_Client.h>

// ================= WIFI DETAILS =================
#define WIFI_SSID "Chiru"
#define WIFI_PASSWORD "Shri1016"

// ================= FIREBASE URL =================
// Do not put / at the end
#define FIREBASE_URL "https://smartlocker-d62fd-default-rtdb.asia-southeast1.firebasedatabase.app"

// ================= EMAIL DETAILS =================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define AUTHOR_EMAIL "goophish2005@gmail.com"
#define AUTHOR_PASSWORD "suaersasqqpgjeov"

#define RECIPIENT_EMAIL "chiru4646111@gmail.com"

// ================= PINS =================
#define SERVO_PIN 18
#define BUZZER_PIN 23
#define PIR_PIN 19

// ================= SERVO ANGLES =================
// If servo works opposite, swap these values
#define LOCK_ANGLE 0
#define UNLOCK_ANGLE 90

Servo lockerServo;
SMTPSession smtp;

// Locker password
String correctPassword = "1234";

int wrongAttempts = 0;
bool lockerLocked = true;

// Firebase checking time
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 1500;

// PIR movement email cooldown
unsigned long lastMovementAlertTime = 0;
const unsigned long movementCooldown = 30000; // 30 seconds

void smtpCallback(SMTP_Status status);

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW);

  lockerServo.attach(SERVO_PIN);

  connectWiFi();

  smtp.callback(smtpCallback);

  lockLocker();

  updateFirebaseInt("attempts", 0);
  updateFirebaseString("command", "");
  updateFirebaseString("entered_password", "");
  updateFirebaseString("movement", "No Movement");

  Serial.println("System Started");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Check Firebase command every 1.5 seconds
  if (millis() - lastCheckTime >= checkInterval) {
    lastCheckTime = millis();

    String command = getFirebaseString("command");

    if (command == "CHECK_PASSWORD") {
      String enteredPassword = getFirebaseString("entered_password");

      checkPassword(enteredPassword);

      updateFirebaseString("command", "");
      updateFirebaseString("entered_password", "");
    }

    if (command == "LOCK") {
      lockLocker();

      updateFirebaseString("command", "");
    }
  }

  // PIR movement checking
  checkMovementInsideLocker();
}

// ================= WIFI FUNCTION =================

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================= PASSWORD CHECK =================

void checkPassword(String enteredPassword) {
  enteredPassword.trim();

  Serial.print("Entered Password: ");
  Serial.println(enteredPassword);

  if (enteredPassword == correctPassword) {
    wrongAttempts = 0;

    unlockLocker();

    updateFirebaseInt("attempts", wrongAttempts);

    Serial.println("Access Granted");
  } 
  else {
    wrongAttempts++;

    updateFirebaseString("status", "Wrong Password");
    updateFirebaseInt("attempts", wrongAttempts);

    beepBuzzer(2);

    Serial.println("Wrong Password");

    if (wrongAttempts >= 3) {
      updateFirebaseString("status", "SECURITY ALERT - 3 Wrong Password Attempts");

      digitalWrite(BUZZER_PIN, HIGH);

      sendWrongPasswordEmailAlert();

      delay(3000);

      digitalWrite(BUZZER_PIN, LOW);

      wrongAttempts = 0;
      updateFirebaseInt("attempts", wrongAttempts);
    }
  }
}

// ================= MOVEMENT CHECK FUNCTION =================

void checkMovementInsideLocker() {
  int motionState = digitalRead(PIR_PIN);

  // Movement alert only when locker is locked
  if (lockerLocked == true && motionState == HIGH) {
    Serial.println("Movement detected inside locked locker!");

    updateFirebaseString("status", "ALERT - Movement Inside Locked Locker");
    updateFirebaseString("movement", "Movement Detected");

    digitalWrite(BUZZER_PIN, HIGH);

    // Send email only once every 30 seconds
    if (millis() - lastMovementAlertTime > movementCooldown) {
      sendMovementEmailAlert();
      lastMovementAlertTime = millis();
    }

    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else {
    // When locker is unlocked, buzzer should always be OFF
    if (lockerLocked == false) {
      digitalWrite(BUZZER_PIN, LOW);
    }

    if (lockerLocked == true && motionState == LOW) {
      updateFirebaseString("movement", "No Movement");
    }
  }
}

// ================= SERVO FUNCTIONS =================

void lockLocker() {
  lockerServo.write(LOCK_ANGLE);
  lockerLocked = true;

  digitalWrite(BUZZER_PIN, LOW);

  updateFirebaseString("status", "Locker Locked - Monitoring Started");
  updateFirebaseString("movement", "No Movement");

  Serial.println("Locker Locked - Monitoring Started");
}

void unlockLocker() {
  lockerServo.write(UNLOCK_ANGLE);
  lockerLocked = false;

  digitalWrite(BUZZER_PIN, LOW);

  updateFirebaseString("status", "Access Granted - Locker Unlocked");
  updateFirebaseString("movement", "Locker Unlocked");

  Serial.println("Locker Unlocked - Buzzer OFF");
}

// ================= BUZZER FUNCTION =================

void beepBuzzer(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

// ================= FIREBASE GET =================

String getFirebaseString(String tag) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";

  http.begin(url);

  int httpCode = http.GET();

  String payload = "";

  if (httpCode > 0) {
    payload = http.getString();

    payload.replace("\"", "");
    payload.trim();

    Serial.println("Firebase GET " + tag + ": " + payload);
  } 
  else {
    Serial.println("Firebase GET failed for " + tag);
  }

  http.end();

  return payload;
}

// ================= FIREBASE UPDATE STRING =================

void updateFirebaseString(String tag, String value) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";
  String data = "\"" + value + "\"";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(data);

  if (httpCode > 0) {
    Serial.println("Firebase updated: " + tag + " = " + value);
  } 
  else {
    Serial.println("Firebase update failed for " + tag);
  }

  http.end();
}

// ================= FIREBASE UPDATE INTEGER =================

void updateFirebaseInt(String tag, int value) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";
  String data = String(value);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(data);

  if (httpCode > 0) {
    Serial.println("Firebase updated: " + tag + " = " + String(value));
  } 
  else {
    Serial.println("Firebase update failed for " + tag);
  }

  http.end();
}

// ================= WRONG PASSWORD EMAIL ALERT =================

void sendWrongPasswordEmailAlert() {
  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;

  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "ESP32 Smart Locker";
  message.sender.email = AUTHOR_EMAIL;

  message.subject = "SECURITY ALERT: Wrong Password Attempts";
  message.addRecipient("Chiru", RECIPIENT_EMAIL);

  String body = "Security Alert!\n\n";
  body += "Three wrong password attempts were detected on the smart locker.\n\n";
  body += "Locker Status: Locked\n";
  body += "Alert Type: Wrong password attempts\n\n";
  body += "Please check the locker immediately.\n\n";
  body += "Device: IoT Smart Locker Security System";

  message.text.content = body.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&config)) {
    Serial.println("SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Wrong password email sending failed");
    Serial.println(smtp.errorReason());
  } 
  else {
    Serial.println("Wrong password alert email sent successfully");
  }

  smtp.closeSession();
}

// ================= MOVEMENT EMAIL ALERT =================

void sendMovementEmailAlert() {
  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;

  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "ESP32 Smart Locker";
  message.sender.email = AUTHOR_EMAIL;

  message.subject = "SECURITY ALERT: Movement Detected Inside Locker";
  message.addRecipient("Chiru", RECIPIENT_EMAIL);

  String body = "Security Alert!\n\n";
  body += "Movement has been detected inside the smart locker while the locker is locked.\n\n";
  body += "Locker Status: Locked\n";
  body += "Alert Type: Internal movement detected\n\n";
  body += "Please check the locker immediately.\n\n";
  body += "Device: IoT Smart Locker Security System";

  message.text.content = body.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&config)) {
    Serial.println("SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Movement email sending failed");
    Serial.println(smtp.errorReason());
  } 
  else {
    Serial.println("Movement alert email sent successfully");
  }

  smtp.closeSession();
}

// ================= SMTP CALLBACK =================

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}
#define AUTHOR_PASSWORD "suaersasqqpgjeov"

// Receiver email
#define RECIPIENT_EMAIL "chiru4646111@gmail.com"

// ================= PINS =================
#define SERVO_PIN 18
#define BUZZER_PIN 23
#define PIR_PIN 19

// ================= SERVO ANGLES =================
// If servo works opposite, swap these values
#define LOCK_ANGLE 0
#define UNLOCK_ANGLE 90

Servo lockerServo;
SMTPSession smtp;

// Locker password
String correctPassword = "1234";

int wrongAttempts = 0;
bool lockerLocked = true;

// Firebase checking time
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 1500;

// PIR movement email cooldown
unsigned long lastMovementAlertTime = 0;
const unsigned long movementCooldown = 30000; // 30 seconds

void smtpCallback(SMTP_Status status);

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW);

  lockerServo.attach(SERVO_PIN);
  lockLocker();

  connectWiFi();

  smtp.callback(smtpCallback);

  updateFirebaseString("status", "System Ready - Locker Locked");
  updateFirebaseInt("attempts", 0);
  updateFirebaseString("command", "");
  updateFirebaseString("entered_password", "");
  updateFirebaseString("movement", "No Movement");

  Serial.println("System Started");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Check Firebase command every 1.5 seconds
  if (millis() - lastCheckTime >= checkInterval) {
    lastCheckTime = millis();

    String command = getFirebaseString("command");

    if (command == "CHECK_PASSWORD") {
      String enteredPassword = getFirebaseString("entered_password");

      checkPassword(enteredPassword);

      updateFirebaseString("command", "");
      updateFirebaseString("entered_password", "");
    }

    if (command == "LOCK") {
      lockLocker();

      updateFirebaseString("status", "Locker Locked");
      updateFirebaseString("movement", "No Movement");
      updateFirebaseString("command", "");
    }
  }

  // Check PIR movement when locker is locked
  checkMovementInsideLocker();
}

// ================= WIFI FUNCTION =================

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================= PASSWORD CHECK =================

void checkPassword(String enteredPassword) {
  enteredPassword.trim();

  Serial.print("Entered Password: ");
  Serial.println(enteredPassword);

  if (enteredPassword == correctPassword) {
    wrongAttempts = 0;

    unlockLocker();

    updateFirebaseString("status", "Access Granted - Locker Unlocked");
    updateFirebaseInt("attempts", wrongAttempts);
    updateFirebaseString("movement", "Locker Unlocked");

    Serial.println("Access Granted");
  } 
  else {
    wrongAttempts++;

    updateFirebaseString("status", "Wrong Password");
    updateFirebaseInt("attempts", wrongAttempts);

    beepBuzzer(2);

    Serial.println("Wrong Password");

    if (wrongAttempts >= 3) {
      updateFirebaseString("status", "SECURITY ALERT - 3 Wrong Password Attempts");

      digitalWrite(BUZZER_PIN, HIGH);

      sendWrongPasswordEmailAlert();

      delay(3000);

      digitalWrite(BUZZER_PIN, LOW);

      wrongAttempts = 0;
      updateFirebaseInt("attempts", wrongAttempts);
    }
  }
}

// ================= MOVEMENT CHECK FUNCTION =================

void checkMovementInsideLocker() {
  int motionState = digitalRead(PIR_PIN);

  if (lockerLocked == true && motionState == HIGH) {
    Serial.println("Movement detected inside locker!");

    updateFirebaseString("status", "ALERT - Movement Inside Locked Locker");
    updateFirebaseString("movement", "Movement Detected");

    digitalWrite(BUZZER_PIN, HIGH);

    // Send email only once every 30 seconds
    if (millis() - lastMovementAlertTime > movementCooldown) {
      sendMovementEmailAlert();
      lastMovementAlertTime = millis();
    }

    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else {
    if (lockerLocked == true) {
      updateFirebaseString("movement", "No Movement");
    }
  }
}

// ================= SERVO FUNCTIONS =================

void lockLocker() {
  lockerServo.write(LOCK_ANGLE);
  lockerLocked = true;

  Serial.println("Locker Locked");
}

void unlockLocker() {
  lockerServo.write(UNLOCK_ANGLE);
  lockerLocked = false;

  Serial.println("Locker Unlocked");
}

// ================= BUZZER FUNCTION =================

void beepBuzzer(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

// ================= FIREBASE GET =================

String getFirebaseString(String tag) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";

  http.begin(url);

  int httpCode = http.GET();

  String payload = "";

  if (httpCode > 0) {
    payload = http.getString();

    payload.replace("\"", "");
    payload.trim();

    Serial.println("Firebase GET " + tag + ": " + payload);
  } 
  else {
    Serial.println("Firebase GET failed for " + tag);
  }

  http.end();

  return payload;
}

// ================= FIREBASE UPDATE STRING =================

void updateFirebaseString(String tag, String value) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";
  String data = "\"" + value + "\"";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(data);

  if (httpCode > 0) {
    Serial.println("Firebase updated: " + tag + " = " + value);
  } 
  else {
    Serial.println("Firebase update failed for " + tag);
  }

  http.end();
}

// ================= FIREBASE UPDATE INTEGER =================

void updateFirebaseInt(String tag, int value) {
  HTTPClient http;

  String url = String(FIREBASE_URL) + "/smart_locker/" + tag + ".json";
  String data = String(value);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(data);

  if (httpCode > 0) {
    Serial.println("Firebase updated: " + tag + " = " + String(value));
  } 
  else {
    Serial.println("Firebase update failed for " + tag);
  }

  http.end();
}

// ================= WRONG PASSWORD EMAIL ALERT =================

void sendWrongPasswordEmailAlert() {
  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;

  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "ESP32 Smart Locker";
  message.sender.email = AUTHOR_EMAIL;

  message.subject = "SECURITY ALERT: Wrong Password Attempts";
  message.addRecipient("Chiru", RECIPIENT_EMAIL);

  String body = "Security Alert!\n\n";
  body += "Three wrong password attempts were detected on the smart locker.\n\n";
  body += "Locker Status: Locked\n";
  body += "Alert Type: Wrong password attempts\n\n";
  body += "Please check the locker immediately.\n\n";
  body += "Device: IoT Smart Locker Security System";

  message.text.content = body.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&config)) {
    Serial.println("SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Wrong password email sending failed");
    Serial.println(smtp.errorReason());
  } 
  else {
    Serial.println("Wrong password alert email sent successfully");
  }

  smtp.closeSession();
}

// ================= MOVEMENT EMAIL ALERT =================

void sendMovementEmailAlert() {
  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;

  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "ESP32 Smart Locker";
  message.sender.email = AUTHOR_EMAIL;

  message.subject = "SECURITY ALERT: Movement Detected Inside Locker";
  message.addRecipient("Chiru", RECIPIENT_EMAIL);

  String body = "Security Alert!\n\n";
  body += "Movement has been detected inside the smart locker while the locker is locked.\n\n";
  body += "Locker Status: Locked\n";
  body += "Alert Type: Internal movement detected\n\n";
  body += "Please check the locker immediately.\n\n";
  body += "Device: IoT Smart Locker Security System";

  message.text.content = body.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&config)) {
    Serial.println("SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Movement email sending failed");
    Serial.println(smtp.errorReason());
  } 
  else {
    Serial.println("Movement alert email sent successfully");
  }

  smtp.closeSession();
}

// ================= SMTP CALLBACK =================

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}