#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Smart Locker"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP_Mail_Client.h>
#include <ESP32Servo.h>

// WiFi details
char ssid[] = "YOUR_WIFI_NAME";  //you wifi name
char pass[] = "YOUR_WIFI_PASSWORD";  // your wifi password

// Gmail SMTP details
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define AUTHOR_EMAIL "your_email@gmail.com"
#define AUTHOR_PASSWORD "your_gmail_app_password"
#define RECIPIENT_EMAIL "receiver_email@gmail.com"

// Pins
#define SERVO_PIN 18
#define DOOR_SENSOR_PIN 19
#define BUZZER_PIN 23

// Servo lock angles
#define LOCK_ANGLE 0
#define UNLOCK_ANGLE 90

Servo lockerServo;
SMTPSession smtp;

String correctPassword = "1234";
String enteredPassword = "";

int wrongAttempts = 0;
bool lockerLocked = true;
bool emailCooldown = false;

BlynkTimer timer;

void smtpCallback(SMTP_Status status);

void setup() {
  Serial.begin(115200);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  lockerServo.attach(SERVO_PIN);
  lockLocker();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  smtp.callback(smtpCallback);

  Blynk.virtualWrite(V2, "System Ready - Locker Locked");
  Blynk.virtualWrite(V3, 255);

  timer.setInterval(1000L, checkDoorSensor);
}

// V0 = Password text input
BLYNK_WRITE(V0) {
  enteredPassword = param.asStr();
}

// V1 = Submit password button
BLYNK_WRITE(V1) {
  int buttonState = param.asInt();

  if (buttonState == 1) {
    checkPassword();
  }
}

// V4 = Manual lock button
BLYNK_WRITE(V4) {
  int buttonState = param.asInt();

  if (buttonState == 1) {
    lockLocker();
    Blynk.virtualWrite(V2, "Locker Locked Manually");
  }
}

void loop() {
  Blynk.run();
  timer.run();
}

void checkPassword() {
  if (enteredPassword == correctPassword) {
    wrongAttempts = 0;

    unlockLocker();

    Blynk.virtualWrite(V2, "Access Granted - Locker Unlocked");
    Blynk.virtualWrite(V3, 0);

    Serial.println("Access Granted");
  } 
  else {
    wrongAttempts++;

    Blynk.virtualWrite(V2, "Wrong Password! Attempt: " + String(wrongAttempts));
    Serial.println("Wrong Password");

    beepBuzzer(2);

    if (wrongAttempts >= 3) {
      Blynk.virtualWrite(V2, "Security Alert! 3 Wrong Attempts");
      digitalWrite(BUZZER_PIN, HIGH);

      sendEmailAlert("Three wrong password attempts detected.");

      delay(3000);
      digitalWrite(BUZZER_PIN, LOW);

      wrongAttempts = 0;
    }
  }

  enteredPassword = "";
  Blynk.virtualWrite(V0, "");
}

void lockLocker() {
  lockerServo.write(LOCK_ANGLE);
  lockerLocked = true;

  Blynk.virtualWrite(V3, 255);
  Serial.println("Locker Locked");
}

void unlockLocker() {
  lockerServo.write(UNLOCK_ANGLE);
  lockerLocked = false;

  Blynk.virtualWrite(V3, 0);
  Serial.println("Locker Unlocked");
}

void checkDoorSensor() {
  int doorState = digitalRead(DOOR_SENSOR_PIN);

  // Door sensor gives HIGH when opened using INPUT_PULLUP
  if (lockerLocked == true && doorState == HIGH && emailCooldown == false) {
    Blynk.virtualWrite(V2, "ALERT! Locker Forced Open");

    digitalWrite(BUZZER_PIN, HIGH);

    sendEmailAlert("Locker was forcefully opened while it was locked.");

    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);

    emailCooldown = true;
    timer.setTimeout(30000L, resetEmailCooldown);
  }
}

void resetEmailCooldown() {
  emailCooldown = false;
}

void beepBuzzer(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void sendEmailAlert(String alertReason) {
  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = "ESP32 Smart Locker";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "SECURITY ALERT: Smart Locker Warning";
  message.addRecipient("User", RECIPIENT_EMAIL);

  String body = "Security Alert!\n\n";
  body += "Suspicious activity detected in the smart locker.\n\n";
  body += "Alert Reason: ";
  body += alertReason;
  body += "\n\nDevice: IoT Smart Locker Security System";

  message.text.content = body.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&config)) {
    Serial.println("SMTP connection failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email sending failed");
    Serial.println(smtp.errorReason());
  } 
  else {
    Serial.println("Email sent successfully");
  }

  smtp.closeSession();
}

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}