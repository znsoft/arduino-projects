
#include "ESP8266WiFi.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
//#include "OneWireSlave.h"
#include <DNSServer.h>
//typedef bool boolean;

#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif
#include "Consult.h"
#include "ConsultConversionFunctions.h"
#include "ConsultRegister.h"
#include "ConsultErrorCode.h"



// Define global myConsult object
Consult myConsult = Consult();


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


const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";


void handleRoot() {

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

}


void handleNotFound() {

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

}





void drawGraph() {
  String out = "";

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    char temp[100];
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send ( 200, "image/svg+xml", out);
}

void Scan() {


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
}


void OBDcmd(String command) {
}

void handleOBD() {
  String message = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 OBD Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>OBD</h1>";
  if (myConsult.initEcu() == false) message += "<br>OBD FAILED";
  char partNumber[12];
  if (myConsult.getEcuPartNumber(partNumber)) {
    message += partNumber;
  }
  message += "<br>end</body>\
</html>";
  server.send ( 200, "text/plain", message );

}


void setup() {

  // Tell Consult which Serial object to use to talk to the ECU
  myConsult.setSerial(&Serial);

  // We want MPH and Farenheit, so pass in false
  // If you want KPH or Celcius, pass in true
  myConsult.setMetric(false);


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
  server.on ( "/", handleRoot );
  server.on ( "/obd", handleOBD );
  server.on ( "/test.svg", drawGraph );
  server.on ( "/scan", Scan );
  server.on ( "/inline", []() {
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
}


void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
