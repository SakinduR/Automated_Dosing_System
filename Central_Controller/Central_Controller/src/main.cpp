#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// 1. PIN DEFINITIONS (Main Plant Only)
// ==========================================
#define RADAR_PIN 34
#define WATER_FLOW_PIN 35
#define WATER_VALVE_PIN 19
#define TANK_SWITCH_PIN 14
#define CHEM_FLOW_PIN 32
#define CHEM_PUMP_PIN 5
#define ALARM_RED_PIN 12
#define ALARM_YELLOW_PIN 13
#define BUZZER_PIN 4

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================================
// 2. IOT & NETWORK SETTINGS
// ==========================================
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "broker.hivemq.com";

const char *TOPIC_STATUS = "hospital/dosing/sakindu/status";
const char *TOPIC_ALARMS = "hospital/dosing/sakindu/alarms";
const char *TOPIC_COMMAND = "hospital/dosing/sakindu/command";

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================
// 3. STATE VARIABLES & QUEUE
// ==========================================
int refillQueue[10]; // Expanded array to handle up to 10 hospital tanks
int queueCount = 0;
bool isFilling = false;
int activeTank = 0;
unsigned long fillStartTime = 0;
const unsigned long FILL_DURATION = 15000;

bool systemHalted = false;
String errorMessage = "";
unsigned long lastAlarmToggle = 0;
bool alarmState = false;
unsigned long lastMqttReconnect = 0;

void addToQueue(int tankID)
{
  if (activeTank == tankID)
    return;
  for (int i = 0; i < queueCount; i++)
    if (refillQueue[i] == tankID)
      return;
  if (queueCount < 10)
    refillQueue[queueCount++] = tankID;
}

int popFromQueue()
{
  if (queueCount == 0)
    return 0;
  int nextTank = refillQueue[0];
  for (int i = 0; i < queueCount - 1; i++)
    refillQueue[i] = refillQueue[i + 1];
  queueCount--;
  return nextTank;
}

// ==========================================
// 4. MQTT CALLBACK (Listening to Nodes)
// ==========================================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (int i = 0; i < length; i++)
    message += (char)payload[i];

  Serial.print("Network Request: ");
  Serial.println(message);

  // Parse incoming requests from any remote node in the hospital
  if (message == "REFILL_TANK1")
    addToQueue(1);
  if (message == "REFILL_TANK2")
    addToQueue(2);
  // You can easily add more here later if the hospital expands
}

// ==========================================
// 5. FAIL-SAFE ALGORITHMS
// ==========================================
void triggerCriticalAlarm(String msg)
{
  systemHalted = true;
  errorMessage = msg;

  digitalWrite(WATER_VALVE_PIN, LOW);
  analogWrite(CHEM_PUMP_PIN, 0);

  client.publish(TOPIC_ALARMS, msg.c_str());
}

void checkFailSafes(unsigned long currentMillis)
{
  if (systemHalted)
    return;

  // Yellow Warning
  if (digitalRead(TANK_SWITCH_PIN) == HIGH)
  {
    digitalWrite(ALARM_YELLOW_PIN, HIGH);
  }
  else
  {
    digitalWrite(ALARM_YELLOW_PIN, LOW);
  }

  // Red Critical Checking
  if (isFilling && (currentMillis - fillStartTime > 2000))
  {
    int waterFlow = analogRead(WATER_FLOW_PIN);
    int chemFlow = analogRead(CHEM_FLOW_PIN);

    if (waterFlow < 100)
      triggerCriticalAlarm("CRITICAL: WATER BLOCKAGE");
    else if (chemFlow < 100)
      triggerCriticalAlarm("CRITICAL: PUMP/LINE FAIL");
  }
}

// ==========================================
// 6. SETUP
// ==========================================
void setup()
{
  Serial.begin(115200);

  pinMode(RADAR_PIN, INPUT);
  pinMode(WATER_FLOW_PIN, INPUT);
  pinMode(CHEM_FLOW_PIN, INPUT);
  pinMode(TANK_SWITCH_PIN, INPUT_PULLUP);

  pinMode(WATER_VALVE_PIN, OUTPUT);
  pinMode(CHEM_PUMP_PIN, OUTPUT);
  pinMode(ALARM_RED_PIN, OUTPUT);
  pinMode(ALARM_YELLOW_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(WATER_VALVE_PIN, LOW);
  analogWrite(CHEM_PUMP_PIN, 0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WIFI CONNECTED!");

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  delay(1500);
  lcd.clear();
}

// ==========================================
// 7. MAIN LOOP
// ==========================================
void loop()
{
  unsigned long currentMillis = millis();

  // Keep Network Alive
  if (!client.connected() && (currentMillis - lastMqttReconnect > 5000))
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect("HospitalCentralNode_Sakindu"))
    {
      client.subscribe(TOPIC_COMMAND);
      client.publish(TOPIC_STATUS, "Central Controller Online");
    }
    lastMqttReconnect = currentMillis;
  }
  client.loop();

  // --- CRITICAL FAULT LOOP ---
  if (systemHalted)
  {
    if (currentMillis - lastAlarmToggle > 500)
    {
      alarmState = !alarmState;
      digitalWrite(ALARM_RED_PIN, alarmState);
      digitalWrite(BUZZER_PIN, alarmState);
      lastAlarmToggle = currentMillis;
    }
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM HALTED!  ");
    lcd.setCursor(0, 1);
    lcd.print("CHECK ALARMS    ");
    return;
  }

  // --- NORMAL OPERATION ---
  checkFailSafes(currentMillis);

  // Check if we need to start dispensing
  if (!isFilling && queueCount > 0)
  {
    activeTank = popFromQueue();
    isFilling = true;
    fillStartTime = currentMillis;

    // Start physical plant hardware
    digitalWrite(WATER_VALVE_PIN, HIGH);
    analogWrite(CHEM_PUMP_PIN, 128);

    // Broadcast the command over Wi-Fi so the specific tank node opens its valve
    String msg = "FILLING TANK: " + String(activeTank);
    client.publish(TOPIC_STATUS, msg.c_str());
  }

  // Check if dispense cycle is complete
  if (isFilling)
  {
    if (currentMillis - fillStartTime >= FILL_DURATION)
    {
      // Shutdown physical plant hardware
      digitalWrite(WATER_VALVE_PIN, LOW);
      analogWrite(CHEM_PUMP_PIN, 0);

      isFilling = false;
      activeTank = 0;

      client.publish(TOPIC_STATUS, "SYSTEM IDLE");
    }
  }

  // Update Display
  lcd.setCursor(0, 0);
  if (isFilling)
  {
    lcd.print("FILLING TANK: ");
    lcd.print(activeTank);
  }
  else
  {
    lcd.print("SYSTEM IDLE     ");
  }

  lcd.setCursor(0, 1);
  lcd.print("In Queue: ");
  lcd.print(queueCount);
  lcd.print("    ");

  delay(50);
}