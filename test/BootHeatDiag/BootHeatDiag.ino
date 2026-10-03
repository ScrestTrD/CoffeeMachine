/*
  BootHeatDiag — Chan doan qua nhiet luc boot.
  Giong thong so NTC cua CoffeeMachine (v21) va dieu khien D7 bang thermostat.
  Muc dich: in ra raw/R/T va trang thai D7 moi 0.5s de doi chieu voi nhiet ke
  ngoai trong lan dun dau tien sau boot.

  Serial @115200 (khong dung LED nen khong xung dot GPIO1/3).
  Lenh: '0' tat D7 (khong dung thermostat nua), '1' bat lai thermostat.

  AN TOAN: co cat cung mem 130C (tuy chi de chan doan). Van giam sat nhiet ke.
*/

#include <Arduino.h>
#include <math.h>

#define HEATER_PIN D7

const float NTC_R_SERIES = 10000.0f;
const float NTC_VCC      = 3.3f;
const float NTC_ADC_FS   = 3.3f;
const float NTC_T0_K     = 298.15f;
float NTC_R0             = 185000.0f;
float NTC_B              = 4890.0f;
const float NTC_CAL_OFFSET_C = 0.0f;
const uint8_t  NTC_SAMPLE_COUNT    = 7;
const uint16_t NTC_SAMPLE_SPACE_MS = 6;
const float NTC_ALPHA    = 0.25f;
const float NTC_MAX_STEP_C = 15.0f;

const float SETPOINT_C   = 80.0f;    // muc an toan de quan sat overshoot
const float HYST_C       = 1.0f;
const float HARD_CUT_C   = 130.0f;   // cat cung chan doan
const uint32_t PRINT_PERIOD_MS = 500;

int samples[NTC_SAMPLE_COUNT];
uint8_t sampleIdx = 0;
uint32_t lastSampleMs = 0;
uint32_t lastPrintMs = 0;
int lastMedian = 0;
float lastRes = NAN;
float rawT = NAN;
float ctrlT = 0.0f;
bool ctrlValid = false;
bool haveSeed = false;
float seedT = 0.0f;
bool heaterOn = false;
bool autoMode = true;
uint32_t bootMs = 0;

void setHeater(bool on) {
  heaterOn = on;
  digitalWrite(HEATER_PIN, on ? HIGH : LOW);
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
  digitalWrite(HEATER_PIN, LOW);
  Serial.begin(115200);
  delay(200);
  bootMs = millis();
  Serial.println("BootHeatDiag v1 — thermostat 80C, cat cung 130C");
  Serial.println("t(ms) | raw | V | R(ohm) | T_raw | T_ctrl | D7 | valid");
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
          if (samples[j] < samples[i]) { int t = samples[i]; samples[i] = samples[j]; samples[j] = t; }
      lastMedian = samples[NTC_SAMPLE_COUNT / 2];
      lastRes = adcToResistance(lastMedian);
      float tRaw = resistanceToTempC(lastRes);
      if (isfinite(tRaw)) {
        if (ctrlValid) {
          if (fabsf(tRaw - rawT) > NTC_MAX_STEP_C) { /* glitch, keep */ }
          else { rawT = tRaw; ctrlT += NTC_ALPHA * (tRaw - ctrlT); }
        } else {
          if (!haveSeed) { seedT = tRaw; haveSeed = true; }
          else if (fabsf(tRaw - seedT) > NTC_MAX_STEP_C) { seedT = tRaw; }
          else { haveSeed = false; rawT = tRaw; ctrlT = tRaw; ctrlValid = true; }
        }
      }
    }
  }

  if (autoMode && ctrlValid) {
    float ref = ctrlT;
    if (ref > HARD_CUT_C) { setHeater(false); }
    else {
      float half = HYST_C * 0.5f;
      if (heaterOn) { if (ref >= SETPOINT_C + half) setHeater(false); }
      else { if (ref < SETPOINT_C - half) setHeater(true); }
    }
  }

  if ((now - lastPrintMs) >= PRINT_PERIOD_MS) {
    lastPrintMs = now;
    float v = lastMedian * (NTC_ADC_FS / 1023.0f);
    Serial.print(now - bootMs); Serial.print(" | ");
    Serial.print(lastMedian); Serial.print(" | ");
    Serial.print(v, 3); Serial.print(" | ");
    if (isnan(lastRes)) Serial.print("---"); else Serial.print(lastRes, 0);
    Serial.print(" | ");
    if (isnan(rawT)) Serial.print("--"); else Serial.print(rawT, 1);
    Serial.print(" | ");
    if (!ctrlValid) Serial.print("--"); else Serial.print(ctrlT, 1);
    Serial.print(" | ");
    Serial.print(heaterOn ? "ON " : "OFF");
    Serial.print(" | ");
    Serial.println(ctrlValid ? "V" : "-");
  }

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '0') { autoMode = false; setHeater(false); Serial.println("MANUAL OFF"); }
    else if (c == '1') { autoMode = true; Serial.println("AUTO ON"); }
  }
}
