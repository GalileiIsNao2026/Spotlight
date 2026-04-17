#include <Arduino.h>

#define MIC_AO_PIN A0
#define MIC_DO_PIN 2

// Se il tuo modulo non ha D0 collegato, lascia pure scollegato MIC_DO_PIN.
// Baud rate seriale
const long SERIAL_BAUD = 115200;
const uint16_t WINDOW_MS = 40;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1200);

  pinMode(MIC_AO_PIN, INPUT);
  pinMode(MIC_DO_PIN, INPUT);

  Serial.println("=== TEST MICROFONO MKR ===");
  Serial.println("Collegamenti:");
  Serial.println("VCC -> 3.3V, GND -> GND, AO -> A0, DO -> D2 (opzionale)");
  Serial.println("Parla o fai un battito di mani vicino al microfono.");
  Serial.println();
}

void loop() {
  uint16_t minV = 1023;
  uint16_t maxV = 0;
  uint32_t sum = 0;
  uint16_t n = 0;

  unsigned long t0 = millis();
  while (millis() - t0 < WINDOW_MS) {
    uint16_t sample = analogRead(MIC_AO_PIN); // 0..1023
    if (sample < minV) minV = sample;
    if (sample > maxV) maxV = sample;
    sum += sample;
    n++;
  }

  uint16_t avg = (n > 0) ? (sum / n) : 0;
  uint16_t p2p = maxV - minV; // escursione segnale nella finestra
  int digitalValue = digitalRead(MIC_DO_PIN); // 0/1

  Serial.print("AVG: ");
  Serial.print(avg);
  Serial.print(" | MIN: ");
  Serial.print(minV);
  Serial.print(" | MAX: ");
  Serial.print(maxV);
  Serial.print(" | P2P: ");
  Serial.print(p2p);
  Serial.print(" | DO: ");
  Serial.println(digitalValue);

  delay(30);
}
