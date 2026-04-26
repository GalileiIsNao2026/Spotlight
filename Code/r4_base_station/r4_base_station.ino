#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <FastLED.h>

// --- WiFi AP del palco (R4) ---
const char AP_SSID[] = "PALCO_R4_BASE";
const char AP_PASS[] = "palco12345";
const uint16_t UDP_PORT = 4210;

// --- LED strip (dal tuo file originale) ---
const uint8_t LED_PIN = 7;
const uint16_t NUM_LEDS = 3;       // Cambia in base alla tua striscia
const uint8_t MAX_BRIGHTNESS = 180; // Limite sicurezza alimentazione

CRGB leds[NUM_LEDS];
WiFiUDP udp;

// Stato audio ricevuto dal nodo MKR: overall,low,mid,high,peak
uint16_t remoteOverall = 0;
uint16_t remoteLow = 0;
uint16_t remoteMid = 0;
uint16_t remoteHigh = 0;
uint8_t remotePeak = 0;
unsigned long lastPacketMs = 0;
const unsigned long SIGNAL_TIMEOUT_MS = 1200;
uint8_t smoothedHue = 100;

void setup() {
  Serial.begin(115200);
  delay(1200);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.clear(true);

  // Avvia R4 come access point
  int status = WiFi.beginAP(AP_SSID, AP_PASS);
  if (status != WL_AP_LISTENING) {
    Serial.println("Errore: AP non avviato");
    while (true) {
      delay(500);
    }
  }

  udp.begin(UDP_PORT);

  IPAddress ip = WiFi.localIP();
  Serial.println("Base station pronta");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP R4: ");
  Serial.println(ip);
  Serial.print("UDP porta: ");
  Serial.println(UDP_PORT);
}

void readUdpFeatures() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  char incoming[20];
  int len = udp.read(incoming, sizeof(incoming) - 1);
  if (len < 0) {
    return;
  }
  incoming[len] = '\0';

  int o = 0, l = 0, m = 0, h = 0, p = 0;
  int parsed = sscanf(incoming, "%d,%d,%d,%d,%d", &o, &l, &m, &h, &p);
  if (parsed < 5) {
    return;
  }

  remoteOverall = (uint16_t)constrain(o, 0, 1023);
  remoteLow = (uint16_t)constrain(l, 0, 1023);
  remoteMid = (uint16_t)constrain(m, 0, 1023);
  remoteHigh = (uint16_t)constrain(h, 0, 1023);
  remotePeak = (uint8_t)constrain(p, 0, 1);
  lastPacketMs = millis();
}

uint8_t pickHueFromBands(uint16_t low, uint16_t mid, uint16_t high) {
  // Mix continuo delle bande: evita blocchi su 3 colori fissi.
  uint32_t sum = (uint32_t)low + (uint32_t)mid + (uint32_t)high;
  if (sum < 6) {
    return smoothedHue;
  }

  const uint8_t HUE_LOW = 10;   // rosso/arancio
  const uint8_t HUE_MID = 95;   // verde
  const uint8_t HUE_HIGH = 160; // blu

  uint32_t weighted =
    (uint32_t)low * HUE_LOW +
    (uint32_t)mid * HUE_MID +
    (uint32_t)high * HUE_HIGH;
  uint8_t rawHue = (uint8_t)(weighted / sum);

  smoothedHue = smoothedHue + (uint8_t)((int)(rawHue - smoothedHue) / 3);
  return smoothedHue;
}

void showAudioOnStrip(uint16_t overall, uint16_t low, uint16_t mid, uint16_t high, uint8_t peak) {
  uint16_t activeLeds = map(overall, 0, 1023, 0, NUM_LEDS);
  uint8_t hue = pickHueFromBands(low, mid, high);
  uint8_t bright = map(overall, 0, 1023, 15, MAX_BRIGHTNESS);
  uint32_t sum = (uint32_t)low + (uint32_t)mid + (uint32_t)high;
  uint16_t strongest = max(low, max(mid, high));
  uint8_t sat = 255;
  if (sum > 0) {
    sat = (uint8_t)constrain((int)(120 + ((strongest * 135UL) / sum)), 120, 255);
  }
  if (peak) {
    bright = MAX_BRIGHTNESS;
  }
  if (overall > 40 && activeLeds == 0) {
    activeLeds = 1;
  }

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < activeLeds) {
      leds[i] = CHSV(hue, sat, bright);
    } else {
      leds[i].fadeToBlackBy(80);
    }
  }
  FastLED.show();
}

void loop() {
  readUdpFeatures();

  // Se il microfono non invia più, spegni gradualmente
  bool signalAlive = (millis() - lastPacketMs) < SIGNAL_TIMEOUT_MS;
  uint16_t overall = signalAlive ? remoteOverall : 0;
  uint16_t low = signalAlive ? remoteLow : 0;
  uint16_t mid = signalAlive ? remoteMid : 0;
  uint16_t high = signalAlive ? remoteHigh : 0;
  uint8_t peak = signalAlive ? remotePeak : 0;

  showAudioOnStrip(overall, low, mid, high, peak);

  static unsigned long lastLog = 0;
  if (millis() - lastLog > 500) {
    lastLog = millis();
    Serial.print("O:");
    Serial.print(overall);
    Serial.print(" L:");
    Serial.print(low);
    Serial.print(" M:");
    Serial.print(mid);
    Serial.print(" H:");
    Serial.print(high);
    Serial.print(" P:");
    Serial.println(peak);
  }

  delay(10);
}
