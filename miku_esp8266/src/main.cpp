/**
 * Part of miku- opensource smart air purifier project subjected to terms of
 * MIT license agreement. A license file is distributed with
 * the project.
 * @Author: Ritesh Sharma
 * @Date: 31-12-2025
 * @Detail: entry point of the application
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define LED_PIN D2
#define DUST_PIN A0
#define RELAY_PIN D1

ESP8266WebServer server(80);

bool purifierOn = false;

/* ===== WiFi Settings ===== */
const char* ssid = "YOUR_WIFI_SSID";  // Enter SSID here
const char* password = "YOUR_WIFI_PASSWORD";  //Enter Password here

/* ===== GP2Y1010 Read ===== */
float readPM25() {
  digitalWrite(LED_PIN, LOW);
  delayMicroseconds(280);
  int adc = analogRead(DUST_PIN);
  delayMicroseconds(40);
  digitalWrite(LED_PIN, HIGH);
  delayMicroseconds(9680);

  float voltage = adc * (5 / 1024.0);
  float dustDensity = max(0.0, 170.0 * (voltage - 0.1));
  return dustDensity;
}

/* ===== HTML UI ===== */
String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {
  font-family: Arial;
  background: #f4f6f8;
  display: flex;
  justify-content: center;
  padding-top: 20px;
}

.container {
  width: 320px;
}

.banner {
  display: flex;
  justify-content: center;
  margin-bottom: 15px;
}

.banner img {
  width: 100%;
  max-width: 320px;
  border-radius: 12px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.1);
}

.card {
  background: white;
  border-radius: 20px;
  padding: 20px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.1);
}

.title {
  font-size: 22px;
  font-weight: 600;
}

.subtitle {
  color: #888;
  font-size: 14px;
}

.gauge {
  margin: 30px auto;
  width: 180px;
  height: 180px;
  border-radius: 50%;
  background: conic-gradient(#ffa726 var(--deg), #eee 0deg);
  display: flex;
  align-items: center;
  justify-content: center;
}

.gauge span {
  font-size: 36px;
  font-weight: bold;
}

.btn {
  width: 100%;
  padding: 15px;
  border-radius: 12px;
  border: none;
  font-size: 18px;
  margin-top: 20px;
  background: #28a745;
  color: black;
}

.btn.off {
  background: #ccc;
  color: black;
}
</style>
</head>

<body>

<div class="container">

  <!-- BRAND BANNER -->
  <a class="banner" href="https://github.com/ritcommit/miku" target="_blank">
    <img src="https://raw.githubusercontent.com/ritcommit/miku/refs/heads/main/resource/miku_banner.png"
         alt="Miku">
  </a>

  <!-- MAIN CARD -->
  <div class="card">
    <div class="title">Miku Air Purifier</div>
    <div class="subtitle">Air must always be free..</div>

    <div class="gauge" id="gauge">
      <span id="pm">--</span>
    </div>

    <button id="power" class="btn off" onclick="toggle()">Power OFF</button>
  </div>

</div>

<script>
function update() {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('pm').innerHTML = Math.round(d.pm);
      let deg = Math.min(360, (d.pm / 833) * 360);
      document.getElementById('gauge').style.setProperty('--deg', deg + 'deg');

      let btn = document.getElementById('power');
      if(d.on) {
        btn.innerHTML = "ON";
        btn.className = "btn";
      } else {
        btn.innerHTML = "OFF";
        btn.className = "btn off";
      }
    });
}

function toggle() {
  fetch('/toggle');
  setTimeout(update, 300);
}

setInterval(update, 2000);
update();
</script>

</body>
</html>
)rawliteral";
}

/* ===== API ===== */
void handleStatus() {
  float pm = readPM25();
  String json = "{";
  json += "\"pm\":" + String(pm, 1) + ",";
  json += "\"on\":" + String(purifierOn ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggle() {
  purifierOn = !purifierOn;
  digitalWrite(RELAY_PIN, purifierOn ? HIGH : LOW);
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(9600);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server started");

  server.on("/", [](){ server.send(200, "text/html", htmlPage()); });
  server.on("/status", handleStatus);
  server.on("/toggle", handleToggle);

  server.begin();
}

void loop() {
  server.handleClient();
}
