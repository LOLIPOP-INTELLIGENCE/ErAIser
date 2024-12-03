#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// WiFi credentials
const char* ssid = "iBam";
const char* password = "123456789";

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define SDA_PIN 5
#define SCL_PIN 7

// Python server details 
const char* serverUrl = "http://172.20.10.3:5002/upload";
const char* finishUrl = "http://172.20.10.3:5002/finished";
const char* responseUrl = "http://172.20.10.3:5002/get_response";

// Streaming control
const int buttonPin = 8;
bool isItStreaming = false;
unsigned long lastDebounceTime = 0;  // Last time the button was pressed
unsigned long debounceDelay = 50;    // Debounce time in milliseconds
int lastButtonState = HIGH;          // Previous reading from the button
int buttonState;                     // Current stable state of the button

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Scrolling text configuration
String currentDisplayText = "Waiting for response...";
int textWidth;
int xPos;
const int scrollSpeed = 2;
unsigned long lastScrollTime = 0;
const int scrollDelay = 50;

void startCameraServer();
void setupLedFlash(int pin);

void updateScrollingText(const char* newText) {
    currentDisplayText = String(newText);
    
    // Recalculate text width
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(2);
    display.getTextBounds(currentDisplayText.c_str(), 0, 0, &x1, &y1, &w, &h);
    textWidth = w;
    
    // Reset position to start scrolling from right
    xPos = SCREEN_WIDTH;
}

void getGPTResponse() {
    HTTPClient http;
    int attempts = 0;
    const int MAX_ATTEMPTS = 60;  // Try for 60 seconds
    
    Serial.println("Waiting for GPT response...");
    updateScrollingText("Waiting for GPT response...");
    
    while (attempts < MAX_ATTEMPTS) {
        http.begin(responseUrl);
        int httpCode = http.GET();
        
        if (httpCode == 200) {  // Response is ready
            String payload = http.getString();
            StaticJsonDocument<2048> doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                const char* response = doc["response"];
                Serial.println("\nGPT Response:");
                Serial.println(response);
                updateScrollingText(response);
                http.end();
                return;
            }
        }
        else if (httpCode == 202) {  // Still processing
            if (attempts % 5 == 0) {  // Print status every 5 seconds
                Serial.println("Still processing...");
                updateScrollingText("Still processing...");
            }
        }
        else {
            Serial.printf("Error getting response: %d\n", httpCode);
            updateScrollingText("Error getting response");
        }
        
        http.end();
        delay(1000);
        attempts++;
    }
    
    Serial.println("Timeout waiting for GPT response");
    updateScrollingText("Timeout waiting for response");
}

// Function to send finished notification
void sendFinishedNotification() {
    HTTPClient http;
    http.begin(finishUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Send the POST request
    int httpResponseCode = http.POST("{}");
    http.end();

    Serial.println("Processing started...");
    getGPTResponse();
}

// Function to capture and send photo
void captureAndSendPhoto() {
  camera_fb_t * fb = NULL;
  
  // Take a photo
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");
  
  // Send the image data
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  if (httpResponseCode > 0) {
    Serial.printf("Frame sent. Response: %d\n", httpResponseCode);
  } else {
    Serial.printf("Error sending frame: %d\n", httpResponseCode);
  }
  
  // Clean up
  http.end();
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  while(!Serial);
  Serial.setDebugOutput(true);
  Serial.println();

  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println("SSD1306 allocation failed");
      for(;;);
  } else {
    Serial.println("SSD1306 Display initialisation success");
  }  

  // Setup display initial state
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  updateScrollingText("Ready to start...");

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_HD;  // Set to HD resolution
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 4;  // High quality
  config.fb_count = 1;
  
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, FRAMESIZE_HD);
  s->set_quality(s, 4);
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  buttonState = digitalRead(buttonPin);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("Click button to start streaming");
  updateScrollingText("WiFi connected. Press button to start.");
}

void loop() {

  int reading = digitalRead(buttonPin);

  // If the button state has changed, reset the debouncing timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }  

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }  

  // Check if enough time has passed since the last state change
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // Only toggle if the new button state is LOW (pressed)
      if (buttonState == LOW) {
        bool previousStreamingState = isItStreaming;
        isItStreaming = !isItStreaming;
        if (isItStreaming) {
          updateScrollingText("Starting stream...");
        }
        Serial.println(isItStreaming ? "Starting stream..." : "Stopping stream...");

        if (previousStreamingState && !isItStreaming) {
            sendFinishedNotification();
        }        
      }
    }
  }

  lastButtonState = reading;


  if (isItStreaming) {
    captureAndSendPhoto();
    delay(200);  // ~5 FPS
  }

  // Update scrolling text
  if (millis() - lastScrollTime >= scrollDelay) {
      xPos -= scrollSpeed;
      
      if (xPos < -textWidth) {
          xPos = SCREEN_WIDTH;
      }
      
      display.clearDisplay();
      display.setCursor(xPos, (SCREEN_HEIGHT - 16) / 2);
      display.print(currentDisplayText);
      display.display();
      
      lastScrollTime = millis();
  }  
  
  delay(10);  // Small delay when not streaming
}