#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h> 
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h> // Added for Video Storage

// --- CONFIGURATION ---
const char* ssid = "shashanksamir_2.4";      // Hardcoded SSID
const char* password = "CLB262BBD6";         // Hardcoded Password
const char* apiKey = "TIKDEVKCTBBKQSX4";     // Hardcoded API Key

// Timezone (Nepal +5:45)
const long  gmtOffset_sec = 20700; 
const int   daylightOffset_sec = 0;

// --- HARDWARE ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- CUSTOM I2C FOR AHT20 ---
#define AHT_SDA 33
#define AHT_SCL 32
TwoWire I2CAHT = TwoWire(1); // Create a second I2C bus instance
Adafruit_AHTX0 aht; // AHT20 Object

// --- WEB SERVER, PREFERENCES & FILESYSTEM ---
WebServer server(80);
Preferences preferences;
File videoFile; // For the video player

// --- GLOBAL VARIABLES ---
int currentMode = 0; // 0=Boot, 1=Arch, 2=Eyes, 3=Custom Text, 4=Video
String customText = "Hello World";
int oledBrightness = 100;

// NEW: Framerate and Upload State Variables
int currentFPS = 16;
int frameDelay = 1000 / 16; 
bool uploadsEnabled = true;

float temp = 0;
int hum = 0;
bool hasValidData = false;
String lastUploadTimeStr = "WAIT";

// Timers
unsigned long prevMillisUpload = 0;
unsigned long prevMillisSensor = 0;
unsigned long prevMillisBuffer = 0;
unsigned long prevMillisDisplay = 0;
unsigned long lastSwitch = 0;

const int archTime = 2100;
const int eyesTime = 20000;

// ================= ARCH LOGO BITMAP =================
const unsigned char arch_logo[] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x6f, 0xf0, 0x01, 0xfe, 0x3b, 0x9f, 0xc7, 0x78, 0x30, 0x03, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x77, 0xf0, 0x01, 0xff, 0x3f, 0xbf, 0xe7, 0xfe, 0x32, 0x2f, 0xc6, 0x02, 0x40, 0x80, 
0x00, 0x00, 0xff, 0xf8, 0x01, 0xcf, 0x3f, 0x7f, 0xe7, 0xff, 0x32, 0x38, 0x66, 0x02, 0x21, 0x80, 
0x00, 0x00, 0xff, 0xf8, 0x00, 0x77, 0x3c, 0x78, 0x47, 0x07, 0x12, 0x30, 0x26, 0x02, 0x33, 0x00, 
0x00, 0x01, 0xff, 0xfc, 0x01, 0xff, 0x38, 0xf0, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x1a, 0x00, 
0x00, 0x01, 0xff, 0xfc, 0x03, 0xff, 0x38, 0xe0, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x0c, 0x00, 
0x00, 0x03, 0xff, 0xfe, 0x07, 0x8f, 0x38, 0xe0, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x0c, 0x00, 
0x00, 0x03, 0xff, 0xfe, 0x07, 0x07, 0x38, 0xf0, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x0e, 0x00, 
0x00, 0x07, 0xff, 0xff, 0x07, 0x07, 0x38, 0x70, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x12, 0x00, 
0x00, 0x07, 0xf8, 0xff, 0x83, 0xcf, 0x38, 0x7c, 0xe7, 0x07, 0x12, 0x20, 0x26, 0x06, 0x31, 0x00, 
0x00, 0x07, 0xff, 0xff, 0x07, 0x07, 0x38, 0x70, 0x07, 0x07, 0x12, 0x20, 0x26, 0x02, 0x12, 0x00, 
0x00, 0x07, 0xf8, 0xff, 0x83, 0xcf, 0x38, 0x7c, 0xe7, 0x07, 0x12, 0x20, 0x26, 0x06, 0x31, 0x00, 
0x00, 0x0f, 0xf0, 0x7f, 0x83, 0xff, 0x38, 0x3f, 0xf7, 0x07, 0x12, 0x20, 0x22, 0x0e, 0x61, 0x80, 
0x00, 0x0f, 0xf0, 0x3f, 0xc1, 0xfb, 0x38, 0x1f, 0xc7, 0x07, 0x12, 0x20, 0x23, 0xfa, 0x40, 0xc0, 
0x00, 0x1f, 0xe0, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 
0x00, 0x1f, 0xe0, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x3f, 0xe0, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x3f, 0xe0, 0x1f, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x7f, 0xe0, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x7f, 0xe0, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0xff, 0x80, 0x07, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0xfc, 0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0xf0, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x03, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}; 

// ================= BOOT DATA =================
const char* bootMessages[] = {
  "INIT: version 2.96",
  "starting version 247.3",
  "[/] loading linux.sys",
  "[/] loading initramfs",
  "[OK] mounted /dev/sda2",
  "starting Arch Linux..."
};
int bootLine = 0;
unsigned long lastLineTime = 0;

// ================= EYE ANIMATION DATA =================
int eyeWidth = 20;
int eyeHeight = 25;
int cornerRadius = 4;

float leftEyeX = 30; 
float rightEyeX = 78;
float eyeY = 20;

float targetLeftX = 30;
float targetRightX = 78;
float targetY = 20;
float moveSpeed = 0.18; 

int blinkState = 0;
unsigned long lastBlinkTime = 0;
unsigned long nextMoveTime = 0;

int archOffsetY = 64; // Keeps track of Arch sliding

// ================= WEB DASHBOARD HTML =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP32 OLED Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; margin: 0; padding: 20px; background: #121212; color: #e0e0e0;}
    .container { max-width: 500px; margin: auto; background: #1e1e1e; padding: 30px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
    h2 { margin-top: 0; color: #ffffff; }
    button { padding: 12px 20px; margin: 8px; cursor: pointer; border: none; border-radius: 6px; background-color: #3498db; color: white; font-weight: bold; transition: 0.3s; width: 80%; max-width: 300px; }
    button:hover { background-color: #2980b9; }
    input[type="text"] { padding: 10px; width: 75%; max-width: 280px; margin-bottom: 10px; border-radius: 6px; border: 1px solid #444; background: #2a2a2a; color: white; }
    input[type="range"] { width: 80%; max-width: 300px; margin: 15px 0; }
    form { background-color: #2a2a2a; padding: 15px; border-radius: 8px; margin-top: 15px; }
    .radio-group { background: #2a2a2a; padding: 15px; border-radius: 8px; margin-top: 15px; text-align: left; margin-left: 10%; margin-right: 10%;}
  </style>
</head><body>
  <div class="container">
    <h2>OLED Dashboard</h2>
    
    <button onclick="setMode(1)">Show Arch Logo</button><br>
    <button onclick="setMode(2)">Show Eyes</button><br>
    <button onclick="setMode(3)">Show Custom Text</button><br>
    <button onclick="setMode(4)" style="background-color: #9b59b6;">Play Custom Video</button><br>
    
    <hr style="border: 1px solid #333; margin: 25px 0;">
    
    <input type="text" id="textInput" placeholder="Enter your text here..."><br>
    <button onclick="updateText()" style="background-color: #2ecc71;">Set New Text</button>
    
    <hr style="border: 1px solid #333; margin: 25px 0;">

    <label><strong>OLED Brightness: </strong><span id="briVal">100</span></label><br>
    <input type="range" min="1" max="255" value="100" id="briSlider" oninput="updateVal('briVal', this.value)" onchange="setBrightness(this.value)">

    <br><br>
    <label><strong>OLED Framerate (FPS): </strong><span id="fpsVal">16</span></label><br>
    <input type="range" min="1" max="60" value="16" id="fpsSlider" oninput="updateVal('fpsVal', this.value)" onchange="setFPS(this.value)">

    <hr style="border: 1px solid #333; margin: 25px 0;">

    <div class="radio-group">
      <label><strong>Sensor Upload Status:</strong></label><br><br>
      <input type="radio" id="uploadOn" name="uploadToggle" value="1" onchange="setUploadState(1)">
      <label for="uploadOn">Enabled (Shows white dot)</label><br><br>
      <input type="radio" id="uploadOff" name="uploadToggle" value="0" onchange="setUploadState(0)">
      <label for="uploadOff">Paused (Removes white dot)</label>
    </div>

    <hr style="border: 1px solid #333; margin: 25px 0;">
    
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <h3 style="color:#f1c40f; margin-top:0;">Upload Video File (.bin)</h3>
      <p style="font-size:12px; color:#aaaaaa;">File must be 1024 bytes per frame (128x64)</p>
      <input type="file" name="video_data" accept=".bin" required style="color:white; margin-bottom: 15px;">
      <br>
      <button type="submit" style="background-color: #e67e22;">Upload & Play</button>
    </form>

  </div>

  <script>
    window.onload = function() {
      fetch('/getBrightness').then(response => response.text()).then(val => {
        document.getElementById('briSlider').value = val;
        document.getElementById('briVal').innerText = val;
      });
      fetch('/getFPS').then(response => response.text()).then(val => {
        document.getElementById('fpsSlider').value = val;
        document.getElementById('fpsVal').innerText = val;
      });
      fetch('/getUpload').then(response => response.text()).then(val => {
        if(val == "1") document.getElementById('uploadOn').checked = true;
        else document.getElementById('uploadOff').checked = true;
      });
    }

    function setMode(m) { fetch('/setMode?m=' + m); }
    
    function updateText() { 
      let t = document.getElementById('textInput').value;
      if(t !== "") { fetch('/setText?t=' + encodeURIComponent(t)); }
    }
    
    function updateVal(target, val) { document.getElementById(target).innerText = val; }
    
    function setBrightness(val) { fetch('/setBrightness?v=' + val); }
    function setFPS(val) { fetch('/setFPS?v=' + val); }
    function setUploadState(val) { fetch('/setUpload?v=' + val); }
  </script>
</body></html>
)rawliteral";


// --- FUNCTION PROTOTYPES ---
void uploadData();
String getFormattedTime();
void updateEyePhysics(unsigned long currentTime);
void handleFileUpload();

void setup() {
  Serial.begin(115200);

  // --- Initialize Preferences (Memory) ---
  preferences.begin("oled-app", false);
  customText = preferences.getString("custText", "Arch Linux"); 
  oledBrightness = preferences.getInt("brightness", 100);       
  currentFPS = preferences.getInt("fps", 16); 
  frameDelay = 1000 / currentFPS;
  uploadsEnabled = preferences.getBool("upload", true);

  // --- Initialize Default I2C bus for OLED ---
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  // Apply saved brightness
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(oledBrightness);
  display.display();

  // --- Initialize Custom I2C for AHT20 ---
  I2CAHT.begin(AHT_SDA, AHT_SCL);
  if (!aht.begin(&I2CAHT)) {
    Serial.println("Could not find AHT20 sensor. Check wiring!");
  }

  // --- Initialize LittleFS for Video Storage ---
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  } else {
    Serial.println("LittleFS mounted successfully");
  }

  // --- Connect to WiFi ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("Go to this IP in your browser: "); 
  Serial.println(WiFi.localIP());
  
  // --- Init Time ---
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  // --- Setup Web Server Routes ---
  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", index_html);
  });

  server.on("/setMode", HTTP_GET, [](){
    if(server.hasArg("m")){
      currentMode = server.arg("m").toInt();
      lastSwitch = millis(); // Reset timers so animations start fresh
      if(currentMode == 1) archOffsetY = 64; // Reset arch slide animation
      
      // Close the video file to save memory if we aren't in video mode
      if(currentMode != 4 && videoFile) {
        videoFile.close();
      }
    }
    server.send(200, "text/plain", "Mode changed");
  });

  server.on("/setText", HTTP_GET, [](){
    if(server.hasArg("t")){
      customText = server.arg("t");
      preferences.putString("custText", customText); // Save to memory
      currentMode = 3; // Auto-switch to text mode
      if(videoFile) videoFile.close();
      lastSwitch = millis();
    }
    server.send(200, "text/plain", "Text updated");
  });

  server.on("/setBrightness", HTTP_GET, [](){
    if(server.hasArg("v")){
      oledBrightness = server.arg("v").toInt();
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(oledBrightness);
      preferences.putInt("brightness", oledBrightness); 
    }
    server.send(200, "text/plain", "Brightness updated");
  });

  server.on("/getBrightness", HTTP_GET, [](){
    server.send(200, "text/plain", String(oledBrightness));
  });

  // NEW: FPS Routes
  server.on("/setFPS", HTTP_GET, [](){
    if(server.hasArg("v")){
      currentFPS = server.arg("v").toInt();
      if(currentFPS < 1) currentFPS = 1;
      frameDelay = 1000 / currentFPS;
      preferences.putInt("fps", currentFPS);
    }
    server.send(200, "text/plain", "FPS updated");
  });

  server.on("/getFPS", HTTP_GET, [](){
    server.send(200, "text/plain", String(currentFPS));
  });

  // NEW: Upload State Routes
  server.on("/setUpload", HTTP_GET, [](){
    if(server.hasArg("v")){
      uploadsEnabled = (server.arg("v").toInt() == 1);
      preferences.putBool("upload", uploadsEnabled);
    }
    server.send(200, "text/plain", "Upload state updated");
  });

  server.on("/getUpload", HTTP_GET, [](){
    server.send(200, "text/plain", uploadsEnabled ? "1" : "0");
  });

  // Route to handle the video upload and stream
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/html", "<h2>Upload Complete! Playing Video...</h2><script>setTimeout(function(){window.location.href='/';},1500);</script>");
  }, handleFileUpload);

  server.begin();
}

void loop() {
  server.handleClient(); // Listen for web dashboard commands
  
  unsigned long currentMillis = millis();

  // ==========================================
  // BACKGROUND TASK 1: SENSOR (Read Every 16s)
  // ==========================================
  if (currentMillis - prevMillisSensor >= 16000) {
    prevMillisSensor = currentMillis;
    
    sensors_event_t humidity_event, temp_event;
    aht.getEvent(&humidity_event, &temp_event); 
    
    float newT = temp_event.temperature;
    int newH = humidity_event.relative_humidity;

    if (!isnan(newT) && newT != 0 && newT < 80 && newH != 0) { 
      temp = newT;
      hum = newH;
      hasValidData = true; 

      Serial.print("AHT20 Reading -> Temp: ");
      Serial.print(temp);
      Serial.print(" °C | Humidity: ");
      Serial.print(hum);
      Serial.println(" %");
    }
  }

  // ==========================================
  // BACKGROUND TASK 2: BUFFER CLEAR (10 MIN)
  // ==========================================
  if (currentMillis - prevMillisBuffer >= 600000) {
    prevMillisBuffer = currentMillis;
    Serial.println("\n--- 10 Minute Mark: Clearing Serial Buffers ---\n");
    Serial.print("Go to this IP in your browser: "); 
    Serial.println(WiFi.localIP());
    Serial.flush(); 
    while(Serial.available() > 0) {
      Serial.read();
    }
  }

  // ==========================================
  // BACKGROUND TASK 3: UPLOAD (Every 16s)
  // ==========================================
  // Wrapped inside the uploadsEnabled check to prevent interruptions
  if (uploadsEnabled && (currentMillis - prevMillisUpload >= 16000)) {
    prevMillisUpload = currentMillis;
    if (hasValidData) {
      uploadData();
    }
  }

  // ==========================================
  // FOREGROUND TASK: ANIMATION STATE MACHINE
  // ==========================================
  // Now uses the dynamic frameDelay set from the dashboard
  if (currentMillis - prevMillisDisplay >= frameDelay) { 
    prevMillisDisplay = currentMillis;
    
    display.clearDisplay();

    if (currentMode == 0) {
      // --- BOOT SEQUENCE ---
      display.setTextSize(1);
      for (int i = 0; i <= bootLine; i++) {
        display.setCursor(0, i * 10);
        display.print(bootMessages[i]);
      }
      if (currentMillis - lastLineTime > 600) {
        bootLine++;
        lastLineTime = currentMillis;
      }
      if (bootLine > 5) {
        currentMode = 1; // Auto-transition to Arch
        lastSwitch = currentMillis;
        archOffsetY = 64; 
      }
      
    } else if (currentMode == 1) {
      // --- ARCH LOGO SLIDE UP ---
      if (archOffsetY > 0) archOffsetY -= 2;

      display.drawBitmap(0, archOffsetY, arch_logo, 128, 64, WHITE);
      
      if (archOffsetY <= 0) {
        display.fillRect(0, 60, 128, 4, BLACK); // Noise fix mask
      }

      // Auto-switch to Eyes if time is up
      if (currentMillis - lastSwitch > archTime) {
        currentMode = 2; 
        lastSwitch = currentMillis;
      }
      
    } else if (currentMode == 2) {
      // --- MOVING EYES ---
      updateEyePhysics(currentMillis);
      
      if (blinkState == 0) {
        display.fillRoundRect((int)leftEyeX, (int)eyeY, eyeWidth, eyeHeight, cornerRadius, WHITE);
        display.fillRoundRect((int)rightEyeX, (int)eyeY, eyeWidth, eyeHeight, cornerRadius, WHITE);
      } else {
        // Blink line
        display.fillRect((int)leftEyeX, (int)eyeY + (eyeHeight / 2), eyeWidth, 2, WHITE);
        display.fillRect((int)rightEyeX, (int)eyeY + (eyeHeight / 2), eyeWidth, 2, WHITE);
      }

      // Auto-switch back to Arch if time is up
      if (currentMillis - lastSwitch > eyesTime) {
        currentMode = 1; 
        lastSwitch = currentMillis;
        archOffsetY = 64; // Reset animation
      }
      
    } else if (currentMode == 3) {
      // --- CUSTOM TEXT MODE ---
      // This mode stays active indefinitely until changed via Web Dashboard
      display.setTextSize(2);
      // Center-ish formatting
      display.setCursor(0, 20); 
      display.println(customText);
      
    } else if (currentMode == 4) {
      // --- VIDEO PLAYER MODE ---
      // Open file if it isn't open
      if (!videoFile || !videoFile.available()) {
        videoFile = LittleFS.open("/custom_video.bin", "r");
        if (!videoFile) {
          Serial.println("No video file found! Reverting to Arch Logo.");
          currentMode = 1; 
          archOffsetY = 64;
          return;
        }
      }

      if (videoFile) {
        // Loop video if we reach the end
        if (videoFile.available() < 1024) {
          videoFile.seek(0); 
        }

        // Read 1 frame and draw it
        uint8_t frameBuffer[1024];
        videoFile.read(frameBuffer, 1024);
        display.drawBitmap(0, 0, frameBuffer, 128, 64, WHITE);
      }
    }

    display.display();
  }
}

// ================= HELPER FUNCTIONS =================

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.println("Upload Started...");
    videoFile = LittleFS.open("/custom_video.bin", "w"); 
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (videoFile) {
      videoFile.write(upload.buf, upload.currentSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (videoFile) {
      videoFile.close();
      Serial.print("Upload Finished! Size: ");
      Serial.println(upload.totalSize);
      currentMode = 4; // Switch to video mode automatically
      lastSwitch = millis();
    }
  }
}

void uploadData() {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    
    // Flash a tiny white dot in the bottom right corner when uploading
    display.fillRect(118, 54, 8, 8, WHITE); 
    display.display();

    String url = "https://api.thingspeak.com/update?api_key=";
    url += apiKey;
    url += "&field1=";
    url += String(temp, 1);
    url += "&field2=";
    url += String(hum);
    
    http.begin(url.c_str());
    int httpCode = http.GET();
    
    if(httpCode == 200) {
      lastUploadTimeStr = getFormattedTime(); 
    } 
    http.end();
  }
}

String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "--:--";
  }
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void updateEyePhysics(unsigned long currentTime) {
  if (currentTime > nextMoveTime && blinkState == 0) {
    targetLeftX = random(10, 45); 
    targetRightX = targetLeftX + 48; // Maintains the gap
    targetY = random(10, 30);
    nextMoveTime = currentTime + random(2000, 4500);
  }

  leftEyeX  += (targetLeftX - leftEyeX) * moveSpeed;
  rightEyeX += (targetRightX - rightEyeX) * moveSpeed;
  eyeY      += (targetY - eyeY) * moveSpeed;

  if (currentTime - lastBlinkTime > 3000) {
    blinkState = 1;
    if (currentTime - lastBlinkTime > 3150) { 
      blinkState = 0;
      lastBlinkTime = currentTime;
    }
  }
}