#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1      // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Default I2C address

// Define the pins we're actually using
#define SDA_PIN 5  // D4
#define SCL_PIN 7  // D8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Scrolling text configuration
const char* scrollText = "Hello! This is a long scrolling message that can display any length of text...";
int textWidth;           // Width of our text in pixels
int xPos;               // Current x position of text
int scrollSpeed = 2;    // How many pixels to move per frame
unsigned long lastScrollTime = 0;
const int scrollDelay = 50;  // Delay between each movement (in milliseconds)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("OLED test starting...");
  
  // Start I2C Communication with the correct pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  
  Serial.println("Display initialized successfully!");
  
  // Clear the buffer
  display.clearDisplay();
  
  // Set text properties
  display.setTextSize(2);              // Make text larger
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setTextWrap(false);          // Don't wrap text to next line
  
  // Calculate the width of the text
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(scrollText, 0, 0, &x1, &y1, &w, &h);
  textWidth = w;
  
  // Start text position at right edge of screen
  xPos = SCREEN_WIDTH;
}

void loop() {
  // Check if it's time to update the scroll position
  if (millis() - lastScrollTime >= scrollDelay) {
    // Move text position
    xPos -= scrollSpeed;
    
    // If text has scrolled completely off screen, reset to right edge
    if (xPos < -textWidth) {
      xPos = SCREEN_WIDTH;
    }
    
    // Clear display
    display.clearDisplay();
    
    // Draw text at current position
    display.setCursor(xPos, (SCREEN_HEIGHT - 16) / 2); // Center vertically (16 is text height)
    display.print(scrollText);
    
    // Show the display buffer on the screen
    display.display();
    
    // Update last scroll time
    lastScrollTime = millis();
  }
}