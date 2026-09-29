/*
  HeaterTempTest — Bat D7 gia nhiet DONG THOI doc nhiet do NTC theo thoi gian thuc.
  NodeMCU D7 = GPIO13 (tren may ca phe la chan SSR boiler).
  NTC: 3V3 -- 10k --+-- A0, NTC chan duoi xuong GND.

  Hanh vi: thermostat giu loanh quanh SETPOINT (mac dinh AUTO tu boot)
  + in nhiet moi 0.5s.
  Lenh Serial @115200: 'a' = AUTO (thermostat), '1' = ep mo D7,
  '0' = ep tat D7.

  CANH BAO: sketch test. Cat cung 145 C luon thang (ke ca manual).
  Giam sat nhiet ke, gui '0' neu thay bat thuong.
*/

#include <Arduino.h>
#include <math.h>

#define HEATER_PIN D7

// Cung thong so NTC nhu NtcTempMonitor (de cot R nhat quan khi fit)
const float NTC_R_SERIES = 10000.0f;
const float NTC_VCC      = 3.3f;
const float NTC_ADC_FS   = 3.3f;
const float NTC_T0_K     = 298.15f;
float NTC_R0             = 185000.0f;
float NTC_B              = 4890.0f;
const float NTC_CAL_OFFSET_C = 15.0f;   // v11: bu +15, doc thap deu 15 (2026-09-30 22:39)
const uint8_t  NTC_SAMPLE_COUNT    = 7;
const uint16_t NTC_SAMPLE_SPACE_MS = 6;
const float NTC_ALPHA    = 0.25f;
const uint32_t PRINT_PERIOD_MS = 500;

int samples[NTC_SAMPLE_COUNT];
uint8_t sampleIdx = 0;
uint32_t lastSampleMs = 0;
float ctrlT = 0.0f;
bool ctrlValid = false;
uint32_t lastPrintMs = 0;
int lastMedian = 0;
float lastRes = NAN;
bool heaterOn = false;

// Dieu khien nhiet do loanh quanh SETPOINT (thermostat ON/OFF)
const float CTRL_SETPOINT_C = 115.0f;
const float CTRL_HYST_C     = 1.0f;    // ON < sp-0.5, OFF >= sp+0.5
const float CTRL_HARD_CUT_C = 145.0f;  // cat cung, luon thang
bool autoMode = true;
bool thermoOn = false;

void setHeater(bool on) {
  heaterOn = on;
  digitalWrite(HEATER_PIN, on ? HIGH : LOW);
  Serial.print("HEATER D7 = ");
  Serial.println(on ? "ON" : "OFF");
}

float adcToResistance(int raw) {
  float v = raw * (NTC_ADC_FS / 1023.0f);
  if (v <= 0.02f) return NAN;
  if (v >= NTC_ADC_FS - 0.02f) return NAN;
  return NTC_R_SERIES * v / (NTC_VCC - v);
}

float resistanceToTempC(float r) {
  if (!(r > 0.0f) || r >= 1.0e9f) return NAN;
  float invT = 1.0f / NTC_T0_K + (1.0f / NTC_B) * logf(r / NTC_R0);
  return (1.0f / invT) - 273.15f + NTC_CAL_OFFSET_C;
}

void setup() {
  pinMode(HEATER_PIN, OUTPUT);
  Serial.begin(115200);
  delay(200);
  Serial.println("HeaterTempTest: thermostat 115C + doc NTC moi 0.5s.");
  Serial.println("'a'=AUTO, '1'=ep mo, '0'=ep tat. Cat cung 145C.");
  Serial.println("MODE | D7 | raw_med | V(V) | R(ohm) | T(C)");
}

void loop() {
  uint32_t now = millis();

  if ((now - lastSampleMs) >= NTC_SAMPLE_SPACE_MS) {
    samples[sampleIdx++] = analogRead(A0);
    lastSampleMs = now;
    if (sampleIdx >= NTC_SAMPLE_COUNT) {
      sampleIdx = 0;
      for (uint8_t i = 0; i < NTC_SAMPLE_COUNT; i++)
        for (uint8_t j = i + 1; j < NTC_SAMPLE_COUNT; j++)
          if (samples[j] < samples[i]) {
            int t = samples[i]; samples[i] = samples[j]; samples[j] = t;
          }
      lastMedian = samples[NTC_SAMPLE_COUNT / 2];
      lastRes = adcToResistance(lastMedian);
      float tRaw = resistanceToTempC(lastRes);
      if (isfinite(tRaw)) {
        if (!ctrlValid) { ctrlT = tRaw; ctrlValid = true; }
        else ctrlT += NTC_ALPHA * (tRaw - ctrlT);
      }
    }
  }

  // Thermostat AUTO: bam 115, chi cat cung 145 moi tat duoc
  if (autoMode && ctrlValid) {
    if (thermoOn) {
      if (ctrlT >= CTRL_SETPOINT_C + CTRL_HYST_C * 0.5f) thermoOn = false;
    } else {
      if (ctrlT < CTRL_SETPOINT_C - CTRL_HYST_C * 0.5f) thermoOn = true;
    }
  }
  bool wantOn = autoMode ? thermoOn : heaterOn;
  if (ctrlValid && ctrlT > CTRL_HARD_CUT_C) wantOn = false;  // cat cung
  if (wantOn != heaterOn) setHeater(wantOn);

  if ((now - lastPrintMs) >= PRINT_PERIOD_MS) {
    lastPrintMs = now;
    float v = lastMedian * (NTC_ADC_FS / 1023.0f);
    Serial.print(autoMode ? "AUTO|" : "MAN |");
    Serial.print(heaterOn ? "ON  | " : "OFF | ");
    Serial.print(lastMedian);
    Serial.print(" | ");
    Serial.print(v, 3);
    Serial.print(" | ");
    if (isnan(lastRes)) Serial.print("---");
    else Serial.print(lastRes, 0);
    Serial.print(" | ");
    if (!ctrlValid) Serial.print("--wait--");
    else Serial.print(ctrlT, 1);
    Serial.println();
  }

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1') { autoMode = false; setHeater(true); }
    else if (c == '0') { autoMode = false; setHeater(false); }
    else if (c == 'a' || c == 'A') {
      autoMode = true;
      Serial.println("MODE = AUTO (thermostat 115C)");
    }
  }
}
