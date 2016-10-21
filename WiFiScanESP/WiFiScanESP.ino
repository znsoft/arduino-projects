#include "ESP8266WiFi.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
#include <DNSServer.h>
#include <Servo.h> 
 
Servo myservo;  // create servo object to control a servo 
                // twelve servo objects can be created on most boards
 
#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif
/* Set these to your desired credentials. */
const char* host = "ppap";
const char *ssid = "PPAP";
const char *password = "12345678";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

ESP8266WebServer server ( 80 );
IPAddress myIP;
ADC_MODE(ADC_VCC);
const int led = 13;

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
int pos = 180;


void handleRoot() {
  digitalWrite ( led, 1 );
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf ( temp, 400,

             "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <br><a href=\"/scan\">Scan WIFI</a><br>\
    <img src=\"/test.svg\" /><br>Voltage = %d\
  </body>\
</html>",
             hr, min % 60, sec % 60, ESP.getVcc()
           );
  server.send ( 200, "text/html", temp );
  digitalWrite ( led, 0 );
}


void handleNotFound() {
  digitalWrite ( led, 1 );
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n<br><a href=/test.svg>TEST GRAPH</a><br><a href=/inline>INline</a>";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  digitalWrite ( led, 0 );
}


void setup() {

  Serial.begin(115200);
  pinMode ( led, OUTPUT );
  digitalWrite ( led, 0 );
  delay(1000);
  Serial.print("Configuring access point...");
  WiFi.mode(WIFI_AP_STA);
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);//password);http://esp8266.github.io/Arduino/versions/2.1.0-rc1/doc/libraries.html
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  //  WiFi.disconnect();

  // modify TTL associated  with the domain name (in seconds)
  // default is 60 seconds
  dnsServer.setTTL(300);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is DNSReplyCode::NonExistentDomain
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server for a specific domain name
  dnsServer.start(DNS_PORT, "www.ppap.ru", apIP);



  myIP = WiFi.softAPIP();
  MDNS.begin(host);
  Serial.println(myIP);
  server.on ( "/", handleRoot );
  server.on ( "/test.svg", drawGraph );
  server.on ( "/scan", Scan );
  server.on ( "/inline", []() {
    pos ++;
//    pos = pos%180;
    myservo.write(pos&127);
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  server.on("/firmware", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });

  server.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(server.client());
  });

  server.begin();

  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("Philips hue clone");
  SSDP.setSerialNumber("001788102201");
  SSDP.setURL("index.html");
  SSDP.setModelName("Philips hue bridge 2012");
  SSDP.setModelNumber("929000226503");
  SSDP.setModelURL("http://www.meethue.com");
  SSDP.setManufacturer("Royal Philips Electronics");
  SSDP.setManufacturerURL("http://www.philips.com");
  SSDP.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.println ( "HTTP server started" );
  myservo.attach(2);  // attaches the servo on GIO2 to the servo object 

  Serial.println("Setup done");
}


void drawGraph() {
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send ( 200, "image/svg+xml", out);
}

void Scan() {

  digitalWrite ( led, 1 );
  String scanTemp;

  //char temp[400], scanTemp[500];
  int sec = millis() / 1000;
  int n = WiFi.scanNetworks();
  sec = (millis() / 1000) - sec;


  scanTemp = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Scan</h1>";


  if (n == 0)
    scanTemp +=  "<br>-no networks found";
  else
  {
    scanTemp += "<br>----networks---- " + String( n , DEC );
    for (int i = 0; i < n; i++) {
      scanTemp +=  "<br>" + String( i , DEC ) + ". ";
      scanTemp += WiFi.SSID(i);
      scanTemp += " (";
      scanTemp +=  WiFi.RSSI(i);
      scanTemp +=  ")";
      scanTemp += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";

    }

  }
  scanTemp += "<br>end</body>\
</html>";
  server.send ( 200, "text/html", scanTemp);

  digitalWrite ( led, 0 );


}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
