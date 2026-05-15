#include <FastLED.h>
const int pinAnalogico = A0; 
const int pinDigitale = 2;
int LED_PIN  = 7;
int  NUM_LEDS = 6;
#define BRIGHTNESS  50
const int trigPin = 3;
const int echoPin = 4;
const int relePin = 5;
CRGB leds[NUM_LEDS];
const int distanza = 20;
bool releStato = false;
bool oggettoVicino = false;  
const int distanzaSoglia = 20; 

void setup() {
  Serial.begin(115200);
  pinMode(pinDigitale, INPUT);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  digitalWrite(relePin, LOW); // Relè inizialmente OFF  
  FastLED.show();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(relePin, OUTPUT);
}

void loop() {
    long durata;
  float distanza; 
  int valoreAnalogico = analogRead(pinAnalogico);
  int valoreDigitale = digitalRead(pinDigitale);

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);


  durata = pulseIn(echoPin, HIGH);
  distanza = durata * 0.034 / 2;

  Serial.print("Distanza: ");
  Serial.println(distanza);


  if (distanza > 0 && distanza < distanzaSoglia) {
    if (!oggettoVicino) {
      releStato = !releStato;
      digitalWrite(relePin, releStato);
      oggettoVicino = true;
      delay(300); // Anti-rimbalzo
    }
  } else {
    oggettoVicino = false;
  }

  if (valoreDigitale == 1 && releStato){
    for (int i = 0; i < NUM_LEDS; i++) {
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Orange;
    leds[2] = CRGB::Yellow;
    leds[3] = CRGB::Green;
    leds[4] = CRGB::Blue;
    leds[5] = CRGB::Purple;
    FastLED.show();
    Serial.print("LED ");
    Serial.print(i);
    Serial.println(" ON");

  }
  }
  else {
    for (int i = 0 ; i< NUM_LEDS; i++){
      leds[i] = (0,0,0);
      FastLED.show();
      }

  }
    if (releStato == True){
    pinMode(7, high);
    }

  Serial.print("Analogico: ");
  Serial.print(valoreAnalogico);
  Serial.print(" | Digitale: ");
  Serial.println(valoreDigitale);

  delay(10);
}