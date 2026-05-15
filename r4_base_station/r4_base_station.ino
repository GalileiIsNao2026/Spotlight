#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <FastLED.h>

// --- WiFi AP del palco (R4) ---
const char AP_SSID[] = "PALCO_R4_BASE";
const char AP_PASS[] = "palco12345";
const uint16_t UDP_PORT = 4210;

// --- Pin Sensore e Relè ---
const int trigPin = 3;
const int echoPin = 4;
const int relePin = 5;
const int distanzaSoglia = 20; // Soglia in cm

// Variabile di stato per bloccare il sistema in "ACCESO"
bool sistemaAttivato = false; 

// --- LED strip ---
const uint8_t LED_PIN = 7;
const uint16_t NUM_LEDS = 3; 
const uint8_t MAX_BRIGHTNESS = 180;

CRGB leds[NUM_LEDS];
WiFiUDP udp;

// Dati audio
uint16_t remoteOverall = 0, remoteLow = 0, remoteMid = 0, remoteHigh = 0;
uint8_t remotePeak = 0;
unsigned long lastPacketMs = 0;
const unsigned long SIGNAL_TIMEOUT_MS = 1200;
uint8_t smoothedHue = 100;

void setup() {
  Serial.begin(115200);
  
  // Configurazione Pin
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, LOW); // Parte spento

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.clear(true);

  if (WiFi.beginAP(AP_SSID, AP_PASS) != WL_AP_LISTENING) {
    Serial.println("Errore AP");
    while (true);
  }

  udp.begin(UDP_PORT);
  Serial.println("Sistema Pronto. In attesa di oggetto...");
}

// Funzione per leggere la distanza dal sensore HC-SR04
long readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout 30ms
  if (duration == 0) return 999; // Nessun ostacolo
  return duration * 0.034 / 2;
}

void readUdpFeatures() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return;

  char incoming[20];
  int len = udp.read(incoming, sizeof(incoming) - 1);
  if (len < 0) return;
  incoming[len] = '\0';

  int o, l, m, h, p;
  if (sscanf(incoming, "%d,%d,%d,%d,%d", &o, &l, &m, &h, &p) == 5) {
    remoteOverall = (uint16_t)constrain(o, 0, 1023);
    remoteLow = (uint16_t)constrain(l, 0, 1023);
    remoteMid = (uint16_t)constrain(m, 0, 1023);
    remoteHigh = (uint16_t)constrain(h, 0, 1023);
    remotePeak = (uint8_t)constrain(p, 0, 1);
    lastPacketMs = millis();
  }
}

uint8_t pickHueFromBands(uint16_t low, uint16_t mid, uint16_t high) {
  uint32_t sum = (uint32_t)low + (uint32_t)mid + (uint32_t)high;
  if (sum < 6) return smoothedHue;
  uint32_t weighted = (uint32_t)low * 10 + (uint32_t)mid * 95 + (uint32_t)high * 160;
  uint8_t rawHue = (uint8_t)(weighted / sum);
  smoothedHue = smoothedHue + (uint8_t)((int)(rawHue - smoothedHue) / 3);
  return smoothedHue;
}

void showAudioOnStrip(uint16_t overall, uint16_t low, uint16_t mid, uint16_t high, uint8_t peak) {
  // Se il sistema non è attivato, i LED restano spenti
  if (!sistemaAttivato) {
    FastLED.clear(true);
    return;
  }

  uint16_t activeLeds = map(overall, 0, 1023, 0, NUM_LEDS);
  uint8_t hue = pickHueFromBands(low, mid, high);
  uint8_t bright = map(overall, 0, 1023, 15, MAX_BRIGHTNESS);
  
  if (peak) bright = MAX_BRIGHTNESS;
  if (overall > 40 && activeLeds == 0) activeLeds = 3;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < activeLeds) {
      leds[0] = CHSV(hue, 255, bright);
      leds[1] = CHSV(hue, 255, bright);
      leds[2] = CHSV(hue, 255, bright);

    } else {
      leds[0].fadeToBlackBy(80);
      leds[1].fadeToBlackBy(80);
      leds[2].fadeToBlackBy(80);
    }
  }
  FastLED.show();
}

void loop() {
  // 1. Controllo attivazione (se non ancora attivo)
  if (!sistemaAttivato) {
    long dist = readDistance();
    if (dist > 0 && dist < distanzaSoglia) {
      sistemaAttivato = true;
      digitalWrite(relePin, HIGH); // Accendi relè per sempre
      Serial.println("ATTIVATO!");
    }
  }

  // 2. Lettura dati UDP
  readUdpFeatures();

  // 3. Gestione timeout segnale
  bool signalAlive = (millis() - lastPacketMs) < SIGNAL_TIMEOUT_MS;
  uint16_t o = signalAlive ? remoteOverall : 0;
  uint16_t l = signalAlive ? remoteLow : 0;
  uint16_t m = signalAlive ? remoteMid : 0;
  uint16_t h = signalAlive ? remoteHigh : 0;
  uint8_t p = signalAlive ? remotePeak : 0;

  // 4. Mostra LED (solo se sistemaAttivato è true)
  showAudioOnStrip(o, l, m, h, p);

  delay(10);
}