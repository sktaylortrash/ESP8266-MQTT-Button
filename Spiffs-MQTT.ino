#include <FS.h> // Include the SPIFFS library
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include "variables.h"

const int FW_VERSION = 907;
//Button GPIO Definitions
#define BUTTON_PIN1 D7
#define BUTTON_PIN2 D6


WiFiClient espClient;
PubSubClient client(espClient);

//Don't Change After Here Unless You Know What You Are Doing

void serveindex(){
  server.send ( 200, "text/html", IndexPage() );
}

void clearconfig() {
  ESP.eraseConfig();
  ESP.reset();
}

void reboot(){
  server.sendHeader("Location","/reboot.html");
  server.send(303);
  timer.setTimeout(1500, clearconfig);
}


void serveinfo(){
  uptime();
  server.send ( 200, "text/html", InfoPage() );
}

void HTTPUpdateConnect() {  
  httpUpdater.setup(&server);

  // Start HTTP server
  server.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    if (!handleFileRead("/upload.html"))                // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });
  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );
  server.on("/info", HTTP_GET, serveinfo);
  server.on("/reboot", HTTP_GET, reboot);
  server.on("/", HTTP_GET, serveindex);
  server.on("/check", HTTP_GET, httpcheck);
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
    });
  server.begin();
  Serial.println(F("HTTP server started"));
  // Set up mDNS responder:
  if (!MDNS.begin(splocalhost.c_str())) {
    Serial.println(F("Error setting up MDNS responder!"));
    while(1) { 
      delay(1000);
    }
  }
  Serial.println(F("mDNS responder started"));
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

bool handleFileRead(String path){  // send the right file to the client (if it exists)
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";                                       // Use the compressed version
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print(F("handleFileUpload Name: ")); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print(F("handleFileUpload Size: ")); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void httpcheck() {                           // If a GET request is made to URI /check
  checkForUpdates();
  server.sendHeader("Location","/");        // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}


void connectWiFi() {
  IPAddress ip(spip[0], spip[1], spip[2], spip[3]);
  IPAddress gateway(spgw[0], spgw[1], spgw[2], spgw[3]);
  IPAddress subnet(spsn[0], spsn[1], spsn[2], spsn[3]);
  IPAddress DNS(spns[0], spns[1], spns[2], spns[3]);
  Serial.println();
  Serial.print(F("Initializing Network "));
  Serial.println();
  WiFi.config(ip, gateway, subnet, DNS);
  Serial.println();
  Serial.print(F("connecting to "));
  Serial.println(spssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(spssid.c_str(), sppassword.c_str());
}


void initHardware() {
  Serial.begin(115200);
  // Setup the buttons with an internal pull-up :
  pinMode(BUTTON_PIN1,INPUT_PULLUP);
  pinMode(BUTTON_PIN2,INPUT_PULLUP);
  // After setting up the button, setup the Bounce instances :
  debouncer1.attach(BUTTON_PIN1);
  debouncer1.interval(200);
  debouncer2.attach(BUTTON_PIN2);
  debouncer2.interval(200);
}

void uptime(){
//** Making Note of an expected rollover *****//   
  if(millis()>=3000000000){ 
    HighMillis=1;
  }
//** Making note of actual rollover **//
  if(millis()<=100000&&HighMillis==1){
    Rollover++;
    HighMillis=0;
  }
  long secsUp = millis()/1000;
  Second = secsUp%60;
  Minute = (secsUp/60)%60;
  Hour = (secsUp/(60*60))%24;
  Day = (Rollover*50)+(secsUp/(60*60*24));  //First portion takes care of a rollover [around 50 days]
};

String getMAC()
{
  uint8_t mac[6];
  char result[14];
  WiFi.macAddress(mac);

 snprintf( result, sizeof( result ), "%02x%02x%02x%02x%02x%02x", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );

  return String( result );
}

void checkForUpdates() {
  String mac = getMAC();
  String fwURL = String( spfwUrlBase );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( mac );
  fwVersionURL.concat( ".version" );

  Serial.println(F( "Checking for firmware updates." ));
  Serial.print(F( "MAC address: " ));
  Serial.println( mac );
  Serial.print(F( "Firmware version URL: " ));
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print(F( "Current firmware version: " ));
    Serial.println( FW_VERSION );
    Serial.print(F( "Available firmware version: " ));
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println(F( "Preparing to update" ));

      String fwImageURL = fwURL;
      fwImageURL.concat( newFWVersion );
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println(F("HTTP_UPDATE_NO_UPDATES"));
          break;
      }
    }
    else {
      Serial.println(F( "Already on latest version" ));
    }
  }
  else {
    Serial.print(F( "Firmware version check failed, got HTTP response code " ));
    Serial.println( httpCode );
  }
  httpClient.end();
}

String InfoPage(){
  String page = "<html lang='en'><head><meta charset='utf-8'>";
  page += "<link rel='stylesheet' href='site.css'><script src='/w3.js'></script><title>";
  page += F("MQTT Button</title></head><body>");
  page += F("<div class='main'>");
  page +=   F("<div class='header'>");
  page +=     "<img src='/logo-trans.png' class='headerimg'>";
  page +=   "</div>";
  page +=   "<div class='left'>";
  page +=     "<h3>Software Info</h3>";
  page +=     F("<dl>");
  page +=       F("<dt>Firmware Version</dt><dd>");
  page +=         FW_VERSION;
  page +=     F("</dd>");
  page +=       F("<dt>Model Version</dt><dd>");
  page +=         "MQTT";
  page +=     F("</dd></dl>");
  page +=     "<h3>Device Info</h3>";
  page +=     F("<dl>");
  page +=       F("<dt>Chip ID</dt><dd>");
  page +=         ESP.getChipId();
  page +=       F("</dd>");
  page +=       F("<dt>Flash Chip ID</dt><dd>");
  page +=         ESP.getFlashChipId();
  page +=       F("</dd>");
  page +=       F("<dt>IDE Flash Size</dt><dd>");
  page +=         ESP.getFlashChipSize();
  page +=       F(" bytes</dd>");
  page +=       F("<dt>Real Flash Size</dt><dd>");
  page +=         ESP.getFlashChipRealSize();
  page +=       F(" bytes</dd>");
  page +=       F("<dt>Uptime</dt><dd>");
  page +=         Day;
  page +=         F(" Days ");
  page +=         Hour;
  page +=         F(" Hours ");
  page +=         Minute;
  page +=         F(" Minutes ");
  page +=         Second;
  page +=         F(" Seconds");
  page +=       F("</dd></dl>");
  page +=     "<h3>Network Info</h3>";
  page +=     F("<dl>");
  page +=       F("<dt>Station MAC</dt><dd>");
  page +=         WiFi.macAddress();
  page +=       F("</dd>");
  page +=       F("<dt>IP Address</dt><dd>");
  page +=         spip[0];
  page +=       F(".");
  page +=         spip[1];
  page +=       F(".");
  page +=         spip[2];
  page +=       F(".");
  page +=         spip[3];
  page +=       F("</dd>");
  page +=       F("<dt>Default Gateway</dt><dd>");
  page +=         spgw[0];
  page +=       F(".");
  page +=         spgw[1];
  page +=       F(".");
  page +=         spgw[2];
  page +=       F(".");
  page +=         spgw[3];
  page +=       F("</dd>");
  page +=       F("<dt>Subnet Mask</dt><dd>");
  page +=         spsn[0];
  page +=       F(".");
  page +=         spsn[1];
  page +=       F(".");
  page +=         spsn[2];
  page +=       F(".");
  page +=         spsn[3];
  page +=       F("</dd>");
  page +=       F("<dt>DNS Server</dt><dd>");
  page +=         spns[0];
  page +=       F(".");
  page +=         spns[1];
  page +=       F(".");
  page +=         spns[2];
  page +=       F(".");
  page +=         spns[3];
  page +=       F("</dd>");
  page +=       F("<dt>SSID</dt><dd>");
  page +=         spssid;
  page +=       F("</dd>");
  page +=       F("<dt>Password</dt><dd>");
  page +=         sppassword;
  page +=       F("</dd>");
  page +=       F("<dt>HostName</dt><dd>");
  page +=         splocalhost;
  page +=       F(".local</dd></dl>");
  page +=     F("</div>");
  page +=     "<div id='wrapper'>";
  page +=     "<div><form action='/reboot'><input type='submit' class='btn-style' value='Reboot' /></form></div>";
  page +=     "<div id='flexcenter'>&nbsp</div>";
  page +=     "<div>&nbsp;</div>";
  page +=     "</div>";
  page +=     "<div w3-include-html='footer.html'></div> ";
  page +=   "</div>";
  page +=   "<script>w3.includeHTML();</script>";
  page += "</body></html>";
  return page;
};

String IndexPage(){
  String page = "<html lang='en'><head><meta charset='utf-8'>";
  page += "<link rel='stylesheet' href='site.css'><script src='/w3.js'></script><title>";
  page += " MQTT Button</title></head><body>";
  page += "<div class='main'>";
  page +=   "<div class='header'>";
  page +=     "<img src='/logo-trans.png' class='headerimg'>";
  page +=   "</div>";
  page +=   "<div class='left'>";
  page +=     "<h1>MQTT : Button Controller</h1>";
  page +=     F("<dl>");
  page +=       F("<dt>Firmware Version</dt><dd>");
  page +=         FW_VERSION;
  page +=     F("</dd></dl>");
  page +=   "</div><br>";
  page +=     "<div w3-include-html='footer.html'></div> ";
  page +=   "</div>";
  page +=   "<script>w3.includeHTML();</script></div>";
  page += "</body></html>";
  return page;
}


bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file"));
    return false;
  }
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println(F("Config file size is too large"));
    return false;
  }
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println(F("Failed to parse config file"));
    return false;
  }
  spssid = json["ssid"].as<String>();
  sppassword = json["password"].as<String>();
  splocalhost = json["localhost"].as<String>();
  spfwUrlBase = json["fwUrlBase"].as<String>();
 
  //IP Address
  spip[0]=json["ip"][0];
  spip[1]=json["ip"][1];
  spip[2]=json["ip"][2];
  spip[3]=json["ip"][3];

  //Gateway Address
  spgw[0]=json["gw"][0];
  spgw[1]=json["gw"][1];
  spgw[2]=json["gw"][2];
  spgw[3]=json["gw"][3];

  //DNS Address
  spns[0]=json["ns"][0];
  spns[1]=json["ns"][1];
  spns[2]=json["ns"][2];
  spns[3]=json["ns"][3];

  //Subnet Mask
  spsn[0]=json["sn"][0];
  spsn[1]=json["sn"][1];
  spsn[2]=json["sn"][2];
  spsn[3]=json["sn"][3];

  int ip0=json["ns"][0];
  int ip1=json["ns"][1];
  int ip2=json["ns"][2];
  int ip3=json["ns"][3];
  char ipa[15];
 return true;
}
void onConnected(const WiFiEventStationModeConnected& event){
  Serial.println ( "Connected to AP." );
  isConnected = 1;    
}

void onDisconnect(const WiFiEventStationModeDisconnected& event){
  Serial.println ( "WiFi On Disconnect." );
  isConnected = 0;
}

void onGotIP(const WiFiEventStationModeGotIP& event){
  Serial.print ( "Got Ip: " );
  isConnected = 2;
  Serial.println(WiFi.localIP());   
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

    if (strcmp(topic,mqtt_status2)==0) {
        Serial.print("Message:");
        for (int i = 0; i < length; i++) {
          Serial.print((char)payload[i]);
          devstate2 =  (char)payload[i]; 
        }
    }
 
    if (strcmp(topic,mqtt_status1)==0) {
        Serial.print("Message:");
        for (int t = 0; t < length; t++) {
          Serial.print((char)payload[t]);
          devstate =  (char)payload[t]; 
        }
    }  
        
  Serial.println();
  Serial.println("-----------------------");
}

void setup() {
  WiFi.disconnect() ;
  WiFi.persistent(false);
  initHardware();
//  SPIFFS.begin();                           // Start the SPI Flash Files System
   if (SPIFFS.begin()) {
  Serial.println("mounted file system");

  // parse json config file
  File jsonFile = SPIFFS.open(MQTT_CONFIG,"r");
  if (jsonFile) {
    // Allocate a buffer to store contents of the file.
    size_t size = jsonFile.size();
    std::unique_ptr<char[]> jsonBuf(new char[size]);
    jsonFile.readBytes(jsonBuf.get(), size);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(jsonBuf.get());
    if (json.success()) {
      strcpy(mqtt_server, json["mqtt_server"]);
      mqtt_port = json["mqtt_port"];
      strcpy(mqtt_user, json["mqtt_user"]);
      strcpy(mqtt_pass, json["mqtt_pass"]);
      strcpy(mqtt_switch1, json["mqtt_topic1"]);
      strcat(mqtt_switch1, "/switch");
      strcpy(mqtt_switch2, json["mqtt_topic2"]);
      strcat(mqtt_switch2, "/switch");
      strcpy(mqtt_status1, json["mqtt_topic1"]);
      strcat(mqtt_status1, "/status");
      strcpy(mqtt_status2, json["mqtt_topic2"]);
      strcat(mqtt_status2, "/status");
      
    } else {
      Serial.println("failed to load json config");
    }
    jsonFile.close();
  }
}
  loadConfig();    
  mConnectHandler = WiFi.onStationModeConnected(onConnected);
  mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
  mGotIpHandler = WiFi.onStationModeGotIP(onGotIP);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  timer.setInterval(600000, checkForUpdates);
  HTTPUpdateConnect();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED && reqConnect > reqConnectNum && isConnected <2){ 
    reqConnect =  0 ;
    isConnected = 0 ;
    WiFi.disconnect() ;
    connectWiFi();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    reqConnect++ ;
    if (isConnected < 2){
      Serial.print(".");
    }
  }
  
  if (isConnected == 2) {
    //Do here what is needed when the ESP8266 is connected
      while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
 
    if (client.connect(splocalhost.c_str(), mqtt_user, mqtt_pass )) {
 
      Serial.println("connected");  
      client.publish("light/test", splocalhost.c_str(), false);
      client.subscribe(mqtt_status1);
      client.subscribe(mqtt_status2);
    } else {
 
      Serial.print("failed with state ");
      Serial.print(client.state());
 
    }
  }
 
  // Start the webserver
  server.handleClient();
  timer.run();
  // Update the Bounce instance :
   debouncer1.update();
   debouncer2.update();
   // Call code if Bounce fell (transition from HIGH to LOW) :
   if ( debouncer1.fell() ) {
      if (devstate == 48) {
        client.publish(mqtt_switch1, "1",false);
        Serial.println(F("Turning On"));
      }
      else{
        client.publish(mqtt_switch1, "0",false);
        Serial.println(F("Turning Off"));
      }
   }
   // Call code if Bounce2 fell (transition from HIGH to LOW) :
   if ( debouncer2.fell() ) {
      if (devstate2 == 48) {
        client.publish(mqtt_switch2, "1",false);
        Serial.println(F("Turning 2 On"));
      }
      else{
        client.publish(mqtt_switch2, "0",false);
        Serial.println(F("Turning 2 Off"));
      }
   }
   client.loop();
  }
}
