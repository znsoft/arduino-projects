#include <ArduinoJson.h>

#include <Arduino.h>
#include "pin_mux_register.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <DNSServer.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <IRremoteESP8266.h>

int RECV_PIN = 2; //an IR detector/demodulatord is connected to GPIO pin 2

IRrecv irrecv(RECV_PIN);

decode_results results;


int lightness = 1000;

int ButtonPin = 1;//TX
const uint16_t PixelCount = 150; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
const RgbColor CylonEyeColor(HtmlColor(0x7f0000));

RgbColor currentColor = NULL;

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// for esp8266 omit the pin
//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount);

NeoPixelAnimator animations(2); // only ever need 2 animations

int speedPixel = 1;
uint16_t lastPixel = 0; // track the eye position
int8_t moveDir = 1; // track the direction of movement
uint16_t G_idxPixel;


const char* ssid = "LeraTree";

#define LED 13

#define IN false
#define OUT true
#define HI true
#define LO false

int cycleCount;
int incremet;
int currentSpeed = 1;

enum Ledmode {
  RunMode,
  FillMode,
  StarsMode,
  PingPongMode,
  RainbowMode,
  RandomMode,
  SlowMoveMode,
  Strobo,
  DemoAll,
  Prog
};

Ledmode currentMode;


// uncomment one of the lines below to see the effects of
// changing the ease function on the movement animation
AnimEaseFunction moveEase =
  //      NeoEase::Linear;
  //      NeoEase::QuadraticInOut;
  //      NeoEase::CubicInOut;
  NeoEase::QuarticInOut;
//      NeoEase::QuinticInOut;
//      NeoEase::SinusoidalInOut;
//      NeoEase::ExponentialInOut;
//      NeoEase::CircularInOut;




void FillAll(RgbColor color) {
  if (color == NULL)
    color = strip.GetPixelColor(strip.PixelCount() - 1);
  for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
  {
    strip.SetPixelColor(indexPixel, color);
  }



}


void MoveAll() {
  RgbColor color = strip.GetPixelColor(strip.PixelCount() - 1);
  for (uint16_t indexPixel = 1; indexPixel < strip.PixelCount(); indexPixel++)
  {
    color = strip.GetPixelColor(indexPixel);
    strip.SetPixelColor(indexPixel - 1, color);
  }
  color = strip.GetPixelColor(0);
  strip.SetPixelColor(strip.PixelCount() - 1, color);
}

void FadeAll(uint8_t darkenBy)
{
  RgbColor color;
  for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
  {
    color = strip.GetPixelColor(indexPixel);
    color.Darken(darkenBy);
    strip.SetPixelColor(indexPixel, color);
  }
}

void FadeAnimUpdate(const AnimationParam& param)
{
  if (param.state == AnimationState_Completed)
  {
    FadeAll(10);
    animations.RestartAnimation(param.index);
  }
}

void MoveAnimUpdate(const AnimationParam& param)
{
  // apply the movement animation curve
  float progress = moveEase(param.progress);

  // use the curved progress to calculate the pixel to effect
  uint16_t nextPixel;
  if (moveDir > 0)
  {
    nextPixel = progress * PixelCount;
  }
  else
  {
    nextPixel = (1.0f - progress) * PixelCount;
  }

  // if progress moves fast enough, we may move more than
  // one pixel, so we update all between the calculated and
  // the last
  if (lastPixel != nextPixel)
  {
    for (uint16_t i = lastPixel + moveDir; i != nextPixel; i += moveDir)
    {
      strip.SetPixelColor(i, CylonEyeColor);
    }
  }
  strip.SetPixelColor(nextPixel, CylonEyeColor);

  lastPixel = nextPixel;

  if (param.state == AnimationState_Completed)
  {
    // reverse direction of movement
    moveDir *= -1;

    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);
  }
}

void SetupAnimations()
{
  // fade all pixels providing a tail that is longer the faster
  // the pixel moves.
  animations.StartAnimation(0, 5, FadeAnimUpdate);

  // take several seconds to move eye fron one side to the other
  animations.StartAnimation(1, 2000, MoveAnimUpdate);
}


WebSocketsServer webSocket = WebSocketsServer(81);



void SetPixel(RgbColor color) {
  strip.SetPixelColor(strip.PixelCount() - 1 , color);
}



// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  uint16_t i = incremet % strip.PixelCount();
  strip.SetPixelColor(i, c);
  strip.Show();
  delay(wait);

}

void rainbow(uint8_t wait) {
  uint16_t i, j = incremet  & 255;
  //for (j = 0; j < 256; j++) {
  for (i = 0; i < strip.PixelCount(); i++) {
    strip.SetPixelColor(i, Wheel((i + j) & 255));
  }
  //  strip.Show();
  //  delay(wait);
  // }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j = incremet % (256 * 5);
  // for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
  for (i = 0; i < strip.PixelCount(); i++) {
    strip.SetPixelColor(i, Wheel(((i * 256 / strip.PixelCount()) + j) & 255));
  }
  // strip.Show();
  //  delay(wait);
  // }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  int j = incremet & 255;

  //  for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
  for (int q = 0; q < 3; q++) {
    for (uint16_t i = 0; i < strip.PixelCount(); i = i + 3) {
      strip.SetPixelColor(i + q, c);  //turn every third pixel on
    }
    strip.Show();

    delay(wait);

    for (uint16_t i = 0; i < strip.PixelCount(); i = i + 3) {
      strip.SetPixelColor(i + q, 0);      //turn every third pixel off
    }
  }
  // }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  int j = incremet % 256;

  //for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
  for (int q = 0; q < 3; q++) {
    for (uint16_t i = 0; i < strip.PixelCount(); i = i + 3) {
      strip.SetPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
    }
    strip.Show();

    delay(wait);

    for (uint16_t i = 0; i < strip.PixelCount(); i = i + 3) {
      strip.SetPixelColor(i + q, 0);      //turn every third pixel off
    }
  }
  //}
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
RgbColor Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return RgbColor(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return RgbColor(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return RgbColor(WheelPos * 3, 255 - WheelPos * 3, 0);
}


void Stars() {
  FadeAll(1);
  RgbColor color;
  byte c = (255 * lightness) / 1000;
  if ((rand() % 6) == 1) {

    color = RgbColor( c,  c,  c);
    //color.Darken(darkenBy);
    strip.SetPixelColor(G_idxPixel , color);
  }
}



void StripEffects( ) {
  RgbColor color;
  G_idxPixel++;
  incremet++;
  byte strobo = (incremet & currentSpeed) == 0 ? 255 : 0;
  if (G_idxPixel >= strip.PixelCount()) {
    G_idxPixel = 0;
    cycleCount++;
  }
  switch (currentMode) {

    case RunMode:
      if ((G_idxPixel % 5) == 0)MoveAll();
      break;
    case FillMode:
      FillAll(NULL);
      break;
    case Strobo:

      color = RgbColor(strobo, strobo, strobo);
      FillAll(color);
      //delay(currentSpeed / 50);
      break;
    case StarsMode:
      Stars();
      break;
    case PingPongMode:
      theaterChaseRainbow(50);
      break;

    case DemoAll:

      switch (cycleCount % 5) {

        case 0:
          rainbow(20);
          break;
        case 1:
          rainbowCycle(20);
          break;
        case 2:
          //
          rainbowCycle(20);
          break;
        case 3:
          rainbowCycle(30);
          break;
        case 4:
          FadeAll(1);
          break;
        case 5:
          Stars();
          break;
      }

      break;

    case SlowMoveMode:
      color = strip.GetPixelColor(G_idxPixel + 1);
      strip.SetPixelColor(G_idxPixel, color);

      break;

    case RainbowMode:

      FadeAll(3);
      color = RgbColor(rand() & 255, rand() & 255, rand() & 255);
      strip.SetPixelColor(G_idxPixel , color);


      break;

    case RandomMode:
      //uint32_t rgb = (uint32_t) rand()&255 * rand() * (rand() % 3);
      color = RgbColor(rand() & 255, rand() & 255, rand() & 255);
      strip.SetPixelColor(G_idxPixel , color);
      break;


  }
}


ADC_MODE(ADC_VCC);

ESP8266WebServer server(80);
std::unique_ptr<DNSServer>        dnsServer;



void doProg(uint8_t * payload) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject&  root = jsonBuffer.parseObject((const char *) &payload[0]);
  if (!root.success())return;

  JsonArray& pixels = root["p"];
  currentMode = Prog;
  long offset = root["o"];
  //JsonArray& pixels = root["p"];
  for (long i = 0; i < pixels.size(); i = i + 1) {
    uint32_t rgb = (uint32_t)pixels[i];
    RgbColor color = HtmlColor(rgb);
    strip.SetPixelColor((uint16_t)(i + offset), color ); 
  }
  strip.Show();

}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  String vol = String(ESP.getVcc(), DEC);
  String pong = String(results.value, HEX);
  uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
  RgbColor color = HtmlColor(rgb);

  switch (type) {
    case WStype_DISCONNECTED:

      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        webSocket.sendTXT(num, vol);
        //digitalWrite(LED, HIGH );
      }
      //webSocket.sendTXT(num, "C");
      break;
    case WStype_TEXT:

      // send data to all connected clients
      // webSocket.broadcastTXT("message here");

      switch (payload[0]) {
        case '$':
          lightness = rgb;


          break;
        case '#':
          SetPixel(color);
          break;

        case '{':

          doProg(payload);


          break;


        case 'b': case 'B'://bflksonrm
          currentMode = RunMode;
          break;

        case 'p': // ping, will reply pong

          webSocket.sendTXT(num, pong );
          return;
          break;

        case 'f': case 'F':   //Echo
          currentMode = FillMode;
          break;

        case 'l': case 'L':   //Echo
          currentMode = RunMode;
          break;

        case 'k': case 'K':   //Echo
          currentMode = RainbowMode;
          break;

        case 's': case 'S':   //Echo
          currentMode = DemoAll;
          break;

        case 'o': case 'O': //speed
          speedPixel = (int)rgb;
          break;


        case 'n': case 'N':   //Echo
          currentMode = RandomMode;
          break;

        case 'r': case 'R':   //Echo
          currentMode = StarsMode;
          break;

        case 'm': case 'M':   //Echo
          currentMode = SlowMoveMode;
          break;

        case 'x': case 'X':   //Echo
          currentMode = PingPongMode;
          break;

        case 'e': case 'E':   //Echo
          currentMode = Strobo;
          break;

        default:
          break;

      }
      webSocket.sendTXT(num, vol);
      break;

    case WStype_BIN:
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
  currentMode = RandomMode;
  pinMode(ButtonPin, INPUT);
  digitalWrite(ButtonPin, LOW );
  strip.Begin();
  FillAll(HtmlColor(0x7f1f7f));
  strip.Show();
  //irrecv.enableIRIn();

  //SetupAnimations();

  //  pinMode(FW, OUTPUT);
  //  digitalWrite(FW, LOW );

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


int enumIter;
int pressTime;


void loop() {

  dnsServer->processNextRequest();

  server.handleClient();

  webSocket.loop();
  StripEffects( );


  strip.Show();
  /*
    if (irrecv.decode(&results)) {
    // Serial.println(results.value, HEX);
     RgbColor color = HtmlColor(results.value);
     FillAll(color);
     currentMode = FillMode;
    irrecv.resume(); // Receive the next value
    pressTime = 0;
    }
  */
  if (digitalRead(ButtonPin) == 1)pressTime++; else pressTime = 0;


  if (pressTime > 30) {
    enumIter++;
    pressTime = 0;
    currentMode = (Ledmode)(enumIter % ((int)DemoAll + 1));
    //delay(500);
  }



}


