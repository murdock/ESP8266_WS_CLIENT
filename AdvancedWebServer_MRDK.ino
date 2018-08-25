const char *wsHost = "192.168.1.7";
int wsPort = 44444;
const char *wsPOST = "192.168.1.7:44444";
String buffer;
#define USE_SERIAL Serial1
#define DHT11_PIN D4
#define MQ135_PIN A0
#define RXPin D6
#define TXPin D7
#define GPSBaud 9600


#include <dht.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#include <WebSocketsClient.h>
WebSocketsClient webSocket;



// the IP address for the shield:
TinyGPSPlus gps;
dht DHT;
ESP8266WebServer server ( 80 );
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
SoftwareSerial ss(TXPin, RXPin);

void handleGPS() {
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      displayGPSInfo();
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
}
void handleRoot() {
	char temp[400];
	int sec = millis() / 1000;
	int min = sec   / 60;
	int hr = min / 60;

	snprintf ( temp, 400,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <script>\
        document.getElementById('main').innerText = 'TEST!!!'\
    </script>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <div id='main'></div>\
  </body>\
</html>",
		hr, min % 60, sec % 60
	);
  
	server.send ( 200, "text/html", temp );
}

void handleNotFound() {
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ ) {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}

	server.send ( 404, "text/plain", message );
}

void setup ( void ) {
	Serial.begin (115200);
  
  ss.begin(GPSBaud);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(320);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //set custom ip for portal
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,1,10), IPAddress(192,168,1,11), IPAddress(255,255,255,0));
  //delay(1);
  wifiManager.autoConnect("PisjunWIFI");
 
  // Wait for connection
    while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );
    }
    Serial.println ( "" );
    Serial.printf("Connection status: %d\n", WiFi.status());
    WiFi.printDiag(Serial);
    
    Serial.print ( "Connected to " );
    Serial.println ( WiFi.SSID() );
    Serial.print ( "IP address: " );
    Serial.println ( WiFi.localIP() );
    if ( MDNS.begin ( "esp8266" ) ) {
      Serial.println ( "MDNS responder started" );
    }

    server.on ( "/", handleRoot );
    server.on ( "/inline", []() {
      server.send ( 200, "text/plain", "this works as well" );
    } );
    //server.on ( "/test", sendMessage );
    server.onNotFound ( handleNotFound );
    server.begin();
    Serial.println ( "HTTP server started" );
    server.send(200);
    // server address, port and URL
    webSocket.begin(wsHost, wsPort, "/");

    // event handler
    webSocket.onEvent(webSocketEvent);
  
    // use HTTP Basic Authorization this is optional remove if not needed
    webSocket.setAuthorization("user", "Password");
  
    // try ever 5000 again if connection has failed
    webSocket.setReconnectInterval(10000);
}
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
 
    int air = analogRead(MQ135_PIN);
    int chk = DHT.read11(DHT11_PIN);
    float h = DHT.humidity;
    float t = DHT.temperature;
    
    if (isnan(h) || isnan(t)) 
    {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    if (isnan(air)) {
      Serial.println("Failed to read from MQ sensor!");
      return;
    }
    root["temperature"] = t;
    root["humidity"] = h;
    root["airQuality"] = air;
    
    if(ss.available()){
      root["lat"] = gps.location.lat();
      root["lng"] = gps.location.lng();
      root["satellites"] = gps.satellites.value();
      root["speedMPH"] = gps.speed.mph();
      root["altitudeFeet"] = gps.altitude.feet();
    } else {
        Serial.println("GPS IS NOT AVAILABLE!");
    }
    
    String data;
    
    root.printTo(data);
    
  switch(type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED: {
      USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

      // send message to server when Connected
      char *msg = "[\"CONNECT\\naccept-version:1.1,1.0\\nheart-beat:10000,10000\\n\\n\\u0000\"]";
      webSocket.sendTXT(msg);
    }
      break;
    case WStype_TEXT:
      USE_SERIAL.printf("[WSc] get text: %s\n", payload);

      // send message to server
      webSocket.sendTXT(data);
      break;
    case WStype_BIN:
      USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);
      // send data to server
      webSocket.sendBIN(payload, length);
      break;
  }
  //delay(5000);
}
void displayGPSInfo()
{
  if (gps.location.isValid())
  {
    root["lat"] = gps.location.lat();
    root["lng"] = gps.location.lng();
    root["satellites"] = gps.satellites.value();
    root["speedMPH"] = gps.speed.mph();
    root["altitudeFeet"] = gps.altitude.feet();
   
    //Serial.println(buffer);
  }
  else
  {
    Serial.print(F("INVALID"));
  }
}
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}
void saveConfigCallback () {
  Serial.println("Should save config");
  //shouldSaveConfig = true;
}
void loop ( void ) {
  webSocket.loop();
  server.handleClient();
  handleGPS();
  delay(5000);
}
