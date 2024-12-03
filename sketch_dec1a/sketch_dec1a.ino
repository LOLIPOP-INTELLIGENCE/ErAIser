#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// WiFi credentials
const char* ssid = "iBam";
const char* password = "123456789";

// Python server details 
const char* serverUrl = "http://172.20.10.3:5002/upload";
const char* finishUrl = "http://172.20.10.3:5002/finished";

// Streaming control
const int buttonPin = 8;
bool isItStreaming = false;
unsigned long lastDebounceTime = 0;  // Last time the button was pressed
unsigned long debounceDelay = 50;    // Debounce time in milliseconds
int lastButtonState = HIGH;          // Previous reading from the button
int buttonState;                     // Current stable state of the button


void startCameraServer();
void setupLedFlash(int pin);

// Function to send finished notification
void sendFinishedNotification() {
    HTTPClient http;
    http.begin(finishUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Send the POST request
    int httpResponseCode = http.POST("{}");
    
    if (httpResponseCode > 0) {
        Serial.printf("Finished notification sent. Response: %d\n", httpResponseCode);
    } else {
        Serial.printf("Error sending finished notification: %d\n", httpResponseCode);
    }
    
    http.end();
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
  
  delay(10);  // Small delay when not streaming
}