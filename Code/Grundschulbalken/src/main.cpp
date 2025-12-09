#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <ESPmDNS.h>

//// ---------- Einstellungen ----------
const char* AP_SSID = "Grundschulbalken_V2.5";
const char* AP_PASS = "12345678";
const IPAddress AP_IP(192,168,4,1);
const IPAddress AP_GW(192,168,4,1);
const IPAddress AP_MASK(255,255,255,0);

// Firmware-Version in Footer
const char* FW_VERSION = "V2.5";

// I2C-Pins ESP32
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr uint8_t SHT_ADDR = 0x44;      // 0x44 Standard, 0x45 alternative
//// -----------------------------------

Adafruit_SHT31 sht31 = Adafruit_SHT31();
WebServer server(80);

float lastT = NAN, lastH = NAN;
unsigned long lastReadMs = 0;
const unsigned long readIntervalMs = 20000;   // alle 20s messen

// ---------- Datenspeicher für History ----------
struct Sample {
  unsigned long ms;   // Zeit seit Start in ms
  float t;
  float h;
};

const size_t MAX_SAMPLES = 720; // ~4h bei 20s Intervall
Sample samples[MAX_SAMPLES];
size_t sampleCount = 0; // aktuelle Anzahl gespeicherter Werte
size_t sampleStart = 0; // Index des ältesten Werts

void addSample(unsigned long ms, float t, float h) {
  if (isnan(t) || isnan(h)) return; // keine ungültigen Werte speichern

  size_t idx;
  if (sampleCount < MAX_SAMPLES) {
    idx = (sampleStart + sampleCount) % MAX_SAMPLES;
    sampleCount++;
  } else {
    // Ringpuffer: Ältesten überschreiben
    idx = sampleStart;
    sampleStart = (sampleStart + 1) % MAX_SAMPLES;
  }
  samples[idx].ms = ms;
  samples[idx].t  = t;
  samples[idx].h  = h;
}

// erzeugt JS-Array "data" aus den gespeicherten Werten
String buildDataJs() {
  String js = "const data=[";
  for (size_t i = 0; i < sampleCount; i++) {
    size_t idx = (sampleStart + i) % MAX_SAMPLES;
    js += "{ms:";
    js += String(samples[idx].ms);
    js += ",t:";
    js += String(samples[idx].t, 2);
    js += ",h:";
    js += String(samples[idx].h, 1);
    js += "}";
    if (i + 1 < sampleCount) js += ",";
  }
  js += "];";
  return js;
}

// ---------- HTML-Seite mit zwei Diagrammen ----------
String htmlPage(float t, float h, bool ok) {
  unsigned long nowMs = millis();
  unsigned long since = (lastReadMs == 0) ? 0 : (nowMs - lastReadMs);
  unsigned long remaining = (readIntervalMs > since) ? (readIntervalMs - since) : 0;
  unsigned long remainingSec = remaining / 1000;

  String s;
  s.reserve(9000);

  s += F(
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>FASW Grundschule – Balken Feuchtemessung</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;"
    "margin:2rem;background:#0b1220;color:#e7eaf3}"
    ".card{max-width:820px;margin:auto;padding:1.5rem;border-radius:16px;"
    "background:#121a2b;box-shadow:0 8px 30px rgba(0,0,0,.35)}"
    "h1{margin:.2rem 0 .8rem;font-size:1.4rem}"
    "h2{margin-top:1rem;font-size:1rem;opacity:.9}"
    ".val{display:flex;gap:1rem;align-items:baseline;margin-bottom:.5rem}"
    ".big{font-size:3rem;font-weight:700}"
    ".small{opacity:.8}"
    ".bad{color:#ff6b6b}"
    "footer{margin-top:2rem;opacity:.55;font-size:.85rem;text-align:center}"
    "button{margin-top:.5rem;padding:.6rem 1rem;border:0;border-radius:12px;"
    "cursor:pointer;background:#2c6df2;color:white}"
    "canvas{margin-top:.5rem;width:100%;max-width:780px;border-radius:8px;"
    "background:#0b1220;border:1px solid #1e293b}"
    ".legend{display:flex;gap:1rem;font-size:.8rem;opacity:.8;margin-top:.2rem}"
    ".dot{width:10px;height:10px;border-radius:999px;display:inline-block;"
    "margin-right:.25rem;vertical-align:middle}"
    ".temp{background:#f97316}"
    ".hum{background:#38bdf8}"
    ".btn-row{display:flex;flex-wrap:wrap;gap:.5rem;margin:.5rem 0 1rem;}"
    ".btn-small{padding:.3rem .7rem;border-radius:999px;border:1px solid #1f2937;"
    "background:#0f172a;color:#e5e7eb;font-size:.8rem;cursor:pointer;}"
    ".btn-small.active{background:#2c6df2;border-color:#2c6df2;color:white;}"
    ".tooltip{position:fixed;z-index:999;background:#020617cc;color:#e5e7eb;"
    "padding:.25rem .6rem;border-radius:.35rem;font-size:.75rem;"
    "pointer-events:none;opacity:0;transition:opacity .12s ease;}"
    "a{color:#3b82f6;text-decoration:none;}"
    "</style></head><body><div class=card><h1>FASW Grundschule – Balken Feuchtemessung</h1>"
  );

  if (ok) {
    s += "<div class=val><span class=big>";
    s += String(t, 2);
    s += "°C</span><span class=small>Temperatur</span></div>"
         "<div class=val><span class=big>";
    s += String(h, 1);
    s += "%</span><span class=small>rel. Feuchte</span></div>";
  } else {
    s += "<p class='bad'><b>Sensor nicht erreichbar</b><br>"
         "Pr&uuml;fe Verkabelung (SDA=GPIO21, SCL=GPIO22), Adresse 0x44/0x45.</p>";
  }

  s += "<p style='margin-top:.4rem;font-size:.9rem;opacity:.75'>N&auml;chste Messung in "
       "<span id='cd'>" + String(remainingSec) +
       "</span> Sekunden (Intervall: 20&nbsp;s)</p>";

  // Zeitfenster-Buttons
  s +=
    "<div class='btn-row'>"
      "<button class='btn-small active' id='btnAll' onclick='setViewWindow(null,\"btnAll\")'>Alles</button>"
      "<button class='btn-small' id='btn10' onclick='setViewWindow(10*60*1000,\"btn10\")'>Letzte 10 Min</button>"
      "<button class='btn-small' id='btn60' onclick='setViewWindow(60*60*1000,\"btn60\")'>Letzte 1&nbsp;h</button>"
      "<button class='btn-small' id='btn240' onclick='setViewWindow(4*60*60*1000,\"btn240\")'>Letzte 4&nbsp;h</button>"
    "</div>";

  // Temperatur-Diagramm
  s +=
    "<h2>Temperatur-Verlauf</h2>"
    "<canvas id='chartTemp' width='780' height='260'></canvas>"
    "<div class='legend'><span><span class='dot temp'></span>Temperatur (&deg;C)</span></div>";

  // Feuchtigkeits-Diagramm
  s +=
    "<h2>Feuchtigkeits-Verlauf</h2>"
    "<canvas id='chartHum' width='780' height='260'></canvas>"
    "<div class='legend'><span><span class='dot hum'></span>Relative Feuchte (%)</span></div>";

  // Buttons
  s +=
    "<div style='margin-top:1rem;'>"
    "<button onclick='location.reload()'>Seite aktualisieren</button>"
    "<button onclick='window.location=\"/export\"'>Daten als CSV exportieren</button>"
    "</div>";

  // Footer mit Link + Version + GitHub
  s += "<footer>developed by "
       "<a href='https://farin-langner.de' target='_blank'><b>Farin Langner</b></a><br>"
       "Version ";
  s += FW_VERSION;
  s += " · <a href='https://github.com/Farin25/FASW-Grundschulebalken-feuchtigkeitsmessung' target='_blank'>Source Code auf GitHub</a>"
       "</footer>"
       "<div id='tooltip' class='tooltip'></div>";

  // ----- JS: Daten + Diagramm + Tooltip + Zoom -----
  s += "<script>";
  s += buildDataJs();  // const data=[...];

  s += "const MEASURE_INTERVAL=";
  s += String(readIntervalMs / 1000);
  s += ";";
  s += "let remaining=";
  s += String(remainingSec);
  s += ";";

  s += R"rawliteral(
let viewWindowMs = null; // null = alle Daten
const chartMeta = {};    // canvasId -> {points:[{x,y,ms,value}], unit:string}
let tooltipEl = null;

function fmtRelTime(ms) {
  const s = Math.floor(ms / 1000);
  const hh = String(Math.floor(s / 3600)).padStart(2, "0");
  const mm = String(Math.floor((s % 3600) / 60)).padStart(2, "0");
  const ss = String(s % 60).padStart(2, "0");
  return hh + ":" + mm + ":" + ss;
}

// Countdown für nächste Messung
function startCountdown() {
  const el = document.getElementById('cd');
  if (!el) return;
  function tick() {
    el.textContent = remaining.toString();
    if (remaining > 0) {
      remaining--;
      setTimeout(tick, 1000);
    }
  }
  tick();
}

// Daten gemäß viewWindow filtern
function getFilteredData() {
  if (!data || !data.length) return [];
  if (viewWindowMs == null) return data;

  const tLast = data[data.length - 1].ms;
  const from = tLast - viewWindowMs;
  return data.filter(d => d.ms >= from);
}

function setViewWindow(ms, btnId) {
  viewWindowMs = ms;
  document.querySelectorAll('.btn-small').forEach(b => b.classList.remove('active'));
  if (btnId) {
    const b = document.getElementById(btnId);
    if (b) b.classList.add('active');
  }
  drawAll();
}

// Zeichnet ein Diagramm in ein Canvas
function drawChart(canvasId, getValue, color, unit) {
  const c = document.getElementById(canvasId);
  if (!c || !c.getContext) return;

  const ctx = c.getContext('2d');
  const w = c.width, h = c.height;
  ctx.clearRect(0, 0, w, h);

  const d = getFilteredData();
  if (!d.length) {
    ctx.fillStyle = '#e5e7eb';
    ctx.font = '12px system-ui';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillText('Noch keine Messdaten vorhanden oder Zeitfenster enthält keine Werte.', 20, 20);
    chartMeta[canvasId] = { points: [], unit };
    return;
  }

  const left = 55, right = 10, top = 10, bottom = 30;
  const innerW = w - left - right;
  const innerH = h - top - bottom;

  // Y-Min/Max bestimmen
  let minV = 1e9, maxV = -1e9;
  for (const item of d) {
    const v = getValue(item);
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
  }
  if (minV === maxV) {
    minV -= 1;
    maxV += 1;
  }

  // Zeitspanne (X-Achse)
  const t0 = d[0].ms;
  const tLast = d[d.length - 1].ms;
  const span = Math.max(1, tLast - t0);

  const xPos = (ms) => left + (ms - t0) / span * innerW;
  const yPos = (v) => top + innerH - (v - minV) / (maxV - minV) * innerH;

  // Achsen
  ctx.strokeStyle = '#4b5563';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(left, top);
  ctx.lineTo(left, top + innerH);
  ctx.lineTo(left + innerW, top + innerH);
  ctx.stroke();

  // Y-Beschriftung + horizontale Hilfslinien
  ctx.fillStyle = '#e5e7eb';
  ctx.font = '10px system-ui';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';

  for (let i = 0; i <= 4; i++) {
    const v = minV + (maxV - minV) * i / 4;
    const y = yPos(v);
    ctx.fillText(v.toFixed(1), left - 5, y);
    ctx.strokeStyle = 'rgba(148,163,184,0.25)';
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(left + innerW, y);
    ctx.stroke();
  }

  // X-Beschriftung (Zeit)
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  ctx.fillStyle = '#e5e7eb';
  const ticks = 5;
  for (let i = 0; i <= ticks; i++) {
    const tt = t0 + span * i / ticks;
    const x = left + innerW * i / ticks;
    ctx.fillText(fmtRelTime(tt), x, top + innerH + 4);
  }

  // Kurve zeichnen + Punkte für Tooltip sammeln
  const points = [];
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  d.forEach((item, idx) => {
    const v = getValue(item);
    const x = xPos(item.ms);
    const y = yPos(v);
    points.push({ x, y, ms: item.ms, value: v });
    if (idx === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();

  chartMeta[canvasId] = { points, unit };
}

// Tooltip-Handling
function attachTooltip(canvasId) {
  const c = document.getElementById(canvasId);
  if (!c || !tooltipEl) return;

  c.addEventListener('mousemove', (e) => {
    const meta = chartMeta[canvasId];
    if (!meta || !meta.points || !meta.points.length) {
      tooltipEl.style.opacity = 0;
      return;
    }
    const rect = c.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    let nearest = null;
    let bestDist = 1e9;
    for (const p of meta.points) {
      const dx = p.x - mouseX;
      const dy = p.y - mouseY;
      const dist = Math.sqrt(dx*dx + dy*dy);
      if (dist < bestDist) {
        bestDist = dist;
        nearest = p;
      }
    }

    if (!nearest || bestDist > 30) {
      tooltipEl.style.opacity = 0;
      return;
    }

    const pageX = rect.left + nearest.x + 10;
    const pageY = rect.top + nearest.y - 10;

    tooltipEl.style.left = pageX + 'px';
    tooltipEl.style.top = pageY + 'px';
    tooltipEl.textContent = fmtRelTime(nearest.ms) + ' · ' +
                            nearest.value.toFixed(1) + ' ' + (meta.unit || '');
    tooltipEl.style.opacity = 1;
  });

  c.addEventListener('mouseleave', () => {
    tooltipEl.style.opacity = 0;
  });
}

function drawAll() {
  drawChart('chartTemp', d => d.t, '#f97316', '°C');
  drawChart('chartHum',  d => d.h, '#38bdf8', '%');
}

// Init
function initUI() {
  tooltipEl = document.getElementById('tooltip');
  attachTooltip('chartTemp');
  attachTooltip('chartHum');
  startCountdown();
  drawAll();
  // Seite alle 30s neu laden, damit neue Werte kommen
  setTimeout(() => location.reload(), 30000);
}

initUI();
)rawliteral";

  s += "</script></div></body></html>";
  return s;
}

// ---------- HTTP-Handler ----------
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

// kleine Hilfsfunktionen für Export-Formatierung
String twoDigits(int v) {
  if (v < 10) return "0" + String(v);
  return String(v);
}

// CSV-Export der gespeicherten Daten:
// Tag;Zeit;Temperatur;Luftfeuchtigkeit
// z.B. 0;00:01:20;="23.45";="56.7"
void handleExport() {
  String csv = "Tag;Zeit;Temperatur;Luftfeuchtigkeit\n";
  for (size_t i = 0; i < sampleCount; i++) {
    size_t idx = (sampleStart + i) % MAX_SAMPLES;
    unsigned long ms = samples[idx].ms;

    unsigned long totalSeconds = ms / 1000;
    unsigned long day = totalSeconds / 86400UL;
    unsigned long secondsOfDay = totalSeconds % 86400UL;
    int hour = secondsOfDay / 3600;
    int minute = (secondsOfDay % 3600) / 60;
    int second = secondsOfDay % 60;

    String timeStr = twoDigits(hour) + ":" + twoDigits(minute) + ":" + twoDigits(second);

    csv += String(day);
    csv += ';';
    csv += timeStr;
    csv += ';';
    csv += "=\"";
    csv += String(samples[idx].t, 2);
    csv += "\";";
    csv += "=\"";
    csv += String(samples[idx].h, 1);
    csv += "\"\n";
  }
  server.send(200, "text/csv; charset=utf-8", csv);
}

// ---------- Sensor lesen ----------
void readSensorIfDue() {
  unsigned long now = millis();
  if (now - lastReadMs < readIntervalMs) return;
  lastReadMs = now;

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    lastT = t;
    lastH = h;
    addSample(now, t, h);
  } else {
    // einmal Adresse umschalten und erneut versuchen
    static bool toggled = false;
    if (!toggled) {
      toggled = true;
      uint8_t alt = (SHT_ADDR == 0x44) ? 0x45 : 0x44;
      sht31 = Adafruit_SHT31();
      if (sht31.begin(alt)) {
        lastT = sht31.readTemperature();
        lastH = sht31.readHumidity();
        if (!isnan(lastT) && !isnan(lastH)) {
          addSample(now, lastT, lastH);
        }
      }
    }
  }
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!sht31.begin(SHT_ADDR)) {
    Serial.println("[SHT31] nicht gefunden (0x44). Versuche 0x45 …");
    if (!sht31.begin(0x45)) {
      Serial.println("[SHT31] Sensor nicht erreichbar.");
    }
  }
  sht31.heater(false); // Heizer aus

  // SoftAP konfigurieren
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  if (WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.print("AP bereit: "); Serial.println(AP_SSID);
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("SoftAP Start FEHLGESCHLAGEN!");
  }

  // mDNS / Bonjour: SHT31.local
  if (MDNS.begin("SHT31")) {   // Hostname -> http://SHT31.local
    Serial.println("mDNS gestartet: http://SHT31.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Fehler beim Start von mDNS.");
  }

  // HTTP-Server
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.on("/export", handleExport);
  server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("HTTP-Server gestartet (Port 80).");
}

void loop() {
  server.handleClient();
  readSensorIfDue();
}