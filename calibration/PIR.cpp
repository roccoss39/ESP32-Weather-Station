/*
 * ESP32 PIR Sensor Test - ENHANCED DEBUG VERSION
 * PIR sensor connected to GPIO pin 27
 * 
 * This program provides detailed debugging information about PIR sensor behavior
 * and shows how to adjust potentiometers for optimal performance.
 */

#include <Arduino.h>

// ===== CONFIGURATION =====
#define PIR_PIN 27        // PIR sensor connected to GPIO 27
#define LED_BUILTIN 5     // Built-in LED for visual indication (pin 5 on your ESP32 board)

// PIR Logic Selection:
// false = Normal logic (HIGH = motion, LOW = no motion) - most common PIR sensors
// true  = Inverted logic (LOW = motion, HIGH = no motion) - some PIR modules with jumper in L mode
#define PIR_INVERTED_LOGIC false

bool motionDetected = false;
bool lastState = false;
unsigned long lastMotionTime = 0;
unsigned long lastStateChangeTime = 0;
unsigned long motionCount = 0;
unsigned long noMotionCount = 0;
unsigned long totalHighTime = 0;
unsigned long totalLowTime = 0;

// Debug counters
unsigned long debugCounter = 0;
unsigned long lastDebugTime = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  // Initialize pins
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Turn off LED initially
  digitalWrite(LED_BUILTIN, LOW);
  
  Serial.println("=== ESP32 PIR Sensor ENHANCED DEBUG Test ===");
  Serial.println("PIR sensor connected to GPIO pin 27");
  Serial.print("PIR Logic Mode: ");
  Serial.println(PIR_INVERTED_LOGIC ? "INVERTED (LOW=motion)" : "NORMAL (HIGH=motion)");
  Serial.println();
  Serial.println("🔧 POTENTIOMETER ADJUSTMENT GUIDE:");
  Serial.println("1. SENSITIVITY (Sx) - Controls detection range");
  Serial.println("   - Clockwise: Higher sensitivity (3-7m range)");
  Serial.println("   - Counter-clockwise: Lower sensitivity (1-3m range)");
  Serial.println("2. TIME DELAY (Tx) - Controls output duration");
  Serial.println("   - Clockwise: Longer delay (5-300 seconds)");
  Serial.println("   - Counter-clockwise: Shorter delay (0.3-5 seconds)");
  Serial.println();
  Serial.println("💡 TIPS:");
  Serial.println("- Start with SENSITIVITY at middle position");
  Serial.println("- Set TIME DELAY to minimum (counter-clockwise)");
  Serial.println("- Adjust based on test results below");
  Serial.println();
  Serial.println("Waiting for sensor to stabilize...");
  
  // Extended stabilization time with countdown
  for (int i = 10; i > 0; i--) {
    Serial.print("Stabilizing... ");
    Serial.print(i);
    Serial.println("s remaining");
    delay(1000);
  }
  
  Serial.println();
  Serial.println("🚀 PIR sensor ready!");
  Serial.println("📊 Starting detailed monitoring...");
  Serial.println("----------------------------------------");
  
  lastStateChangeTime = millis();
  lastDebugTime = millis();
}

void loop() {
  // Read PIR sensor state with configurable logic
  int pirRawState = digitalRead(PIR_PIN);
  motionDetected = PIR_INVERTED_LOGIC ? !pirRawState : pirRawState;
  debugCounter++;
  
  // Check for state change
  if (motionDetected != lastState) {
    unsigned long currentTime = millis();
    unsigned long stateDuration = currentTime - lastStateChangeTime;
    
    if (motionDetected) {
      // Motion detected
      motionCount++;
      lastMotionTime = currentTime;
      totalLowTime += stateDuration;
      
      Serial.print("🚨 [");
      Serial.print(currentTime / 1000.0, 1);
      Serial.print("s] MOTION DETECTED! ");
      Serial.print("(Count: ");
      Serial.print(motionCount);
      Serial.print(", Low duration: ");
      Serial.print(stateDuration);
      Serial.println("ms)");
      
      // Turn on built-in LED
      digitalWrite(LED_BUILTIN, HIGH);
      
    } else {
      // Motion stopped
      noMotionCount++;
      unsigned long motionDuration = currentTime - lastMotionTime;
      totalHighTime += stateDuration;
      
      Serial.print("✅ [");
      Serial.print(currentTime / 1000.0, 1);
      Serial.print("s] Motion STOPPED ");
      Serial.print("(Duration: ");
      Serial.print(motionDuration);
      Serial.print("ms, High time: ");
      Serial.print(stateDuration);
      Serial.println("ms)");
      
      // Turn off built-in LED
      digitalWrite(LED_BUILTIN, LOW);
    }
    
    lastState = motionDetected;
    lastStateChangeTime = currentTime;
  }
  
  // Detailed status every 5 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastDebugTime > 5000) {
    Serial.println();
    Serial.print("📊 [");
    Serial.print(currentTime / 1000.0, 1);
    Serial.println("s] === STATUS REPORT ===");
    Serial.print("Current state: ");
    Serial.println(motionDetected ? "🔴 MOTION" : "🟢 NO MOTION");
    Serial.print("Total detections: ");
    Serial.println(motionCount);
    Serial.print("Total stops: ");
    Serial.println(noMotionCount);
    Serial.print("Debug reads: ");
    Serial.println(debugCounter);
    Serial.print("Avg HIGH time: ");
    Serial.print(motionCount > 0 ? totalHighTime / motionCount : 0);
    Serial.println("ms");
    Serial.print("Avg LOW time: ");
    Serial.print(noMotionCount > 0 ? totalLowTime / noMotionCount : 0);
    Serial.println("ms");
    
    // Current state duration
    unsigned long currentStateDuration = currentTime - lastStateChangeTime;
    Serial.print("Current state duration: ");
    Serial.print(currentStateDuration);
    Serial.println("ms");
    
    Serial.println("💡 TIPS:");
    if (motionCount == 0) {
      Serial.println("- No motion detected yet. Try moving closer or increase SENSITIVITY");
    } else if (totalHighTime / motionCount > 10000) {
      Serial.println("- HIGH time very long. Decrease TIME DELAY potentiometer");
    } else if (totalHighTime / motionCount < 500) {
      Serial.println("- HIGH time very short. Increase TIME DELAY slightly");
    }
    Serial.println("----------------------------------------");
    
    lastDebugTime = currentTime;
    debugCounter = 0;  // Reset counter
  }
  
  // Fast sampling for accurate timing
  delay(10);
}

/*
 * Expected behavior:
 * 1. When motion is detected, the built-in LED should turn on
 * 2. Serial monitor should show "MOTION DETECTED!" messages
 * 3. When motion stops, LED turns off and duration is shown
 * 4. Motion counter keeps track of total detections
 * 
 * Troubleshooting:
 * - If no motion is ever detected, check wiring and power supply
 * - PIR sensors typically need 5V power (VCC to 5V, GND to GND, OUT to GPIO 27)
 * - Some PIR sensors have sensitivity and delay adjustment potentiometers
 * - Make sure the sensor has warmed up (can take 10-60 seconds after power on)
 */