# Oregon-RCSwitch RX 433Mhz

The Oregon-RCSwitch processes the following data using an ESP01, NodeMCU (ESP8266), or ESP32:

- Receiving RCSwitch data via  a 433 MHz receiver
- Receiving Oregon encoded temperature and humidity data via a 433 MHz receiver
- Receiving MQTT commands and setting GPIO pins. This enables, for example, the switching of light sources
- Reading temperature and humidity data from a directly connected sensor (e.g., DHT22, BME680, MCP9808)
- Forwarding the received data to an MQTT broker (e.g., HomeAssistant)


## Configuration

Depending on the hardware connected, the constants must be set accordingly in main.cpp. The mqtt-broker must be configured in Settings.h.

## Switching on and off light sources

Light sources typically require more power than an ESPxxx can supply. For this reason, the signals from the GPIO pins must be amplified.  

Here is an example:

![Schematic](/images/schematic.png)  
![Board](/images/board.png)

