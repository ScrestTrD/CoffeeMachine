/*
  RelayD5D6Test — Test 2 relay: D5 (van) va D6 (bom) tren may ca phe.
  NodeMCU D5 = GPIO14, D6 = GPIO12. HIGH = ON.
  Hanh vi: D5 ON 1s -> OFF -> D6 ON 1s -> OFF, lap lai.
  Mo Serial @115200 de xem + nghe relay dong/ngat.
*/

#include <Arduino.h>

#define RELAY_D5 D5
#define RELAY_D6 D6
const uint32_t TOGGLE_MS = 1000;

void setup() {
  pinMode(RELAY_D5, OUTPUT);
  pinMode(RELAY_D6, OUTPUT);
  digitalWrite(RELAY_D5, LOW);
  digitalWrite(RELAY_D6, LOW);
  Serial.begin(115200);
  delay(200);
  Serial.println("RelayD5D6Test: D5 ON 1s / OFF, D6 ON 1s / OFF, lap lai");
}

void pulse(int pin, const char *name) {
  digitalWrite(pin, HIGH);
  Serial.print(name);
  Serial.println(" = ON");
  delay(TOGGLE_MS);
  digitalWrite(pin, LOW);
  Serial.print(name);
  Serial.println(" = OFF");
  delay(200);   // nghi giua 2 relay
}

void loop() {
  pulse(RELAY_D5, "D5");
  pulse(RELAY_D6, "D6");
}
