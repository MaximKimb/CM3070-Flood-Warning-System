#include <SPI.h>
#include <LoRa.h>

#define FREQUENCY 868E6

const int PIN_SS   = 5;
const int PIN_RST  = 14;
const int PIN_DIO0 = 2;

const int BUZZER_PIN = 25;

const int RAIN_THRESHOLD = 2500;
const int REQUIRED_BAD_READINGS = 3;

unsigned long POWERED_ON_TIME;
const unsigned long TIMEOUT_MS = 30000;
unsigned long lastBeepTime = 0;
bool alarmActive = false;


//State per node
struct NodeState {
  int nodeID;
  int badCount;
  unsigned long lastSeen;
};

NodeState nodes[20];
int nodeCount = 0;

//Find or create node entry
NodeState* getNode(int id) {
  for (int i = 0; i < nodeCount; i++) {
    if (nodes[i].nodeID == id) return &nodes[i];
  }
  if (nodeCount < 20) {
    nodes[nodeCount].nodeID = id;
    nodes[nodeCount].badCount = 0;
    nodes[nodeCount].lastSeen = 0;
    nodeCount++;
    return &nodes[nodeCount - 1];
  }
  return nullptr;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  POWERED_ON_TIME = millis();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  LoRa.setPins(PIN_SS, PIN_RST, PIN_DIO0);

  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa OK");
}

//Function to beep buzzer for specific durations
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

//Process packet
void processPacket(String msg) {
  if (!msg.startsWith("RAIN:")) return;

  int a = msg.indexOf(':');
  int b = msg.indexOf(':', a + 1);
  int c = msg.indexOf(':', b + 1);

  if (a<0 || b< 0|| c<0) return;

  int nodeID = msg.substring(a + 1, b).toInt();
  int rainValue = msg.substring(b + 1, c).toInt();
  int packetID = msg.substring(c + 1).toInt();

  Serial.print("Node ");
  Serial.print(nodeID);
  Serial.print(" | Rain: ");
  Serial.print(rainValue);
  Serial.print(" | packet ");
  Serial.println(packetID);

  NodeState* n = getNode(nodeID);
  if (!n) return;
  n->lastSeen = millis();

  if (rainValue < RAIN_THRESHOLD) {
    n->badCount++;
    if (n->badCount >= REQUIRED_BAD_READINGS) {
      beep(300);
    }
  } else {
    n->badCount = 0;
  }

}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }
    Serial.print("Received: ");
    Serial.println(msg);
    processPacket(msg);
  }

  //Timeout logic
  bool anyRecent = false;
  unsigned long now = millis();
  for (int i = 0; i < nodeCount; i++) {
    if (now - nodes[i].lastSeen < TIMEOUT_MS) {
      anyRecent = true;
      break;
    }
  }
  if (nodeCount > 0 && !anyRecent && millis() - POWERED_ON_TIME > 30000) {
    alarmActive = true;
  } else {
      alarmActive = false;
  }
  if (alarmActive) {
    if (millis() - lastBeepTime > 1000) {  // beep every 1 second
        beep(100);
        lastBeepTime = millis();
    }
  }
}
