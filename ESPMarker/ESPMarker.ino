#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Hash.h>
#include <DNSServer.h>

const char* ssid = "Free WiFi";

ESP8266WebServer server(80);
std::unique_ptr<DNSServer>        dnsServer;

  
void setup() {

  WiFi.mode(WIFI_AP);
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);
  // Connect tp Wifi

  dnsServer.reset(new DNSServer());
   /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(53, "*", WiFi.softAPIP());
  //SERVER INIT
//  server.on("/generate_204", handleRoot);  //Android/Chrome OS captive portal check.
//  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
      server.send(200, "text/plain", "<html><head><meta refresh='1;https://znsoft.github.io/'></head><body>Wait...</body></html>");
      delay(10000);
      WiFi.softAPdisconnect(true);
      delay(10000);
      setup();
      //server.client()
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.begin();
}

void loop() {

  dnsServer->processNextRequest();

  server.handleClient();

}
