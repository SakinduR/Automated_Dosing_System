#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// 1. HARDWARE PINS (Dispenser Tank Only)
// ==========================================
#define SENSOR_PIN 26 // Optical Level Sensor (Pushbutton)
#define VALVE_LED 27  // SMC VX2 Valve (Green LED)

// ==========================================
// 2. NETWORK CONFIGURATION
// ==========================================
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "broker.hivemq.com";

// These MUST exactly match the Central Controller's topics!
const char *TOPIC_COMMAND = "hospital/dosing/sakindu/command";
const char *TOPIC_STATUS = "hospital/dosing/sakindu/status";

WiFiClient espClient;
PubSubClient client(espClient);

bool lastButtonState = HIGH;

// ==========================================
// 3. MQTT LISTENER (Waiting for Hub Commands)
// ==========================================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (int i = 0; i < length; i++)
    message += (char)payload[i];

  Serial.print("Central Hub says: ");
  Serial.println(message);

  // If the hub tells us it's our turn, open the physical valve!
  if (message == "FILLING TANK: 1")
  {
    digitalWrite(VALVE_LED, HIGH);
    Serial.println("VALVE OPENED.");
  }
  // If the hub says idle, ensure valve is closed.
  else if (message == "SYSTEM IDLE")
  {
    digitalWrite(VALVE_LED, LOW);
    Serial.println("VALVE CLOSED.");
  }
}

// ==========================================
// 4. SETUP
// ==========================================
void setup()
{
  Serial.begin(115200);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(VALVE_LED, OUTPUT);
  digitalWrite(VALVE_LED, LOW);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
}

// ==========================================
// 5. MAIN LOOP
// ==========================================
void loop()
{
  // Keep MQTT Connected (Note the unique client name for this specific node)
  if (!client.connected())
  {
    if (client.connect("HospitalDispenser1_Sakindu"))
    {
      client.subscribe(TOPIC_STATUS); // Listen to the Hub's status updates
      Serial.println("Connected to MQTT Broker.");
    }
  }
  client.loop();

  // Read the local Tank Level Sensor
  bool currentButtonState = digitalRead(SENSOR_PIN);

  // If fluid is LOW (Button pressed) and it wasn't pressed a millisecond ago
  if (currentButtonState == LOW && lastButtonState == HIGH)
  {
    Serial.println("Fluid Low! Requesting Refill from Central Hub...");

    // Publish request to the internet
    client.publish(TOPIC_COMMAND, "REFILL_TANK1");
    delay(500); // Simple debounce to prevent spamming
  }

  lastButtonState = currentButtonState;
}