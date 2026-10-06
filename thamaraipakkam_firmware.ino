#include <Arduino.h>

// ==============================================================================
// THAMARAIPAKKAM FLOOD WARNING SENSOR NODE
// ==============================================================================
// SENSOR WIRING:
// JSN-SR04T (Ultrasonic): 5V, GND, TRIG->5, ECHO->18 (*** USE VOLTAGE DIVIDER ***)
// YF-S201 (Flow Sensor): 5V, GND, Data->19 
// SW-420 (Vibration): 3.3V, GND, DO->21
// ==============================================================================

// CONFIG FLAG: Identifies the source of the velocity data.
// Options: "YFS201_PLACEHOLDER", "MANNING_ESTIMATE", "RENKEER_MODBUS"
#define VELOCITY_SOURCE "YFS201_PLACEHOLDER" 

const int trigPin = 5;
const int echoPin = 18;
const int flowPin = 19;
const int vibSensorPin = 21;

unsigned long lastReadTime = 0;
unsigned long lastFlowTime = 0;
long distBuffer[5];

// Interrupt variables for YF-S201
volatile int pulseCount = 0;

void IRAM_ATTR flowInterrupt() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(vibSensorPin, INPUT);
  
  pinMode(flowPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowPin), flowInterrupt, FALLING);
  
  // Wait for serial to initialize
  delay(1000);
}

// Bubble sort to find median and filter acoustic spikes
void sortBuffer(long *buf, int len) {
  for (int i = 0; i < len - 1; i++) {
    for (int j = 0; j < len - i - 1; j++) {
      if (buf[j] > buf[j + 1]) {
        long temp = buf[j];
        buf[j] = buf[j + 1];
        buf[j + 1] = temp;
      }
    }
  }
}

// Ping the ultrasonic sensor
long getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // 30ms timeout ~ 5 meters max
  long duration = pulseIn(echoPin, HIGH, 30000); 
  
  if (duration == 0) return -1; // Timeout
  return duration * 0.034 / 2;
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Sample exactly once per second
  if (currentMillis - lastReadTime >= 1000) {
    // 1. Distance (Median of 5 pings)
    for (int i = 0; i < 5; i++) {
      distBuffer[i] = getDistanceCM();
      delay(30); 
    }
    sortBuffer(distBuffer, 5);
    long medianDist = distBuffer[2];

    // 2. Velocity Calculation (Placeholder Logic)
    unsigned long dt = currentMillis - lastFlowTime;
    
    // YF-S201 frequency formula: F = 7.5 * Q (L/min)
    float freq = (pulseCount * 1000.0) / dt; 
    float flow_lpm = freq / 7.5;
    
    pulseCount = 0; // Reset for next interval
    lastFlowTime = currentMillis;

    float surface_velocity_ms = 0.0;

    if (String(VELOCITY_SOURCE) == "YFS201_PLACEHOLDER") {
      // v = Q / A
      // Convert L/min to m^3/s: Q_m3s = flow_lpm / (60 * 1000)
      // Assume default 1/2" pipe area for the bench test: A = 0.00012667 m^2
      surface_velocity_ms = (flow_lpm / 60000.0) / 0.00012667;
    } 
    // STUB: Real Renkeer RS-series Doppler radar logic
    // else if (String(VELOCITY_SOURCE) == "RENKEER_MODBUS") {
    //   // modbus.requestFrom(1, 0x04, 0x0001, 1);
    //   // surface_velocity_ms = modbus.readRegister() / 100.0;
    // }

    // 3. Vibration
    int vib = digitalRead(vibSensorPin);

    // 4. Output ONE strict JSON line over Serial
    Serial.print("{\"t\":");
    Serial.print(currentMillis);
    
    Serial.print(",\"dist_cm\":");
    if (medianDist < 20 || medianDist > 500) {
      Serial.print("null"); // Handle sensor timeout / blind spot
    } else {
      Serial.print(medianDist);
    }
    
    Serial.print(",\"flow_lpm\":");
    Serial.print(flow_lpm, 2);
    
    Serial.print(",\"surface_velocity_ms\":");
    Serial.print(surface_velocity_ms, 3);
    
    Serial.print(",\"vib\":");
    Serial.print(vib);
    
    Serial.println("}");
    
    lastReadTime = currentMillis;
  }
}
