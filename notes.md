# 📘 ESPHome – DFRobot SHT31 Sensor

## 🔧 Projektüberblick
ESP32 liest Temperatur & Luftfeuchtigkeit vom DFRobot SHT31 über I²C aus  
und stellt sie über WLAN + Weboberfläche bereit.

---

## ⚙️ Hardware

### 🧩 Komponenten
- ESP32 DevKit V1  
- DFRobot SHT31-Sensor (Gravity-Version oder Standard-Breakout)

### 🔌 Verkabelung (ESP32 DevKit V1)
| Sensor-Pin | Farbe (DFRobot-Kabel) | ESP32-Pin | Hinweis |
|-------------|----------------------|------------|----------|
| **VCC**     | Rot   | **3V3** (bzw. 5 V *falls Modul 3.3–5 V tolerant ist*) | Versorgung |
| **GND**     | Schwarz | **GND** | Masse |
| **SDA**     | Grün | **GPIO 21** | Datenleitung |
| **SCL**     | Blau | **GPIO 22** | Taktleitung |

> 💡 Wenn keine I²C-Geräte gefunden werden → **Grün ↔ Blau tauschen**  
> oder Adresse auf **0x45** ändern.

---

## 🧠 ESPHome-Konfiguration (`V1.yml`)

```yaml
esphome:
  name: dfrobot-sht31
  friendly_name: DFRobot SHT31 Sensor

esp32:
  board: esp32dev
  framework:
    type: arduino

wifi:
  ssid: "MakerSpace"
  password: "DEIN_PASSWORT"

logger:
api:
ota:
  platform: esphome

web_server:
  port: 80
  auth:
    username: admin
    password: meinpasswort

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true
  frequency: 100kHz

sensor:
  - platform: sht3xd
    address: 0x44        # ggf. 0x45 ausprobieren
    temperature:
      name: "SHT31 Temperatur"
      accuracy_decimals: 1
    humidity:
      name: "SHT31 Luftfeuchtigkeit"
      accuracy_decimals: 1
    update_interval: 15s
