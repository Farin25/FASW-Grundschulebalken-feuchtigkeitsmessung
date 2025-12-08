#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>


const char* AP_SSID = "Grundschulbalken";
const char* AP_PASS = "12345678";     
const IPAddress AP_IP(192,168,4,1);
const IPAddress AP_GW(192,168,4,1);
const IPAddress AP_MASK(255,255,255,0);

// I2C-Pins ESP32
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr uint8_t SHT_ADDR = 0x44;    


Adafruit_SHT31 sht31 = Adafruit_SHT31();
WebServer server(80);

float lastT = NAN, lastH = NAN;
unsigned long lastReadMs = 0;
const unsigned long readIntervalMs = 2000;

String htmlPage(float t, float h, bool ok) {
  String s = F(
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>ESP32 SHT31</title>"
    "<style>body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;"
    "margin:2rem;background:#0b1220;color:#e7eaf3}"
    ".card{max-width:520px;margin:auto;padding:1.5rem;border-radius:16px;"
    "background:#121a2b;box-shadow:0 8px 30px rgba(0,0,0,.35)}"
    "h1{margin:.2rem 0 .8rem;font-size:1.4rem}"
    ".val{display:flex;gap:1rem;align-items:baseline}"
    ".big{font-size:3rem;font-weight:700}"
    ".small{opacity:.8}"
    ".bad{color:#ff6b6b}"
    "footer{margin-top:1rem;opacity:.6;font-size:.9rem;text-align:center}"
    "button{margin-top:1rem;padding:.6rem 1rem;border:0;border-radius:12px;"
    "cursor:pointer;background:#2c6df2;color:white}"
    "</style></head><body><div class=card><h1>DFRobot SHT31 (ESP32)</h1>");
  if (ok) {
    s += "<div class=val><span class=big>";
    s += String(t, 2);
    s += "°C</span><span class=small>Temperatur</span></div>"
         "<div class=val><span class=big>";
    s += String(h, 1);
    s += "%</span><span class=small>rel. Feuchte</span></div>";
  } else {
    s += "<p class='bad'><b>Sensor nicht erreichbar</b><br>"
         "Prüfe Verkabelung (SDA=GPIO21, SCL=GPIO22), Adresse 0x44/0x45.</p>";
  }
  s += F(
    "<button onclick='location.reload()'>Aktualisieren</button>"
    "<footer>SoftAP: 192.168.4.1 · API: <code>/api</code></footer>"
    "<script>setTimeout(()=>location.reload(),5000);</script>"
    "</div></body></html>");
  return s;
}

void handleRoot(){
  bool ok = !isnan(lastT) && !isnan(lastH);
  server.send(200, "text/html; charset=utf-8", htmlPage(lastT, lastH, ok));
}

void handleApi(){
  if (isnan(lastT) || isnan(lastH)) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"sensor_unavailable\"}");
  } else {
    String j = "{\"ok\":true,\"t\":" + String(lastT,2) + ",\"h\":" + String(lastH,1) + "}";
    server.send(200, "application/json", j);
  }
}

void readSensorIfDue() {
  unsigned long now = millis();
  if (now - lastReadMs < readIntervalMs) return;
  lastReadMs = now;

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    lastT = t;
    lastH = h;
  } else {
    // fals es nicht funktioniert einmal Adresse umschalten und erneut versuchen
    static bool toggled = false;
    if (!toggled) {
      toggled = true;
      // versuch der zweite Adresse
      uint8_t alt = (SHT_ADDR == 0x44) ? 0x45 : 0x44;
      sht31 = Adafruit_SHT31();
      if (sht31.begin(alt)) {
        lastT = sht31.readTemperature();
        lastH = sht31.readHumidity();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);


  Wire.begin(I2C_SDA, I2C_SCL);

  if (!sht31.begin(SHT_ADDR)) {
    Serial.println("[SHT31] nicht gefunden (0x44). Versuche 0x45 …");
    if (!sht31.begin(0x45)) {
      Serial.println("[SHT31] Sensor nicht erreichbar.");
    }
  }
  sht31.heater(false);


  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  if (WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.print("AP bereit: "); Serial.println(AP_SSID);
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("SoftAP Start FEHLGESCHLAGEN!");
  }


  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("HTTP-Server gestartet (Port 80).");
}

void loop() {
  server.handleClient();
  readSensorIfDue();
}
