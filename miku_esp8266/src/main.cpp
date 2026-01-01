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
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

#define LED_PIN D2
#define DUST_PIN A0
#define RELAY_PIN D1
#define DHT_PIN D5
#define DHT_TYPE DHT11


ESP8266WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);

// last fetched external values
float last_ext_pm25 = -1.0;
int last_ext_aqi = -1;

bool purifierOn = false;

/* ===== WiFi Settings ===== */
const char* ssid = "YOUR_WIFI_SSID";  // Enter SSID here
const char* password = "YOUR_WIFI_PASSWORD";  //Enter Password here
const char* AQICN_TOKEN = "YOUR_AQICN_TOKEN"; // Set to your token

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
  background: conic-gradient(#eee var(--deg), #eee 0deg);
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background 0.6s ease, color 0.4s ease;
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

select {
  width: 100%;
  padding: 10px;
  border-radius: 8px;
  border: 1px solid #ddd;
  margin-top: 10px;
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

    <div style="display:flex;justify-content:space-between;align-items:center;">
      <div style="font-weight:600;margin-right:8px;">City</div>
      <select id="city" onchange="fetchAQI()">
        <option>Delhi</option>
        <option>Mumbai</option>
        <option>Kolkata</option>
        <option>Chennai</option>
        <option>Bengaluru</option>
        <option>Hyderabad</option>
        <option>Pune</option>
        <option>Ahmedabad</option>
        <option>Lucknow</option>
        <option>Jaipur</option>
      </select>
    </div>

    <div style="margin-top:10px; text-align:center;">
      <div class="subtitle">Outdoor AQI (<span id="cityName">--</span>)</div>
      <div class="gauge" id="gauge" style="--deg:0deg;">
        <span id="aqi">--</span>
      </div>
      <div class="subtitle" id="aqiCategory"></div>
    </div>

    <div style="margin-top:15px;">
      <div class="subtitle">Indoor</div>
      <div style="padding-top:8px;">
        <div style="display:flex;justify-content:space-between;padding:6px 0;">
          <div>PM2.5 (sensor)</div>
          <div><span id="pm">--</span> &micro;g/m&sup3;</div>
        </div>
        <div style="display:flex;justify-content:space-between;padding:6px 0;">
          <div>Humidity</div>
          <div><span id="hum">--</span> %</div>
        </div>
        <div style="display:flex;justify-content:space-between;padding:6px 0;">
          <div>Temperature</div>
          <div><span id="temp">--</span> &deg;C</div>
        </div>
      </div>
    </div>

    <button id="power" class="btn off" onclick="toggle()">Power OFF</button>
  </div>

</div>

<script>
function update() {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      // show sensor (indoor) values as text (labels left, values right handled in HTML)
      document.getElementById('pm').innerHTML = (d.pm !== null && d.pm !== undefined) ? Math.round(d.pm) : "--";
      document.getElementById('hum').innerHTML = (d.humidity !== null && d.humidity !== undefined) ? Math.round(d.humidity) : "--";
      document.getElementById('temp').innerHTML = (d.temperature !== null && d.temperature !== undefined) ? Math.round(d.temperature) : "--";

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

function fetchAQI() {
  let city = document.getElementById('city').value;
  fetch('/aqi?city=' + encodeURIComponent(city))
    .then(r => r.json())
    .then(a => {
      document.getElementById('cityName').innerHTML = a.city;
          const gauge = document.getElementById('gauge');
      const catEl = document.getElementById('aqiCategory');
      function setGaugeColorAndDeg(color, deg) {
        gauge.style.setProperty('--deg', deg + 'deg');
        // set background using conic-gradient so color fills up to --deg
        gauge.style.background = 'conic-gradient(' + color + ' var(--deg), #eee 0deg)';
      }

      if(a.aqi !== null && a.aqi !== undefined) {
        const val = Math.round(a.aqi);
        document.getElementById('aqi').innerHTML = val;
        let deg2 = Math.min(360, (val / 500) * 360);
        // color by AQI ranges
        let color = '#4caf50';
        if (val <= 50) color = '#4caf50';
        else if (val <= 100) color = '#ffc107';
        else if (val <= 150) color = '#ff9800';
        else if (val <= 200) color = '#ff5722';
        else if (val <= 300) color = '#d32f2f';
        else color = '#7e0000';
        setGaugeColorAndDeg(color, deg2);
        catEl.innerHTML = a.category;
      } else if (a.pm25 !== null && a.pm25 !== undefined) {
        const val = Math.round(a.pm25);
        document.getElementById('aqi').innerHTML = val;
        let deg2 = Math.min(360, (val / 500) * 360);
        // map category to color if available
        let color = '#4caf50';
        const cat = (a.category || '').toLowerCase();
        if (cat.indexOf('good') >= 0) color = '#4caf50';
        else if (cat.indexOf('moderate') >= 0) color = '#ffc107';
        else if (cat.indexOf('unhealthy for sensitive') >= 0) color = '#ff9800';
        else if (cat.indexOf('unhealthy') >= 0) color = '#ff5722';
        else if (cat.indexOf('very unhealthy') >= 0) color = '#d32f2f';
        else if (cat.indexOf('hazard') >= 0) color = '#7e0000';
        setGaugeColorAndDeg(color, deg2);
        catEl.innerHTML = a.category;
      } else {
        document.getElementById('aqi').innerHTML = "--";
        catEl.innerHTML = "Unknown";
        // reset gauge visual
        setGaugeColorAndDeg('#eee', 0);
      }
    }).catch(e => {
      document.getElementById('aqi').innerHTML = "--";
      document.getElementById('aqiCategory').innerHTML = "Unknown";
    });
}

function toggle() {
  fetch('/toggle');
  setTimeout(update, 300);
}

setInterval(update, 2000);
update();
// Fetch AQI once on load for default selection; subsequent calls only when user changes city
fetchAQI();
</script>

</body>
</html>
)rawliteral";
}

/* ===== API ===== */
void handleStatus() {
  float pm = readPM25();
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();

  String json = "{";
  json += "\"pm\":" + String(pm, 1) + ",";
  if (!isnan(hum)) json += "\"humidity\":" + String(hum, 1) + ","; else json += "\"humidity\":null,";
  if (!isnan(temp)) json += "\"temperature\":" + String(temp, 1) + ","; else json += "\"temperature\":null,";
  json += "\"on\":" + String(purifierOn ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggle() {
  purifierOn = !purifierOn;
  digitalWrite(RELAY_PIN, purifierOn ? HIGH : LOW);
  server.send(200, "text/plain", "OK");
}

// ===== AQI Fetch =====
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

float extractNumber(const String &s, int start) {
  int i = start;
  // find first digit or '-' or '.'
  while (i < (int)s.length() && !(isDigit(s[i]) || s[i] == '-' || s[i] == '.')) i++;
  if (i >= (int)s.length()) return NAN;
  int j = i;
  while (j < (int)s.length() && (isDigit(s[j]) || s[j] == '.' || s[j] == '-')) j++;
  String num = s.substring(i, j);
  return num.toFloat();
}

// Simple string search helper: find a numeric value near a key (e.g., "aqi" or "pm25") without full JSON parsing.
bool findNumberNearKey(const String &s, const String &key, float &out, int window=200) {
  int idx = s.indexOf(key);
  if (idx < 0) return false;
  int start = idx;
  int end = min((int)s.length(), idx + window);
  // find ':' after key within window
  int colon = -1;
  for (int i = idx; i < end; i++) {
    if (s[i] == ':') { colon = i; break; }
  }
  if (colon >= 0) start = colon + 1;
  else start = idx;

  // move to first digit / sign / dot
  int i = start;
  while (i < end && !(isDigit(s[i]) || s[i] == '-' || s[i] == '.')) i++;
  if (i >= end) return false;
  int j = i;
  while (j < end && (isDigit(s[j]) || s[j] == '.' || s[j] == '-')) j++;
  if (j == i) return false;
  String num = s.substring(i, j);
  out = num.toFloat();
  return true;
}
float fetchCityPM25(const String &city) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

    // Use AQICN API (https://aqicn.org)
  // prefer IAQI pm25 if present, also capture AQI index

  String url = "https://api.waqi.info/feed/" + urlEncode(city) + "/?token=" + String(AQICN_TOKEN);
  Serial.println("Fetching AQICN: " + url);
  if (!http.begin(client, url.c_str())) {
    Serial.println("HTTP begin failed for AQICN");
    return -1.0;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "miku-esp8266/1.0");
  int code = http.GET();
  Serial.printf("HTTP code: %d\n", code);
  if (code != HTTP_CODE_OK) {
    Serial.println("AQICN request failed");
    http.end();
    return -1.0;
  }

  String body = http.getString();
  http.end();
  Serial.printf("Payload len: %d\n", (int)body.length());
  //Serial.println(body.substring(0, min(50, (int)body.length())));
  last_ext_pm25 = -1.0;
  last_ext_aqi = -1;

  // Try simple string search first (faster, avoids full JSON parsing when not needed)
  float tmpf;
  if (findNumberNearKey(body, "\"aqi\"", tmpf, 200)) {
    last_ext_aqi = (int)tmpf;
    Serial.printf("Found AQI via string search: %d\n", last_ext_aqi);
  }
  if (findNumberNearKey(body, "\"pm25\"", tmpf, 200) || findNumberNearKey(body, "\"pm2_5\"", tmpf, 200)) {
    last_ext_pm25 = tmpf;
    Serial.printf("Found PM2.5 via string search: %.1f\n", last_ext_pm25);
  }

  if (last_ext_pm25 >= 0) {
    return last_ext_pm25;
  }
  if (last_ext_aqi >= 0) {
    // return -1 for pm25 and let handler include aqi
    return -1.0;
  }

  Serial.println("No pm25 or aqi found in AQICN response");
  return -1.0;
}

String aqiCategory(float pm25) {
  if (pm25 < 0) return "Unknown";
  if (pm25 <= 12.0) return "Good";
  if (pm25 <= 35.4) return "Moderate";
  if (pm25 <= 55.4) return "Unhealthy for Sensitive";
  if (pm25 <= 150.4) return "Unhealthy";
  if (pm25 <= 250.4) return "Very Unhealthy";
  return "Hazardous";
}

String aqiIndexCategory(int aqi) {
  if (aqi < 0) return "Unknown";
  if (aqi <= 50) return "Good";
  if (aqi <= 100) return "Moderate";
  if (aqi <= 150) return "Unhealthy for Sensitive";
  if (aqi <= 200) return "Unhealthy";
  if (aqi <= 300) return "Very Unhealthy";
  return "Hazardous";
}

void handleAQI() {
  String city = server.arg("city");
  if (city.length() == 0) city = "Delhi";
  float pm25 = fetchCityPM25(city);
  String json = "{";
  json += "\"city\":\"" + city + "\",";

  // include both AQI index (if available) and pm25 (if available)
  if (last_ext_aqi >= 0) json += "\"aqi\":" + String(last_ext_aqi) + ","; else json += "\"aqi\":null,";
  if (pm25 >= 0) json += "\"pm25\":" + String(pm25, 1) + ","; else json += "\"pm25\":null,";

  String category = "Unknown";
  if (last_ext_aqi >= 0) category = aqiIndexCategory(last_ext_aqi);
  else if (pm25 >= 0) category = aqiCategory(pm25);
  json += "\"category\":\"" + category + "\"";

  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(9600);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);

  // initialize DHT sensor
  dht.begin();
  Serial.println("DHT sensor initialized");

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
  server.on("/aqi", handleAQI);

  server.begin();
}

void loop() {
  server.handleClient();
}
