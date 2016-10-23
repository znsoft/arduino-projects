
#include "ESP8266WiFi.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
//#include "OneWireSlave.h"
#include <DNSServer.h>

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






String debugstring;
//------------------------------------------------------------
void MyDebug(String s) {

  //      Serial.print(s);
}

void MyDebug(char s) {

  //      Serial.print(s);
}



#define K_IN  3
#define K_OUT 1

#define READ_ATTEMPTS 125

char command;                                                 //Terminal Commands

char pid_reslen[] =                                         //PID Lengths
{
  // pid 0x00 to 0x1F
  4, 4, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
  2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 4,

  // pid 0x20 to 0x3F
  4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1,
  1, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2,

  // pid 0x40 to 0x4E
  4, 8, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2
};

long tempLong;
char str[40];


int obdConnect() {
  MyDebug("Attempting ECU initialization...");
  if (iso_init() == 0) {
    MyDebug("PASS");
    return 0;
  }
  else {
    MyDebug("FAIL");
    return 1;
  }
}


byte iso_init()
{
  byte b;

  serial_tx_off();                             //disable UART so we can "bit-Bang" the slow init.
  serial_rx_off();
  delay(3000);                                 //k line should be free of traffic for at least two secconds.


  digitalWrite(K_OUT, HIGH);                // drive K line high for 300ms
  delay(300);

  // send 0x33 at 5 bauds

  digitalWrite(K_OUT, LOW);                  // start bit
  delay(200);

  b = 0x33;
  for (byte mask = 0x01; mask != 0; mask <<= 1)
  {
    if (b & mask)
      digitalWrite(K_OUT, HIGH);
    else
      digitalWrite(K_OUT, LOW);
    delay(200);
  }

  digitalWrite(K_OUT, HIGH);                // stop bit + 60 ms delay
  delay(260);


  serial_rx_on();                       // switch now to 10400 bauds


  byte i = 0;                                  // wait for 0x55 from the ECU (up to 300ms)
  while (i < 3 && !iso_read_byte(&b)) {
    i++;
  }

  MyDebug(b);

  if (b != 0x55) {
    return 1;
  }

  iso_read_byte(&b);
  MyDebug(b);                    // wait for kw1 and kw2
  iso_read_byte(&b);
  MyDebug(b);
  iso_write_byte(~b);                         // send ~kw2 (invert of last keyword)
  iso_read_byte(&b);
  MyDebug(b);          // ECU answer by 0xCC (~0x33)
  if (b != 0xCC)
    return 1;

  return 0;
}



void serial_rx_off() {
  pinMode(K_IN, OUTPUT);
  //UCSR0B &= ~(_BV(RXEN0));
}

void serial_tx_off() {
  pinMode(K_OUT, OUTPUT);
  //UCSR0B &= ~(_BV(TXEN0));
  delay(20);                                 //allow time for buffers to flush
}

void serial_rx_on() {
  pinMode(K_OUT, OUTPUT);
  pinMode(K_IN,  INPUT_PULLUP);
  Serial.begin(10400);
}


boolean iso_read_byte(byte * b)
{
  int readData;
  boolean success = true;
  byte t = 0;

  while (t != READ_ATTEMPTS  && (readData = Serial.read()) == -1) {
    delay(1);
    t++;
  }
  if (t >= READ_ATTEMPTS) {
    success = false;
  }
  if (success)
  {
    *b = (byte) readData;
  }

  return success;
}

void iso_write_byte(byte b)
{
  serial_rx_off();
  Serial.print(b);
  delay(10);
  serial_rx_on();
}


byte iso_checksum(byte *data, byte len)
{
  byte i;
  byte crc;

  crc = 0;
  for (i = 0; i < len; i++)
    crc = crc + data[i];

  return crc;
}

byte iso_write_data(byte *data, byte len)
{
  byte i, n;
  byte buf[20];

  // ISO header
  buf[0] = 0x68;
  buf[1] = 0x6A;      // 0x68 0x6A is an OBD-II request
  buf[2] = 0xF1;      // our requesterХs address (off-board tool)


  // append message
  for (i = 0; i < len; i++)
    buf[i + 3] = data[i];

  // calculate checksum
  i += 3;
  buf[i] = iso_checksum(buf, i);

  // send char one by one
  n = i + 1;
  for (i = 0; i < n; i++)
  {
    iso_write_byte(buf[i]);
  }

  return 0;
}

// read n byte(s) of data (+ header + cmd and crc)
// return the count of bytes of message (includes all data in message)
byte iso_read_data(byte *data, byte len)
{
  byte i;
  byte buf[20];
  byte dataSize = 0;

  for (i = 0; i < len + 6; i++)
  {
    if (iso_read_byte(buf + i))
    {
      dataSize++;
    }
  }

  memcpy(data, buf + 5, len);
  delay(55);    //guarantee 55 ms pause between requests
  return dataSize;
}

boolean get_pid(byte pid, char *retbuf, long *ret)
{
  byte cmd[2];                                                    // to send the command
  byte buf[10];                                                   // to receive the result

  byte reslen = pid_reslen[pid];                                 // receive length depends on pid

  cmd[0] = 0x01;                                                  // ISO cmd 1, get PID
  cmd[1] = pid;

  iso_write_data(cmd, 2);                                         // send command, length 2

  if (!iso_read_data(buf, reslen))                                 // read requested length, n bytes received in buf
  {
    MyDebug("ISO Read Data Error.");
    return false;
  }

  *ret = buf[0] * 256U + buf[1];                                   // a lot of formulas are the same so calculate a default return value here

  MyDebug("Return Value ");
  MyDebug(pid);
  MyDebug(" : ");
  //MyDebug(*ret);

  return true;
}


//---------------------------------------------

void setup() {
  //ds.init(rom);
  // ds.waitForRequest(false);
  pinMode(K_IN, INPUT);
  pinMode(K_OUT, OUTPUT);
  digitalWrite(K_OUT, LOW);
  MyDebug("-=Arduino OBD2 Terminal=-");

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
  if (command == "1")    obdConnect();
  if (command == "2") get_pid(0x01, str, &tempLong);
  if (command == "3") get_pid(0x03, str, &tempLong);
  if (command == "4") get_pid(0x10, str, &tempLong);
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

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    OBDcmd(server.arg ( i ));
    char temp[100];
    sprintf(temp, "\"%d\" <br>", tempLong);

    message += temp;
  }
 message += "<br>end</body>\
</html>";
  server.send ( 200, "text/plain", message );

}


void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
