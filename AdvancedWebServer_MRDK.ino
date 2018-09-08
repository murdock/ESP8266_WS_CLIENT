const char* mdnsName = "multisensor";
const char* htmlfile = "/index.html";
#include <FS.h>   //Include File System Headers

#define DHT11_PIN D4
#define MQ135_PIN A0
#define RXPin D6
#define TXPin D7
#define GPSBaud 9600
#define pinReset 9 // Reset button D9
#include <dht.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;
dht DHT;
ESP8266WebServer server ( 80 );
MDNSResponder mdns;
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
SoftwareSerial ss(TXPin, RXPin);

//define your default values here, if there are different values in config.json, they are overwritten.
char multipass_server[40];
char multipass_port[6] = "80";
char multipass_token[34] = "MULTIPASS_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

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
  server.sendHeader("Location", "/index.html",true);   //Redirect to our html web page
  server.send(302, "text/plane","");
}

void handleWebRequests(){
  if(loadFromSpiffs(server.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.println(message);
}
void setup ( void ) {
	Serial.begin (115200);
  ss.begin(GPSBaud);
  Serial.println("*****");
  Serial.println("GPS Initialized");
  
  //create portal
  Serial.println("softAP initialized...");
  Serial.println(WiFi.softAP("MultisensorPortal", "001122") ? "Ready" : "Failed!");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  Serial.println("connected to:");
  Serial.println(WiFi.SSID());
  
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(multipass_server, json["multipass_server"]);
          strcpy(multipass_port, json["multipass_port"]);
          strcpy(multipass_token, json["multipass_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_multipass_server("server", "multipass server", multipass_server, 40);
  WiFiManagerParameter custom_multipass_port("port", "multipass port", multipass_port, 5);
  WiFiManagerParameter custom_multipass_token("token", "multipass token", multipass_token, 32);

  //WiFiManager
  //Local intialization
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,101), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add parameters here
  wifiManager.addParameter(&custom_multipass_server);
  wifiManager.addParameter(&custom_multipass_port);
  wifiManager.addParameter(&custom_multipass_token);
  
  if (!wifiManager.autoConnect("MultisensorSetup", "password*")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(multipass_server, custom_multipass_server.getValue());
  strcpy(multipass_port, custom_multipass_port.getValue());
  strcpy(multipass_token, custom_multipass_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["multipass_server"] = multipass_server;
    json["multipass_port"] = multipass_port;
    json["multipass_token"] = multipass_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266", WiFi.localIP())) {
      MDNS.begin(mdnsName); // start the multicast domain name server
      Serial.print("mDNS responder started: http://");
      Serial.print(mdnsName);
      Serial.println(".local");
  }

  server.on ( "/", handleRoot );
  server.on ( "/getData", sendDataMessage);
  server.onNotFound(handleWebRequests);
  server.begin();
  Serial.println ( "HTTP server started" );
  server.send(200);
  MDNS.addService("http", "tcp", 80);
}

void sendDataMessage(){
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
    root["sensor"] = true;
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
    server.send ( 200, "text/plain", data );
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
  }
  else
  {
    Serial.print(F("INVALID"));
    Serial.print("");
  }
}


void loop ( void ) {
  server.handleClient();
  handleGPS();
}
bool loadFromSpiffs(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.html";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}
