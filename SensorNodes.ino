#include <SPI.h>
#include <LoRa.h>

//Increment NodeID for each sensor
#define NODE_ID 1 
//LoRa Frequency
#define FREQUENCY 868E6

//LoRa pins
const int PIN_SS   = 5;
const int PIN_RST  = 14;
const int PIN_DIO0 = 2;

//Rain sensor
const int RAIN_AO = 34;

//Duplicate supression
struct SeenPacket {
  int nodeID;
  int packetID;
};

const int MAX_SEEN = 30;
SeenPacket seenPackets[MAX_SEEN];
int seenIndex = 0;

unsigned long packetCounter = 0;

//Check if the nodeID packetID pair has been seen before
bool hasSeenPacket(int nodeID, int pktID) {
  for (int i = 0; i < MAX_SEEN; i++) {
    if (seenPackets[i].nodeID == nodeID &&
        seenPackets[i].packetID == pktID) {
      return true; //Already processed the packet before
    }
  }
  return false; //It is a new packet
}

//Store a packet in the circular buffer of recently seen packets
void storePacket(int nodeID, int pktID) {
  seenPackets[seenIndex].nodeID = nodeID;
  seenPackets[seenIndex].packetID = pktID;
  seenIndex = (seenIndex + 1) % MAX_SEEN; //Wrap around
}

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < MAX_SEEN; i++) {
    seenPackets[i].nodeID = -1;
    seenPackets[i].packetID = -1;
  }
  Serial.println("Sensor Node (Mesh Enabled)");
  Serial.print("Node ID: ");
  Serial.println(NODE_ID);

  LoRa.setPins(PIN_SS, PIN_RST, PIN_DIO0);

  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init failed");
    while (1);
  }
}

void sendOwnPacket() {
  int rainValue = analogRead(RAIN_AO);
  packetCounter++;

  Serial.print("Sending packet :");
  Serial.println(packetCounter);

  LoRa.beginPacket();
  LoRa.print("RAIN:");
  LoRa.print(NODE_ID);
  LoRa.print(":");
  LoRa.print(rainValue);
  LoRa.print(":");
  LoRa.print(packetCounter);
  LoRa.endPacket();

  storePacket(NODE_ID, packetCounter);
}

//Process received packets
void processIncomingPacket(String msg) {
  if (!msg.startsWith("RAIN:")) return;

  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  int third = msg.indexOf(':', second + 1);

  if (first < 0 || second < 0 || third < 0) return;

  int senderID = msg.substring(first + 1, second).toInt();
  //int senderID = msg.substring(second).toInt();
  int rainValue = msg.substring(second + 1, third).toInt();
  int pktID = msg.substring(third + 1).toInt();

  if (senderID == NODE_ID) return;

  if (hasSeenPacket(senderID, pktID)) return;

  Serial.print("Forwarding packet from Node ");
  Serial.print(senderID);
  storePacket(senderID, pktID);

  //Re-broadcast
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
}

void loop() {
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    sendOwnPacket();
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }
    Serial.print("Received: ");
    Serial.println(msg);
    processIncomingPacket(msg);
  }
}
