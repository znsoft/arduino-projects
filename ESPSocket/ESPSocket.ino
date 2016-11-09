#include <Arduino.h>
#include "pin_mux_register.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <DNSServer.h>

WebSocketsServer webSocket = WebSocketsServer(81);

const char* ssid = "LeraCar";

#define LED 13

#define IN false
#define OUT true
#define HI true
#define LO false

#define FW 3
#define BK 1
#define RI 2
#define LE 0

ADC_MODE(ADC_VCC);

ESP8266WebServer server(80);
std::unique_ptr<DNSServer>        dnsServer;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  String vol = String(ESP.getVcc(), DEC);
  switch (type) {
    case WStype_DISCONNECTED:
        digitalWrite(FW, LOW );
        digitalWrite(BK, LOW );
        digitalWrite(RI, LOW );
        digitalWrite(LE, LOW );
        digitalWrite(LED, LOW );
      
      break;
    case WStype_CONNECTED:
    {
      IPAddress ip = webSocket.remoteIP(num);
      webSocket.sendTXT(num, vol);
      digitalWrite(LED, HIGH );
    }
     //webSocket.sendTXT(num, "C");
      break;
    case WStype_TEXT:

      // send data to all connected clients
      // webSocket.broadcastTXT("message here");

      switch (payload[0]) {
        case 'f': case 'F':
          digitalWrite(BK, LOW );
          digitalWrite(FW, HIGH );
          //webSocket.sendTXT(0,rssi);
          break;

        case 'b': case 'B':
          digitalWrite(FW, LOW );
          digitalWrite(BK, HIGH );
          break;

        case 'p': // ping, will reply pong
          webSocket.sendTXT(0, "pong");
          break;

        case 'r': case 'R':   //Echo
          //webSocket.sendTXT(0,text);
          digitalWrite(LE, LOW );
          digitalWrite(RI, HIGH );

          break;
        case 'l': case 'L':   //Echo
          //webSocket.sendTXT(0,text);
          digitalWrite(RI, LOW );
          digitalWrite(LE, HIGH );

          break;

        case 's': case 'S':   //Echo
          //webSocket.sendTXT(0,text);
          digitalWrite(FW, LOW );
          digitalWrite(BK, LOW );

          break;

        case 'n': case 'N':   //Echo
          //webSocket.sendTXT(0,text);
          digitalWrite(RI, LOW );
          digitalWrite(LE, LOW );

          break;


        default:
          //webSocket.sendTXT(0, "**** UNDEFINED ****");
          break;

      }
            webSocket.sendTXT(num, vol);
      break;

    case WStype_BIN:
      //    hexdump(payload, lenght);
      // send message to client
      webSocket.sendBIN(num, payload, lenght);
      break;
      

  }

}


//holds the current upload
File fsUploadFile;

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
  }
}

void handleFileDelete() {
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

void handleRoot() {
handleFileRead("/");
  
  }


  
void setup() {
  pinMode(FW, OUTPUT);
  pinMode(BK, OUTPUT);
  pinMode(RI, OUTPUT);
  pinMode(LE, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(FW, LOW );
  digitalWrite(BK, LOW );
  digitalWrite(RI, LOW );
  digitalWrite(LE, LOW );
  digitalWrite(LED, LOW );
  
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
    }
  }

  WiFi.mode(WIFI_AP);
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);
  // Connect tp Wifi

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
 dnsServer.reset(new DNSServer());
   /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(53, "*", WiFi.softAPIP());
  //SERVER INIT
  //list directory
  server.on("/generate_204", handleRoot);  //Android/Chrome OS captive portal check.
  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);


  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(ESP.getVcc());
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
}

void loop() {

dnsServer->processNextRequest();

  server.handleClient();

  webSocket.loop();

}
