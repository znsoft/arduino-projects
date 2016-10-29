#include "ESP8266WiFi.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
#include <DNSServer.h>
#include <ESP8266HTTPUpdateServer.h>

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
ESP8266HTTPUpdateServer httpUpdater;
IPAddress myIP;
ADC_MODE(ADC_VCC);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  // Tell Consult which Serial object to use to talk to the ECU
  myConsult.setSerial(&Serial);

  // We want MPH and Farenheit, so pass in false
  // If you want KPH or Celcius, pass in true
  myConsult.setMetric(true);


  WiFi.mode(WIFI_AP_STA);
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);//password);http://esp8266.github.io/Arduino/versions/2.1.0-rc1/doc/libraries.html
  dnsServer.setTTL(300);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNS_PORT, "www.ppap.ru", apIP);

  myIP = WiFi.softAPIP();
  MDNS.begin(host);
  server.on ( "/", handleRoot );
  server.on ( "/reset", []() {
    ESP.reset();
  } );
  server.on ( "/tx1", []() {
    pinMode(1, OUTPUT);
    pinMode(3, OUTPUT);
    digitalWrite(1, HIGH);
    digitalWrite(3, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    server.send ( 200, "text/plain", "ON" );
  } );


  server.on ( "/rx", []() {
    pinMode(1, INPUT);
    pinMode(3, INPUT);
    digitalWrite(1, LOW);
    digitalWrite(3, LOW);
    digitalWrite(LED_BUILTIN, HIGH);
    String s = " gpio1 = " + String(digitalRead(1), HEX) + "; gpio3 = " + String(digitalRead(3), HEX);
    server.send ( 200, "text/plain", s );
  } );



  server.on ( "/tx0", []() {
    pinMode(1, OUTPUT);
    pinMode(3, OUTPUT);
    digitalWrite(1, LOW);
    digitalWrite(3, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    server.send ( 200, "text/plain", "OFF" );
  } );

  server.on ( "/obd", handleOBD );
  server.on ( "/test.svg", drawGraph );
  server.on ( "/scan", Scan );
  server.on ( "/clearerrorcodes", ClearOBDErrors );
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  httpUpdater.setup(&server);

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

//..----------


void handleRoot() {

  char temp[500];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  snprintf ( temp, 500,
             "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>OBD ESP8266</h1>\
    <p>Uptime: %02d:%02d:%02d  Voltage = %d mV</p>\
    <br><a href=\"/scan\">Scan WIFI</a><br>\
    <br><a href=\"/obd\"> OBD 2 </a><br>\
    <img src=\"/test.svg\" /><br>\
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
int y = 0;
void drawGraph() {
  String out = "";

  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = ESP.getVcc() % 120;
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
      scanTemp +=  WiFi.BSSIDstr(i);
      scanTemp += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "  " : " *";
    }

  }
  scanTemp += "<br>end</body>\
</html>";
  server.send ( 200, "text/html", scanTemp);
}

void ClearOBDErrors() {
  myConsult.initEcu();
  myConsult.clearErrorCodes();
  handleOBD();
}

String GetOBDMessage() {
  String message = "<html>\
  <head>\
    <meta http-equiv='refresh' content='5;url=/obd'/>\
    <title>ESP8266 OBD Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>OBD</h1>";
  if (myConsult.initEcu() == false) {
    message += "<br>OBD CONSULT FAILED .... ";
    return message;
  }

  message += "<br>OBD INIT OK<br>Try to get last errors...";
  int numberOfErrorCodes = 0;
  ConsultErrorCode errorCode = ConsultErrorCode();
  if (myConsult.getNumberOfErrorCodes(&numberOfErrorCodes)) {
    message +=  "<br> " + String( numberOfErrorCodes , DEC ) + " Errors:   -=<a href=/clearerrorcodes>Clear</a>=-";


    // Then retrieve each code and display
    for (int x = 1; x <= numberOfErrorCodes; x++) {
      // Display which code number
      // Attempt to pull code
      if (myConsult.getErrorCode(x, &errorCode)) {
        // Got our error code
        errorCode.getCode();
        errorCode.getLastSeen();
        message +=  "<br> Er = " + String( errorCode.getCode(), HEX) + " , Last seen  " + String( errorCode.getLastSeen(), HEX);
      }
    }
  }
  message += "<br>Try to get coolantTemp";

  int coolantTemp;
  if (myConsult.getRegisterValue(ECU_REGISTER_COOLANT_TEMP, ECU_REGISTER_NULL, &coolantTemp)) {
    // coolantTemp now contains the temp, but we need to convert it to something human readable
    coolantTemp = ConsultConversionFunctions::convertCoolantTemp(coolantTemp);
    message +=  "<br>Coolant temp  = " + String( coolantTemp , DEC ) + ". ";

  }

  // Attempt to read Tachometer,
  message += "<br>Attempt to read Tachometer";

  // This register spans two registers, first pass in MSB, then LSB
  int tach;
  if (myConsult.getRegisterValue(ECU_REGISTER_TACH_MSB, ECU_REGISTER_TACH_LSB, &tach)) {
    // tach now contains the tach value, but we need to convert it to something human readable
    tach = ConsultConversionFunctions::convertTachometer(tach);
    message +=  "<br>Tachometer = " + String( tach , DEC ) + ". ";

  }

  message += "<br>getEcuPartNumber";

  char partNumber[12];
  if (myConsult.getEcuPartNumber(partNumber)) {
    message += "<br>PN = " + String(partNumber);
  }

  message += "<br>Create an Array of ConsultRegisters";
  // Create an Array of ConsultRegisters we want to read
  int numRegisters = 5;
  ConsultRegister myRegisters[numRegisters];

  // Create a register classes for each register type
  // Pass in a label to describe it, the register Addresses, and a function pointer to the function that
  // will convert the value from the ECU value to something human readable
  myRegisters[0] = ConsultRegister("Coolant", ECU_REGISTER_COOLANT_TEMP, ECU_REGISTER_NULL, &ConsultConversionFunctions::convertCoolantTemp);
  myRegisters[1] = ConsultRegister("Timing", ECU_REGISTER_IGNITION_TIMING, ECU_REGISTER_NULL, &ConsultConversionFunctions::convertIgnitionTiming);
  myRegisters[2] = ConsultRegister("Battery", ECU_REGISTER_BATTERY_VOLTAGE, ECU_REGISTER_NULL, &ConsultConversionFunctions::convertBatteryVoltage);
  myRegisters[3] = ConsultRegister("Speed", ECU_REGISTER_VEHICLE_SPEED, ECU_REGISTER_NULL, &ConsultConversionFunctions::convertVehicleSpeed);
  myRegisters[4] = ConsultRegister("Tach", ECU_REGISTER_TACH_MSB, ECU_REGISTER_TACH_LSB, &ConsultConversionFunctions::convertTachometer);

  // Now that we've defined these, we want to continuely poll the ecu for these values
  if (myConsult.startEcuStream(myRegisters, numRegisters)) {
    // Read registers 3 times from the ecu
    for (int x = 0; x < 3; x++) {
      message += "<br>Read registers 3 times from the ecu";

      // Read ecu stream
      if (myConsult.readEcuStream(myRegisters, numRegisters)) {
        // Loop thru values
        for (int y = 0; y < numRegisters; y++) {
          // Has each registers updated value
          myRegisters[y].getValue();
          message += "<br>" + String(myRegisters[y].getLabel()) + " = " + String(myRegisters[y].getValue());
        }
      }
    }
    // Stop ecu stream
    myConsult.stopEcuStream();
  }
  return message;
}


void handleOBD() {
  String message = GetOBDMessage();

  message += "<br>end</body>\
</html>";
  server.send ( 200, "text/html", message );

}


