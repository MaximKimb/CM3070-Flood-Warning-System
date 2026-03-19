# CM3070-Flood-Warning-System
Repository for a decentralised IoT flood early-warning system using ESP32 and LoRa

The system includes:
- Rainfall sensor-relay nodes (ESP32 + LoRa + FR-04 sensor)
- Gateway node hosting a local dashboard
- Alert-only nodes that trigger a buzzer after 3 consecutive low rainfall readings

Repository Structure:
/SensorNodes/ Firmware for rainfall sensor-relay nodes
/AlertNode/ Firmware for alert-only nodes
/GatewayNode/ Gateway firmware + dashboard hosting

Running the system
1. Flash each file to the appropriate ESP32 using Arduino IDE.
2. Power on the nodes.
3. Connect the gateway's WiFi AP and open the dashboard IP shown in serial monitor.
4. Simulate rainfall to trigger alerts.
