#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

const char* WIFI_SSID = "LiverboxEQ72568";
const char* WIFI_PASS = "eN2f%dDR-6qgh7";

WebServer server(80);

//LoRa frequency
#define FREQUENCY 868E6

//LoRa pins
const int PIN_SS   = 5;
const int PIN_RST  = 14;
const int PIN_DIO0 = 2;

//Threshold for alerts
const int RAIN_THRESHOLD = 2500;

//Node tracking
const int MAX_NODES = 50;
const int HISTORY_LEN = 200;

struct NodeStatus {
  int nodeID;
  int lastRainValue;
  unsigned long lastSeen;
  int lastRSSI;
  unsigned long packetCount;
  time_t lastAlertTime;
  int history[HISTORY_LEN];
  int historyCount;
  int historyIndex;
};

NodeStatus nodes[MAX_NODES];
int nodeCount = 0;

unsigned long totalPackets = 0;
unsigned long startTime = 0;

//Assigning custom names to nodes
String getNodeName(int id) {
  switch(id) {
    case 1: return "Valley South Sensor";
    case 2: return "Mountain East Sensor";
    default: return "Sensor " + String(id);
  }
}

NodeStatus* getNode(int nodeID) {
  for (int i = 0; i < nodeCount; i++) {
    if (nodes[i].nodeID == nodeID) return &nodes[i];
  }
  if (nodeCount < MAX_NODES) {
    NodeStatus &n = nodes[nodeCount];
    n.nodeID = nodeID;
    n.lastRainValue = -1;
    n.lastSeen = 0;
    n.lastRSSI = 0;
    n.packetCount = 0;
    n.lastAlertTime = 0;
    n.historyCount = 0;
    n.historyIndex = 0;
    for (int i = 0; i < HISTORY_LEN; i++) n.history[i] = -1;
    nodeCount++;
    return &nodes[nodeCount - 1];
  }
  return nullptr;
}

//Format the timestamp
String formatTimestamp(time_t t) {
  if (t == 0) return "None";
  struct tm *tm_info = localtime(&t);
  char buffer[32];
  strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
  return String(buffer);
}

//Create the sparkline for viewing history
String buildSparklineSVG(NodeStatus* n) {
  if (n->historyCount == 0) {
    return "<svg width='100%' height='80'></svg>";
  }
  const float width = 200.0;
  const float height = 80.0;
  const int minVal = 0;
  const int maxVal = 4095;

  //Threshold Y position
  float thresholdNorm = (float)(RAIN_THRESHOLD - minVal) / (float)(maxVal - minVal);
  float thresholdY = height - (thresholdNorm * (height - 4)) - 2;

  //Build polyline points
  String points = "";
  for (int i = 0; i < n->historyCount; i++) {
    int idx = (n->historyIndex - n->historyCount + i + HISTORY_LEN) % HISTORY_LEN;
    int v = n->history[idx];
    if (v < 0) continue;
    float x = (n->historyCount == 1) ? width / 2 : (width * i) / (n->historyCount - 1);
    float norm = (float)(v - minVal) / (float)(maxVal - minVal);
    float y = height - (norm * (height - 4)) - 2;
    points += String(x, 1) + "," + String(y, 1) + " ";
  }

  String svg = "<svg width='100%' height='80' viewBox='0 0 200 80'>";

  //Danger shading
  svg += "<rect x='0' y='" + String(thresholdY) + "' width='200' height='" + String(height - thresholdY) + "' ";
  svg += "fill='rgba(255, 0, 0, 0.10)' />";

  //Threshold line
  svg += "<line x1='0' y1='" + String(thresholdY) + "' x2='200' y2='" + String(thresholdY) + "' ";
  svg += "stroke='#e74c3c' stroke-width='2' stroke-dasharray='4,3' />";

  //Sparkline
  svg += "<polyline fill='none' stroke='#2b4c7e' stroke-width='2' points='" + points + "' />";

  svg += "</svg>";
  return svg;
}

//Create the dashboard
String generateDashboard() {
  String html = "<html><head>";
  html += "<meta http-equiv='refresh' content='1'>";
  html += "<style>";

  //Global
  html += "body { font-family: Arial, sans-serif; background:#eef1f5; margin:0; padding:0; }";
  html += ".header { background:#2b4c7e; color:white; padding:20px; font-size:26px; font-weight:bold; }";
  //html += ".header { background:#2b4c7e; color:white; padding:18px; font-size:24px; font-weight:bold; }";
  html += ".container { padding:20px; }";

  //Grid and cards
  html += ".grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:20px; }";
  html += ".card { background:white; padding:16px 16px 20px 16px; border-radius:12px; box-shadow:0 3px 8px rgba(0,0,0,0.15); position:relative; }";

  //The severity banner
  html += ".severity-banner { position:absolute; top:0; left:0; right:0; height:6px; border-radius:12px 12px 0 0; }";
  html += ".sev-green { background:#3cb371; }";
  html += ".sev-yellow { background:#f1c40f; }";
  html += ".sev-red { background:#e74c3c; }";

  //Text
  html += ".node-title { font-size:18px; font-weight:bold; color:#333; margin-top:10px; }";
  html += ".node-subtitle { font-size:13px; color:#666; margin-bottom:6px; }";

  //Status badges
  html += ".status { padding:4px 10px; border-radius:6px; font-weight:bold; display:inline-block; margin-top:8px; font-size:13px; }";
  html += ".online { background:#d4f8d4; color:#1a7f1a; }";
  html += ".offline { background:#ffd6d6; color:#a30000; }";

  //Rain
  html += ".rain { font-size:16px; margin-top:8px; }";
  html += ".rain-green { color:#1a7f1a; font-weight:bold; }";
  html += ".rain-yellow { color:#c7a100; font-weight:bold; }";
  html += ".rain-red { color:#a30000; font-weight:bold; }";

  //Icons
  html += ".icon { font-size:18px; margin-right:6px; vertical-align:middle; }";

  //Health
  html += ".health { margin-top:8px; font-size:13px; color:#555; }";
  html += ".sparkline { margin-top:8px; }";

  //Footer
  html += ".footer { margin-top:20px; padding:10px 20px; background:#2b4c7e; color:white; font-size:13px; display:flex; flex-wrap:wrap; gap:15px; }";
  html += ".footer-item { margin-right:15px; }";

  html += "</style></head><body>";

  html += "<div class='header'>🌧️ Flood Monitoring Dashboard</div>";
  html += "<div class='container'>";
  html += "<div class='grid'>";

  unsigned long now = millis();
  int onlineCount = 0;
  for (int i = 0; i < nodeCount; i++) {
    NodeStatus* n = &nodes[i];
    unsigned long age = (now - n->lastSeen) / 1000;
    bool online = age < 5;
    if (online) onlineCount++;

    //Severity based on rain
    String sevClass = "sev-green";
    String rainClass = "rain-green";
    String rainIcon = "🌤️";

    if (n->lastRainValue < RAIN_THRESHOLD && n->lastRainValue >= 2500) {
      sevClass = "sev-yellow";
      rainClass = "rain-yellow";
      rainIcon = "🌧️";
    } else if (n->lastRainValue < 2500) {
      sevClass = "sev-red";
      rainClass = "rain-red";
      rainIcon = "⛈️";
      //rainIcon = "☔";
    }

    html += "<div class='card'>";
    html += "<div class='severity-banner " + sevClass + "'></div>";
    html += "<div class='node-title'>" + getNodeName(n->nodeID) + "</div>";
    html += "<div class='node-subtitle'>Node ID: " + String(n->nodeID) + "</div>";
    html += "<div class='rain " + rainClass + "'>";
    html += "<span class='icon'>" + rainIcon + "</span>";
    html += "Rain Value: " + String(n->lastRainValue) + "</div>";

    if (online) {
      html += "<div class='status online'>🟢 Online</div>";
    } else {
      html += "<div class='status offline'>🔴 Offline</div>";
    }
    html += "<div class='node-subtitle'>Last seen: " + String(age) + "s ago</div>";

    //RSSI and packet count
    html += "<div class='health'>";
    html += "📶 RSSI: " + String(n->lastRSSI) + " dBm &nbsp;&nbsp; ";
    html += "📦 Packets: " + String(n->packetCount);
    html += "</div>";

    //Last alert
    html += "<div class='node-subtitle'>⚠️ Last Alert: " + formatTimestamp(n->lastAlertTime) + "</div>";

    //Sparkline
    html += "<div class='sparkline'>";
    html += buildSparklineSVG(n);
    html += "</div>";

    html += "</div>"; //Card
  }

  html += "</div>"; //Grid

  //Footer
  html += "<div class='footer'>";
  html += "<div class='footer-item'>System Status: ✅ OK</div>";
  html += "<div class='footer-item'>LoRa Packets Received: " + String(totalPackets) + "</div>";
  html += "<div class='footer-item'>Nodes Online: " + String(onlineCount) + " / " + String(nodeCount) + "</div>";
  html += "<div class='footer-item'>Gateway Uptime: " + formatTimestamp(time(nullptr)) + "</div>";
  html += "</div>";

  html += "</div></body></html>";
  return html;
}

//Process the incoming packets
void processPacket(String msg, int rssi) {
  if (!msg.startsWith("RAIN:")) return;

  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  int third = msg.indexOf(':', second + 1);

  if (first < 0 || second < 0 || third < 0) return;

  int nodeID = msg.substring(first + 1, second).toInt();
  int rainValue = msg.substring(second + 1, third).toInt();
  int packetID = msg.substring(third + 1).toInt();

  if (nodeID <= 0) return;

  Serial.print("Node ");
  Serial.print(nodeID);
  Serial.print(" | Rain: ");
  Serial.print(rainValue);
  Serial.print(" | pkt ");
  Serial.print(packetID);
  Serial.print(" | RSSI ");
  Serial.println(rssi);

  NodeStatus* n = getNode(nodeID);
  if (!n) return;

  n->lastRainValue = rainValue;
  n->lastSeen = millis();
  n->lastRSSI = rssi;
  n->packetCount++;

  //Update history
  n->history[n->historyIndex] = rainValue;
  n->historyIndex = (n->historyIndex + 1) % HISTORY_LEN;
  if (n->historyCount < HISTORY_LEN) n->historyCount++;

  //Update last alert time if below threshold
  if (rainValue < RAIN_THRESHOLD) {
    n->lastAlertTime = time(nullptr);
  }

  totalPackets++;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int i = 0; i < MAX_NODES; i++) {
    nodes[i].nodeID = -1;
  }
  startTime = millis();

  //WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.localIP());

  //Time sync
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synced!");

  server.on("/", []() {
    server.send(200, "text/html; charset=UTF-8", generateDashboard());
  });
  server.begin();

  //LoRa
  LoRa.setPins(PIN_SS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa OK");
}

void loop() {
  server.handleClient();
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) {
      msg += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();
    Serial.print("Received: ");
    Serial.println(msg);
    processPacket(msg, rssi);
  }
}