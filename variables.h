
// WiFi Definitions.
String spssid = "";
String sppassword = "";
String splocalhost = ""; // Defines mDNS host name .local is appended
String spfwUrlBase = "";
WiFiEventHandler mDisConnectHandler;
WiFiEventHandler mConnectHandler;
WiFiEventHandler mGotIpHandler;
int reqConnect = 0 ; 
int isConnected = 0 ;
int previousMillis = 0 ;
const long interval = 500;          
const long reqConnectNum = 15; // number of intervals to wait for connection
//End Customization

//Blank IP Variables
int spip[4];
int spgw[4];
int spsn[4];
int spns[4];

// MQTT pub/sub for the button functions
#define MQTT_CONFIG  "/mqtt.json"
// these values are loaded from a config.json file in the data directory
char mqtt_server[18] = "***.mqtt.local";
int  mqtt_port       = 0;
char mqtt_user[9]    = "********";
char mqtt_pass[13]   = "************";
char mqtt_switch1[75] = "*********";
char mqtt_switch2[75] = "*********";
char mqtt_status1[75] = "*********";
char mqtt_status2[75] = "*********";

// ESP8266 Variables
String url = "";
int devstate = LOW;
int devstate2 = LOW;
File fsUploadFile;              // a File object to temporarily store the received file
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// the timer object
SimpleTimer timer;

//Definitions used for the Up Time Logger
long Day=0;
int Hour =0;
int Minute=0;
int Second=0;
int HighMillis=0;
int Rollover=0;

// Instantiate a Bounce object :
Bounce debouncer1 = Bounce(); 

// Instantiate another Bounce object
Bounce debouncer2 = Bounce(); 
