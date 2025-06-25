# 🗑️ ESP32-CAM Trash Classifier

An embedded project using the **ESP32-CAM** to capture images of waste items and send them to a server for classification via HTTPS.

## 📸 Features

* Captures images using the ESP32-CAM module.
* Sends images over HTTPS (`multipart/form-data`) to a classification server.
* Parses JSON responses for classification results.
* Built-in LED flash control.
* Serial commands for control (`snap`, `status`, `reconnect`).
* Automatically reconnects to WiFi and re-syncs time using NTP.
* Certificate validation disabled for development tunnels (can be secured for production).

---

## 🛠️ Hardware Requirements

* ESP32-CAM (AI Thinker variant)
* FTDI USB-to-Serial adapter
* Optional: Power supply (5V/2A recommended)
* Optional: Button or switch for EN/GPIO0 flashing

---

## 🌐 Server Endpoint

Ensure your server has an endpoint at:

```
POST https://<your-server>/classify
```

* Accepts `multipart/form-data` with a file field named `file`.
* Returns a JSON response with classification results.

---

## 📦 Dependencies

Include these libraries in your Arduino project:

```cpp
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
```

---

## ⚙️ Configuration

Update the following in the `.ino` sketch:

```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

const char *serverURL = "https://your-server.com/classify";
```

Optionally adjust:

* Timezone (for HTTPS SSL validation)
* Flash LED behavior
* Camera quality settings

---

## 🧪 Serial Commands

Type the following in the Serial Monitor (115200 baud):

| Command       | Action                          |
| ------------- | ------------------------------- |
| `snap`      | Capture photo and classify      |
| `status`    | Show WiFi/IP/Time/Camera status |
| `reconnect` | Reconnect WiFi and re-sync time |

---

## 🚦 Status LEDs

* Flash LED (GPIO 4) turns on briefly during capture.
* Serial monitor logs show status and results.

---

## 🔐 Security Note

⚠️  **For development use only** :

```cpp
client.setInsecure();
```

This bypasses SSL certificate validation. For production:

* Use `client.setCACert()` with your server's root certificate.
* Sync time with NTP to validate SSL expiration.

---

## 🧠 Sample Response Format

Expected JSON format from server:

```json
{
  "class": "plastic",
  "confidence": 0.98
}
```

---

## 📂 File Structure

```
esp32-cam-trash-classifier/
├── src/
│   └── esp32_trash_classifier.ino
├── README.md
```

---

## 🚀 Flashing Instructions

1. Connect ESP32-CAM to your computer via FTDI adapter.
2. Hold **GPIO0** low and press **EN** to enter flashing mode.
3. Use Arduino IDE or PlatformIO to upload the code.
4. Open Serial Monitor at  **115200 baud** .

---

## 🧰 Troubleshooting

* **Camera init failed?**
  * Ensure `PWDN_GPIO_NUM` is set to `32` (AI Thinker)
  * Check power supply (5V/2A recommended)
* **HTTPS failing?**
  * Check DNS resolution
  * Re-sync time using `synchronizeTime()`

---

## 📜 License

MIT License – Use freely with attribution.
