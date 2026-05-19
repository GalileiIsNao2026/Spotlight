#include <WiFiNINA.h>
#include <WiFiUdp.h>

// --- Modalita alimentazione ---
const bool BATTERY_MODE = true;

// --- Connessione al Modem ---
const char SSID[] = "Galileiisnao";
const char PASS[] = "SebastianoMagnano"; 

// !!! IMPORTANTE !!! 
// Sostituisci questo IP con quello che il modem assegna al tuo Arduino R4.
// Lo puoi leggere sul Monitor Seriale dell'R4 all'avvio.
IPAddress r4Ip(192, 168, 1 ,188); 

const uint16_t UDP_PORT = 4210;

// --- Microfono KY-038 ---
const uint8_t MIC_PIN = A0;
const uint8_t MIC_DO_PIN = 2;
const uint16_t SAMPLE_WINDOW_MS = BATTERY_MODE ? 16 : 20;
const uint16_t SEND_INTERVAL_MS = BATTERY_MODE ? 45 : 25;
// Sensibilita KY-038 (auto): alza MANUAL_TRIM per piu sensibilita.
const float MANUAL_TRIM = BATTERY_MODE ? 8.5f : 10.0f;
const float MIN_GAIN = 2.5f;
const float MAX_GAIN = 12.0f;
const float TARGET_LEVEL = 520.0f;
const float BASE_GATE = 0.001f;

WiFiUDP udp;
unsigned long lastSendMs = 0;

struct AudioFeatures {
  uint16_t overall;
  uint16_t low;
  uint16_t mid;
  uint16_t high;
  uint8_t peak;
};

// Filtri temporali per pseudo-bande (robusti su KY-038)
float envFast = 0.0f;
float envMid = 0.0f;
float envSlow = 0.0f;
float overallSmooth = 0.0f;
float noiseFloor = 8.0f;
float envTracker = 50.0f;

uint16_t readEnvelopeP2P() {
  unsigned long startMs = millis();
  uint16_t signalMin = 1023;
  uint16_t signalMax = 0;

  while (millis() - startMs < SAMPLE_WINDOW_MS) {
    uint16_t sample = analogRead(MIC_PIN);
    if (sample < signalMin) {
      signalMin = sample;
    }
    if (sample > signalMax) {
      signalMax = sample;
    }
  }

  uint16_t raw = signalMax - signalMin;

  noiseFloor += 0.03f * ((float)raw - noiseFloor);
  noiseFloor = constrain(noiseFloor, 0.2f, 120.0f);

  float gate = noiseFloor + BASE_GATE;
  float envelope = (raw > gate) ? ((float)raw - gate) : 0.0f;

  envTracker *= 0.985f;
  if (envelope > envTracker) envTracker = envelope;
  envTracker = constrain(envTracker, 12.0f, 700.0f);

  float autoGain = TARGET_LEVEL / envTracker;
  float finalGain = constrain(autoGain * MANUAL_TRIM, MIN_GAIN, MAX_GAIN);

  int amplified = (int)(envelope * finalGain);
  if (amplified > 1023) amplified = 1023;
  return (uint16_t)amplified;
}

AudioFeatures extractFeatures(uint16_t env) {
  float x = (float)env;

  envFast += 0.50f * (x - envFast);
  envMid += 0.20f * (x - envMid);
  envSlow += 0.05f * (x - envSlow);

  float lowF = envSlow;
  float midF = envMid - envSlow;
  float highF = envFast - envMid;

  if (midF < 0) midF = 0;
  if (highF < 0) highF = 0;

  overallSmooth += 0.22f * (x - overallSmooth);

  AudioFeatures f;
  f.overall = (uint16_t)constrain((int)overallSmooth, 0, 1023);
  f.low = (uint16_t)constrain((int)lowF, 0, 1023);
  f.mid = (uint16_t)constrain((int)midF * 2, 0, 1023);
  f.high = (uint16_t)constrain((int)highF * 3, 0, 1023);
  f.peak = (digitalRead(MIC_DO_PIN) == HIGH) ? 1 : 0;
  return f;
}

void connectToModem() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connessione a: ");
    Serial.println(SSID);
    WiFi.begin(SSID, PASS);

    unsigned long t0 = millis();
    while (millis() - t0 < 8000) {
      if (WiFi.status() == WL_CONNECTED) {
        break;
      }
      delay(300);
      Serial.print(".");
    }
    Serial.println();
  }

  Serial.println("Connesso al Modem!");
  Serial.print("IP MKR: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  pinMode(MIC_PIN, INPUT);
  pinMode(MIC_DO_PIN, INPUT);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Modulo WiFi non trovato");
    while (true) {
      delay(500);
    }
  }

  connectToModem();
  udp.begin(UDP_PORT);

  if (BATTERY_MODE) {
    WiFi.lowPowerMode();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Battery mode: ON");
  } else {
    WiFi.noLowPowerMode();
    Serial.println("Battery mode: OFF");
  }
}

void sendFeatures(const AudioFeatures &f) {
  char payload[40];
  snprintf(
    payload,
    sizeof(payload),
    "%u,%u,%u,%u,%u",
    f.overall,
    f.low,
    f.mid,
    f.high,
    f.peak
  );

  udp.beginPacket(r4Ip, UDP_PORT);
  udp.write((const uint8_t*)payload, strlen(payload));
  udp.endPacket();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToModem();
  }

  if (millis() - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = millis();

    uint16_t env = readEnvelopeP2P();
    AudioFeatures f = extractFeatures(env);
    sendFeatures(f);

    Serial.print("O:");
    Serial.print(f.overall);
    Serial.print(" L:");
    Serial.print(f.low);
    Serial.print(" M:");
    Serial.print(f.mid);
    Serial.print(" H:");
    Serial.print(f.high);
    Serial.print(" P:");
    Serial.println(f.peak);
  }
}