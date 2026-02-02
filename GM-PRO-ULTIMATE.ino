#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include "SSD1306Wire.h"

extern "C" {
  #include "user_interface.h"
  void wifi_send_pkt_freedom(unsigned char *buf, unsigned short len, bool sys_seq);
}

SSD1306Wire display(0x3c, D2, D1, GEOMETRY_64_48); 
DNSServer dnsServer;
ESP8266WebServer server(80);

bool isDeauthing = false;

// KODE HTML YANG MEMBENTUK TAMPILAN DI GAMBAR KAMU
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
    body { background: #050505; color: #00ff41; font-family: 'Courier New', monospace; padding: 10px; margin: 0; text-shadow: 0 0 5px #00ff41; }
    .header { text-align: center; border-bottom: 2px solid #00ff41; padding-bottom: 10px; margin-bottom: 10px; }
    .control-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
    .btn { padding: 12px; text-align: center; font-weight: bold; text-decoration: none; border-radius: 4px; border: 1px solid #00ff41; background: #000; color: #00ff41; font-size: 11px; cursor: pointer; }
    .on { background: #ff0000; color: #fff; border: none; box-shadow: 0 0 15px #ff0000; }
    .sel-btn { padding: 5px 10px; background: #00ff41; color: #000; border: none; font-size: 10px; font-weight: bold; border-radius: 3px; }
    table { width: 100%; border-collapse: collapse; margin-top: 15px; background: rgba(0,20,0,0.3); }
    th { text-align: left; border-bottom: 2px solid #00ff41; padding: 8px; font-size: 10px; color: #888; }
    td { padding: 10px 8px; border-bottom: 1px solid #222; font-size: 11px; }
    .selected { background: rgba(0, 255, 65, 0.2); border: 1px solid #00ff41; }
    pre { background: #000; border: 1px solid #333; padding: 10px; height: 90px; overflow-y: scroll; color: #00ff41; font-size: 9px; margin-top: 10px; }
    .box { border: 1px solid #333; padding: 12px; border-radius: 8px; margin-top: 15px; }
</style></head><body>
    <div class="header"><h2 style="margin:0;">⚡ GM-PRO <span style="color:red;">ULTIMATE</span> ⚡</h2><small>🛰️ Control Center: vivo1904</small></div>
    <div class="control-grid">
        <a href="/deauth" class="btn on">☢️ DEAUTH</a>
        <a href="#" class="btn" style="background:#0055ff; color:#fff; border:none;">🛰️ BEACON CLONE</a>
    </div>
    <table>
        <tr><th>SSID</th><th>CH</th><th>SIGNAL</th><th>ACT</th></tr>
        <tr class="selected"><td>🎯 Target_WiFi</td><td>6</td><td>85%</td><td><a href="/deauth" class="sel-btn" style="background:red; color:white;">TARGET</a></td></tr>
    </table>
    <h4>📝 CRITICAL LOGS</h4><pre>[🚀] System Online...<br>[📡] WiFi Scanner Active<br>[🛡️] GM-PRO Engine Ready</pre>
    <div class="box"><a href="#" style="background:#00ff41; color:#000; display:block; text-align:center; padding:12px; text-decoration:none; font-weight:bold; border-radius:5px;">📂 BUKA BRANKAS 🔑</a></div>
</body></html>
)=====";

void setup() {
  display.init();
  display.flipScreenVertically();
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAP("vivo1904", "sangkur87"); 
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/deauth", []() {
    isDeauthing = !isDeauthing;
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.onNotFound([]() { server.send_P(200, "text/html", INDEX_HTML); });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  display.clear();
  display.drawString(0, 0, "GM-PRO V3.9");
  display.drawString(0, 15, isDeauthing ? "ATTACKING" : "STANDBY");
  display.display();
}
