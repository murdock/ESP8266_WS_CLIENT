# Observer
### Simple meteostation with GPS, CO2, temperature and humidity sensors.
### This device uses Webpack bundle of SPA (React/Node) and running from the SPIFFS memory of NodeMCU. At the first run device will be available through WiFi as ObserverSetup and will provide config portal to connect the Observer to network. After successful device connection it will be available on "http://observer.local" inside your network.

### Requirements:
##### NodeMCU v3, MQ-135, DHT-11, and Ublox NEO-8M GPS

### Use this pinout (or make your own):
###### DHT11_PIN = D4
###### MQ135_PIN = A0
###### RXPin (GPS Rx) = D6
###### TXPin (GPS Tx) = D7
