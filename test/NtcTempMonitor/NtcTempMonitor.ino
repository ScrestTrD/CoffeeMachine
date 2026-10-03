/*
  NtcTempMonitor — Doc nhiet do NTC theo thoi gian thuc qua Serial.
  Cung thong so mach + loc nhu CoffeeMachine.ino (v4) de so doc KHOP
  voi nhiet do firmware dieu khien nhin thay.

  Mach: 3V3 -- 10k --+-- A0
                     NTC (chan duoi) xuong GND
  NodeMCU A0 full-scale 0..3.3V.

  Nap len ESP, mo Serial Monitor @115200 de xem.
  Luu y: GPIO1/GPIO3 tren may co gan LED nen LED se nhap nhay theo
  Serial — van doc duoc so lieu binh thuong.

  Lenh Serial (go + Enter):
    'p' -> in tham so dang dung (R_SERIES, B, R0)
*/

#include <Arduino.h>
#include <math.h>

// --- Thong so GIONG HET CoffeeMachine.ino (v4) ---
const float NTC_R_SERIES = 10000.0f;  // 10k keo len 3.3V
const float NTC_VCC      = 3.3f;
const float NTC_ADC_FS   = 3.3f;      // A0 NodeMCU 0..3.3V
const float NTC_T0_K     = 298.15f;   // 25 C (Kelvin)
float NTC_R0             = 185000.0f; // fit 2 diem NTC moi 2026-09-30 21:06 (32/86 C)
float NTC_B              = 4890.0f;
// Offset -18 cu da bo (v8): loi that la do doc duong cong, khong phai offset deu.
const float NTC_CAL_OFFSET_C = 0.0f;   // v16: bench uniform +15 HIGH -> bo +15 (2026-10-03)
const uint8_t  NTC_SAMPLE_COUNT   = 7;
const uint16_t NTC_SAMPLE_SPACE_MS = 6;
const float NTC_ALPHA    = 0.25f;     // loc thong thap
const uint32_t PRINT_PERIOD_MS = 500; // in moi 0.5s

int samples[NTC_SAMPLE_COUNT];
uint8_t sampleIdx = 0;
uint32_t lastSampleMs = 0;
float ctrlT = 0.0f;
bool ctrlValid = false;
uint32_t lastPrintMs = 0;
int lastMedian = 0;
float lastRes = NAN;

float adcToResistance(int raw) {
  float v = raw * (NTC_ADC_FS / 1023.0f);
  if (v <= 0.02f) return NAN;              // gan chap mach
  if (v >= NTC_ADC_FS - 0.02f) return NAN; // gan ho mach
  return NTC_R_SERIES * v / (NTC_VCC - v); // NTC chan duoi
}

float resistanceToTempC(float r) {
  if (!(r > 0.0f) || r >= 1.0e9f) return NAN;
  float invT = 1.0f / NTC_T0_K + (1.0f / NTC_B) * logf(r / NTC_R0);
  return (1.0f / invT) - 273.15f;
}

void printParams() {
  Serial.print("Params R_SERIES=");
  Serial.print(NTC_R_SERIES, 0);
  Serial.print(" B=");
  Serial.print(NTC_B, 1);
  Serial.print(" R0=");
  Serial.print(NTC_R0, 0);
  Serial.print(" CAL_OFFSET=");
  Serial.print(NTC_CAL_OFFSET_C, 1);
  Serial.println("C");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("NtcTempMonitor — giong thong so CoffeeMachine.ino v4");
  printParams();
  Serial.println("raw_med | V(V) | R(ohm) | T_raw(C) | T_ctrl(C)");
}

void loop() {
  uint32_t now = millis();

  // Gom du 7 mau cach nhau 6ms (giong firmware) roi loc
  if ((now - lastSampleMs) >= NTC_SAMPLE_SPACE_MS) {
    samples[sampleIdx++] = analogRead(A0);
    lastSampleMs = now;
    if (sampleIdx >= NTC_SAMPLE_COUNT) {
      sampleIdx = 0;
      // median chong nhieu dot bien
      for (uint8_t i = 0; i < NTC_SAMPLE_COUNT; i++)
        for (uint8_t j = i + 1; j < NTC_SAMPLE_COUNT; j++)
          if (samples[j] < samples[i]) {
            int t = samples[i]; samples[i] = samples[j]; samples[j] = t;
          }
      lastMedian = samples[NTC_SAMPLE_COUNT / 2];
      lastRes = adcToResistance(lastMedian);
      float tRaw = resistanceToTempC(lastRes) + NTC_CAL_OFFSET_C;
      if (isfinite(tRaw)) {
        if (!ctrlValid) { ctrlT = tRaw; ctrlValid = true; }
        else ctrlT += NTC_ALPHA * (tRaw - ctrlT);
      }
    }
  }

  // In moi 0.5s
  if ((now - lastPrintMs) >= PRINT_PERIOD_MS) {
    lastPrintMs = now;
    float v = lastMedian * (NTC_ADC_FS / 1023.0f);
    float tRaw = resistanceToTempC(lastRes) + NTC_CAL_OFFSET_C;
    Serial.print(lastMedian);
    Serial.print(" | ");
    Serial.print(v, 3);
    Serial.print(" | ");
    if (isnan(lastRes)) Serial.print("---");
    else Serial.print(lastRes, 0);
    Serial.print(" | ");
    if (isnan(tRaw)) Serial.print("--err--");
    else Serial.print(tRaw, 1);
    Serial.print(" | ");
    if (!ctrlValid) Serial.print("--wait--");
    else Serial.print(ctrlT, 1);
    Serial.println();
  }

  // Lenh 'p'
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'p' || c == 'P') printParams();
  }
}
