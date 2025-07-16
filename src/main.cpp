#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h> // Required for HTTPS connections
#include <time.h>             // Required for NTP time synchronization

// WiFi credentials, Mkae Sure To Use Correct Detaile, For Example:
const char *ssid = "Smith";
const char *password = "2!$%888thyik@gur@5s";

// Server configuration
// IMPORTANT: If using HTTPS, ensure your server URL starts with "https://"
const char *serverURL = "https://7jbs67jq-8000.inc1.devtunnels.ms/classify"; // Using '/classify' as per your server output // Replace with your URL

// String host = "http://192.168.172.130:8000";

// NTP Server for time synchronization (crucial for HTTPS certificate validation)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;     // Adjust for your local GMT offset in seconds (e.g., +3600 for CAT)
const int daylightOffset_sec = 0; // Adjust for daylight saving in seconds

// Camera pin definitions for ESP32-CAM AI Thinker
// IMPORTANT: PWDN_GPIO_NUM must be 32 for AI-Thinker boards to power on the camera.
#define PWDN_GPIO_NUM 32  // <--- CORRECTED THIS PIN!
#define RESET_GPIO_NUM -1 // -1 is common for AI-Thinker
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// LED pin for the flash (GPIO 4 on AI-Thinker)
#define FLASH_LED_PIN 4

// Function Prototypes (Declarations)
// These tell the compiler about the existence and signature of the functions
// before they are actually defined later in the code.
bool initCamera();
void connectToWiFi();
void synchronizeTime();
void takePhotoAndClassify();
String sendImageToServer(uint8_t *imageData, size_t imageSize);
void parseClassificationResult(String jsonResponse);
void showStatus();
void TestGet();
String sendToServer2(uint8_t *imageData, size_t imagesize);

void setup()
{
  Serial.begin(115200);
  Serial.println("\nESP32-CAM Trash Classifier Starting...");

  // Initialize LED pin as output and ensure it's off initially.
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Connect to WiFi network.
  connectToWiFi();

  // Synchronize time via NTP, essential for HTTPS (SSL/TLS certificate validation)
  synchronizeTime();

  // Initialize camera and check for success.
  if (initCamera())
  {
    Serial.println("✅ Camera initialized successfully");
  }
  else
  {
    Serial.println("❌ Camera initialization failed! Please check wiring, power, and PWDN_GPIO_NUM (should be 32 for AI-Thinker).");
    // If camera fails to initialize, halt execution as it's a critical component.
    while (true)
    {
      delay(1000); // Loop indefinitely to indicate a fatal error
    }
  }

  Serial.println("Ready! Type 'snap' to take a photo and classify trash.");
  Serial.println("Commands:");
  Serial.println("  snap      - Take photo and classify");
  Serial.println("  status    - Show connection status");
  Serial.println("  get    - Test Whether You Can Make Network Requests");
  Serial.println("  reconnect - Attempt to reconnect to WiFi");
}

void loop()
{
  // Check for serial commands from the user.
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();        // Remove leading/trailing whitespace
    command.toLowerCase(); // Convert command to lowercase for case-insensitive comparison

    if (command == "snap")
    {
      Serial.println("📸 Taking photo...");
      takePhotoAndClassify();
    }
    else if (command == "status")
    {
      showStatus();
    }
    else if (command == "reconnect")
    {
      Serial.println("Attempting to reconnect WiFi...");
      connectToWiFi(); // Attempt to reconnect
      if (WiFi.status() == WL_CONNECTED)
      {
        synchronizeTime(); // Re-sync time if WiFi reconnects
      }
    }
    else if (command == "get")
    {

      TestGet();
    }
    else
    {
      Serial.println("❓ Unknown command. Available: snap, status, reconnect");
    }
  }

  // Periodically check WiFi status and attempt to reconnect if disconnected.
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("⚠️ WiFi disconnected. Attempting to reconnect...");
    connectToWiFi();
    if (WiFi.status() == WL_CONNECTED)
    {
      synchronizeTime(); // Re-sync time if WiFi reconnects
    }
    delay(5000); // Wait a bit before checking again to avoid rapid reconnection attempts
  }

  delay(100); // Small delay to prevent blocking the CPU
}

// Initializes the ESP32-CAM module with specified configurations.
bool initCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  // Use non-deprecated pin names for SCCB SDA/SCL
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; // This is now 32
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;       // XCLK frequency
  config.pixel_format = PIXFORMAT_JPEG; // Output format is JPEG

  // Configure frame size and JPEG quality based on PSRAM availability.
  // PSRAM allows for larger frames and higher quality images.
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_VGA; // 640x480 is plenty for classification
    config.jpeg_quality = 12;          // Slightly reduced quality = less memory
    config.fb_count = 1;               // Lower buffer count to save RAM
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA; // 800x600 resolution
    config.jpeg_quality = 12;           // Default quality for no PSRAM
    config.fb_count = 1;                // Only one frame buffer if no PSRAM
  }

  // Initialize the camera driver.
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("❌ Camera init failed with error 0x%x", err);
    return false;
  }

  // Get sensor instance to adjust camera settings for optimal classification.
  sensor_t *s = esp_camera_sensor_get();
  // Adjust common camera settings for potentially better image quality for classification.
  // These values can be fine-tuned based on your environment.
  s->set_brightness(s, 0);                 // -2 to 2 (0 is default)
  s->set_contrast(s, 0);                   // -2 to 2 (0 is default)
  s->set_saturation(s, 0);                 // -2 to 2 (0 is default)
  s->set_special_effect(s, 0);             // 0-No Effect, 1-Negative, 2-Grayscale, etc.
  s->set_whitebal(s, 1);                   // 0 = disable , 1 = enable Automatic White Balance (AWB)
  s->set_awb_gain(s, 1);                   // 0 = disable , 1 = enable AWB Gain
  s->set_wb_mode(s, 0);                    // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);              // 0 = disable , 1 = enable Automatic Exposure Control (AEC)
  s->set_ae_level(s, 0);                   // -2 to 2 (0 is default, higher value means brighter image)
  s->set_aec2(s, 0);                       // 0 = disable, 1 = enable AEC-2 (Exposure algorithm)
  s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6 (0-8x, 1-16x, ..., 6-128x), sets upper limit for gain
  s->set_aec_value(s, 300);                // 0 to 1200 (default 300) exposure value
  s->set_dcw(s, 1);                        // 0 = disable, 1 = enable downsize enables windowing (DCW)
  s->set_bpc(s, 0);                        // 0 = disable, 1 = enable Bad Pixel Correction
  s->set_wpc(s, 1);                        // 0 = disable, 1 = enable White Pixel Correction
  s->set_raw_gma(s, 1);                    // 0 = disable, 1 = enable Gamma correction
  s->set_lenc(s, 1);                       // 0 = disable, 1 = enable Lens Correction
  s->set_hmirror(s, 0);                    // 0 = disable, 1 = enable horizontal mirror
  s->set_vflip(s, 0);                      // 0 = disable, 1 = enable vertical flip
  s->set_colorbar(s, 0);                   // 0 = disable, 1 = enable test pattern

  return true;
}

// Connects the ESP32 to the specified WiFi network.
void connectToWiFi()
{
  // Check if already connected to avoid unnecessary attempts.
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("✅ Already connected to WiFi.");
    return;
  }

  // Explicitly configure DNS servers to ensure reliable hostname resolution
  // Google Public DNS servers are used as a reliable fallback.
  WiFi.disconnect(); // Disconnect before setting config to apply new settings
  WiFi.config(INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));

  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");

  int attempts = 0;
  // Wait for connection with a timeout.
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  { // Wait up to 20 seconds (40 * 500ms)
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi connected");
    Serial.print("📶 IP address: ");
    Serial.println(WiFi.localIP());

    // Optional: Test DNS resolution here to confirm it works
    IPAddress resolvedIP;
    Serial.print("🔍 Resolving hostname for server: ");
    // Extract just the hostname part from the serverURL for DNS lookup
    String host = serverURL;
    int schemeEnd = host.indexOf("://");
    if (schemeEnd != -1)
    {
      host = host.substring(schemeEnd + 3);
    }
    int pathStart = host.indexOf("/");
    if (pathStart != -1)
    {
      host = host.substring(0, pathStart);
    }
    Serial.print(host);
    if (WiFi.hostByName(host.c_str(), resolvedIP))
    {
      Serial.print(" -> Resolved to IP: ");
      Serial.println(resolvedIP);
    }
    else
    {
      Serial.println(" -> ❌ DNS Resolution FAILED! This might prevent HTTPS connection.");
    }
  }
  else
  {
    Serial.println("\n❌ WiFi connection failed! Status: " + String(WiFi.status()));
  }
}

// Synchronizes ESP32's time using Network Time Protocol (NTP).
// This is essential for validating SSL/TLS certificates when using HTTPS.
void synchronizeTime()
{
  Serial.println("⏰ Initializing NTP time synchronization...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  time_t now = time(nullptr);
  // Wait for time to be set. The loop continues until the time is greater than an arbitrary
  // value (e.g., a few days after epoch) to ensure it's properly synced.
  int attempts = 0;
  while (now < 24 * 3600 * 2 && attempts < 20)
  { // Wait up to 10 seconds (20 * 500ms)
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }
  Serial.println(); // Newline after dots

  if (now >= 24 * 3600 * 2)
  {
    Serial.println("✅ Time synchronized successfully.");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      Serial.print("Current UTC Time: ");
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
  }
  else
  {
    Serial.println("⚠️ WARNING: Failed to synchronize time. HTTPS connections may fail.");
  }
}

// Takes a photo, sends it to the server for classification, and parses the response.
void takePhotoAndClassify()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("❌ Error: WiFi not connected. Cannot take photo and classify.");
    return;
  }

  // Turn on flash LED for better lighting conditions.
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100); // Brief delay for LED to stabilize

  // Capture photo from the camera.
  camera_fb_t *fb = esp_camera_fb_get();

  // Turn off flash LED immediately after capturing.
  digitalWrite(FLASH_LED_PIN, LOW);

  if (!fb)
  {
    Serial.println("❌ Error: Failed to capture photo from camera buffer.");
    return;
  }

  Serial.printf("📸 Photo captured: %d bytes (format: %s)\n", fb->len,
                (fb->format == PIXFORMAT_JPEG) ? "JPEG" : "Other");

  // Send the captured image data to the server.
  // String result = sendImageToServer(fb->buf, fb->len);
  String result = sendToServer2(fb->buf, fb->len);

  // Release the camera frame buffer after use to free up memory.
  esp_camera_fb_return(fb);

  // Parse and display the classification result from the server.
  parseClassificationResult(result);
}

// Parses the JSON response from the classification server and prints the result.
void parseClassificationResult(String jsonResponse)
{
  if (jsonResponse.length() == 0)
  {
    Serial.println("❌ Classification: ERROR - No response received from server.");
    return;
  }

  Serial.println("📨 Raw Server Response: " + jsonResponse); // Print raw response for debugging

  // Allocate a DynamicJsonDocument. Adjust size if your JSON responses are larger.
  // 1024 bytes should be sufficient for a simple {"category": "..."} response.
  DynamicJsonDocument doc(1024);

  // Deserialize the JSON string.
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (error)
  {
    Serial.printf("❌ Classification: ERROR - Failed to parse JSON response: %s\n", error.c_str());
    return;
  }

  // Check if the "category" key exists in the JSON response.
  if (doc.containsKey("category"))
  {
    String category = doc["category"];
    // Convert to uppercase first, then print.
    category.toUpperCase();
    Serial.println("=== CLASSIFICATION RESULT ===");
    Serial.print("Category: ");
    Serial.println(category); // Print category in uppercase for emphasis

    // Provide user-friendly messages based on the classification category.
    if (category == "PAPER")
    {
      Serial.println("♻️  PAPER WASTE - Recyclable");
    }
    else if (category == "METAL")
    {
      Serial.println("🥫 METAL WASTE - Recyclable");
    }
    else if (category == "GLASS")
    {
      Serial.println("🍶 GLASS WASTE - Recyclable");
    }
    else if (category == "ORGANIC")
    {
      Serial.println("🍃 ORGANIC WASTE - Compostable");
    }
    else if (category == "NOT_TRASH")
    {
      Serial.println("🚫 NOT TRASH - Item not recognized as waste");
    }
    else
    {
      Serial.println("❓ UNKNOWN CATEGORY - Please verify server response logic.");
    }
    Serial.println("=============================");
  }
  else
  {
    Serial.println("❌ Classification: ERROR - 'category' key not found in server response JSON.");
  }
}

// Displays the current status of the ESP32-CAM, including WiFi and memory information.
void showStatus()
{
  Serial.println("=== ESP32-CAM STATUS ===");
  Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
  }
  Serial.printf("Server URL: %s\n", serverURL);
  Serial.printf("Free Heap Memory: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Minimum Free Heap: %d bytes (since last reset)\n", esp_get_minimum_free_heap_size());
  Serial.println("========================");
}

// Test Http client library
void TestGet()
{
  HTTPClient http;

  String url = "http://192.168.254.15:8000/test";

  Serial.println("================================JSON RESPONSE Test ============================");

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0)
  {
    Serial.printf("HTTP GET Response Code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      Serial.println("Raw JSON: ");
      Serial.println(payload);

      const size_t capacity = 256; // Adjust as needed
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error)
      {
        const char *status = doc["status"];
        const char *message = doc["message"];

        Serial.printf("🪧 Status: %s\n", status);
        Serial.printf("💬 Message: %s\n", message);
      }
      else
      {
        Serial.print("⚠️ JSON parse error: ");
        Serial.println(error.c_str());
        Serial.println("Payload was:");
        Serial.println(payload);
      }
    }
    else
    {
      Serial.printf("❌ HTTP GET failed!: %s\n", http.errorToString(httpCode).c_str());
    }
  }
  else
  {
    Serial.printf("❌ HTTP GET failed to connect: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  Serial.println("================================End Of RESPONSE================================");
}

// Sends the image data to the configured server using HTTP POST multipart/form-data.
// This function now correctly implements streaming using the http.POST("") method
// and writing to the stream pointer, which is more broadly compatible.
String sendToServer2(uint8_t *imageData, size_t imagesize)
{
  HTTPClient http;

  String url = "http://192.168.254.15:8000/classify"; // Replace with your URL

  // Add timeout settings
  http.setTimeout(15000);       // 15 seconds timeout
  http.setConnectTimeout(5000); // 5 seconds connection timeout

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  int contentLength = bodyStart.length() + imagesize + bodyEnd.length();

  uint8_t *payload = (uint8_t *)malloc(contentLength);
  if (!payload)
  {
    Serial.println("❌ Memory Allocation Failed!");
    return "";
  }

  memcpy(payload, bodyStart.c_str(), bodyStart.length());
  memcpy(payload + bodyStart.length(), imageData, imagesize);
  memcpy(payload + bodyStart.length() + imagesize, bodyEnd.c_str(), bodyEnd.length());

  Serial.println("📡 Connecting to: " + url);
  http.begin(url);

  String contentTypeHeader = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentTypeHeader);

  Serial.println("📤 Sending POST request... (Content-Length: " + String(contentLength) + ")");
  int httpCode = http.POST(payload, contentLength);

  free(payload);

  Serial.println("🔄 HTTP Response Code: " + String(httpCode));

  String response = "";
  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      response = http.getString();
      Serial.println("📨 Raw Server Response: " + response);
    }
    else
    {
      Serial.println("❌ HTTP Error: " + String(httpCode));
      response = http.getString(); // Sometimes error responses contain useful info
      Serial.println("📨 Error Response: " + response);
    }
  }
  else
  {
    Serial.println("❌ Connection Error: " + String(httpCode));
    Serial.println("Possible causes: Network timeout, server unreachable, or connection refused");
  }

  http.end();
  return response;
}
