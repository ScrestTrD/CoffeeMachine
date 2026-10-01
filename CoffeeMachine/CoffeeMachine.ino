/*
  CoffeeMachine — NodeMCU ESP8266 coffee-machine controller.
  by Truong Cong Dinh 2026.09.03

  v5 (2026-09-30):
    - NTC_CAL_OFFSET_C = -18 C (readings ran uniformly hot; applied post-Beta).
    - FW_VERSION shown on display during BOOT_SAFE. A different digit is
      a version mismatch; a matching digit does not prove binary identity.
  v6 (2026-09-30):
    - ABS_OVERTEMP_C lowered 180 -> 145 C (SETPOINT_MAX auto 175 -> 140 C).
  v7 (2026-09-30):
    - Fault/state guards, recording timeout, live readiness and preamble abort.
    - Fresh NTC watchdog, unfiltered overtemp guard and checked EEPROM saves.
  v8 (2026-09-30 19:36):
    - NTC refit from 3 live points vs reference thermometer (112/138/146 C):
      R0 58000 -> 82000, Beta 3950 -> 3660, offset -18 -> 0. Residual <= 0.7 C
      in 112..146 C. The old -18 offset was fitted on a mismatched report and
      made high-temp readings worse; it is removed.
    - CFG_VERSION 3 -> 4 to force defaults, so a stored old R0/Beta can never
      survive the flash (presets must be re-recorded once).
  v9 (2026-09-30 21:06):
    - NTC refit on NEW sensor, 2 steady end points (owner: middle point noisy):
      ref 32 C -> R 127211 ohm, ref 86 C -> R 11433 ohm.
      R0 82000 -> 185000, Beta 3660 -> 4890. Exact at both ends; mid-point
      (ref 98) predicted ~90 — re-verify with steady multi-point data.
    - CFG_VERSION 4 -> 5 to force the new defaults (presets must be
      re-recorded once).
  v10 (2026-09-30 22:21):
    - Display: re-push static 595 frame every 5 ms even when unchanged
      (DISPLAY_REFRESH_MS). Heals latch corruption from relay/pump/SSR
      sag/EMI faster than the eye can see. CFG unchanged (presets kept).
  v11 (2026-09-30 22:39):
    - NTC_CAL_OFFSET_C 0 -> +15 C: v9 fit reads uniformly ~15 LOW vs reference.
      CFG unchanged (offset is a code const; stored R0/Beta stay valid).
  v14 (2026-10-01):
    - Brew heat covers the whole preset/recording cycle, including pump-off soak.
    - On cycle end/abort, recover only while filtered NTC is below setpoint.
      At/above setpoint stop immediately and clear the old thermostat request.
    - Monitor after brew: OFF at set, ON below set; return to thermostat only
      after fresh readings stay in [set, set+0.5 C] for 3 s; no timed heat boost.
    - Keep v11 NTC calibration, fault codes, 145 C cutoff and EEPROM v5.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include <ShiftRegister74HC595.h>

// Firmware version shown during BOOT_SAFE. This is not a binary hash:
// distinct builds may share a version; verify uploaded artifacts separately.
static const uint8_t FW_VERSION = 15;

// ============================================================================
//  1. PIN MAP  (COFFE_README section 2)
// ============================================================================
#define PIN_SDI    D2   // GPIO4  -> 74HC595 data (SDI)
#define PIN_SCLK   D3   // GPIO0  -> 74HC595 shift clock (SCLK)
#define PIN_LOAD   D4   // GPIO2  -> 74HC595 latch (LOAD)
#define PIN_NTC    A0   // analog input, 0..3.3 V, NTC on bottom leg
#define PIN_PUMP   D6   // GPIO12 -> pump relay,  HIGH = ON
#define PIN_VALVE  D5   // GPIO14 -> solenoid-valve relay, HIGH = ON
#define PIN_SSR    D7   // GPIO13 -> boiler SSR, HIGH = ON
#define PIN_SETB   D0   // GPIO16 -> SET button, external 10k pull-up, LOW = pressed
#define PIN_RUNB   D8   // GPIO15 -> RUN button, external 10k pull-down, HIGH = pressed
#define PIN_FLOW   D1   // GPIO5  -> flowmeter interrupt
#define PIN_LED_SET 3   // GPIO3 (RX), HIGH = ON, 1 kOhm series resistor
#define PIN_LED_RUN 1   // GPIO1 (TX), HIGH = ON, 1 kOhm series resistor

// GPIO6..GPIO11 are connected to SPI flash; MUST NOT be used.

// ============================================================================
//  2. CONFIG / TUNING CONSTANTS
// ============================================================================

// --- Temperature control (thermostat) ---------------------------------------
// ABS_OVERTEMP_C is the SSR hard-cut ceiling, NOT the desired water temperature.
// Invariants (machine-checked by the static_asserts below):
//   SETPOINT_MAX_C < ABS_OVERTEMP_C   thermostat needs headroom to converge
//   ABS_OVERTEMP_C < NTC_MAX_TEMP_C   else E1 latches before the cut fires
constexpr float ABS_OVERTEMP_C      = 145.0f;
constexpr float SETPOINT_HEADROOM_C = 5.0f;
const float DEFAULT_SETPOINT_C  = 97.5f;
const float SETPOINT_MIN_C      = 90.0f;
constexpr float SETPOINT_MAX_C      = ABS_OVERTEMP_C - SETPOINT_HEADROOM_C;  // 140.0
constexpr float SETPOINT_STEP_C = 0.5f;   // constexpr: needed by the step assert below

// On/off thermostat: the SSR is steady ON below (setpoint - 0.5 C) and steady
// OFF at/above (setpoint + 0.5 C). Total dead-band = 1 C around the setpoint.
// No PWM, no time-proportional window.
const float THERMOSTAT_HYSTERESIS_C = 1.0f;
const uint32_t RECOVERY_STABLE_MS = 3000; // observation only, never forced heat
const float RECOVERY_MAX_SWING_C = 0.10f; // entire confirmation-window range
const float RECOVERY_MAX_DROP_C = 0.05f;  // fall from peak restarts confirmation
// Software noise tolerances; validate against the physical sensor on the bench.
const float READY_BAND_C         = 2.0f;    // ready threshold below setpoint

// --- NTC measurement (COFFE_README section 4) ---
const float NTC_R_SERIES        = 10000.0f;  // 10k divider top
const float NTC_VCC             = 3.3f;      // rail
const float NTC_ADC_FS          = 3.3f;      // confirmed A0 full-scale 0..3.3V
const float NTC_T0_K            = 298.15f;   // 25 C in Kelvin
const uint8_t NTC_SAMPLE_COUNT  = 7;         // samples per filtered reading
const uint16_t NTC_SAMPLE_SPACE_MS = 6;      // spacing between raw samples
const float NTC_ALPHA           = 0.25f;     // low-pass filter coefficient
const float NTC_MIN_TEMP_C      = -40.0f;    // plausible sensor range
constexpr float NTC_MAX_TEMP_C      = 300.0f;    // must stay above ABS_OVERTEMP_C

// NTC curve fitted 2026-09-30 21:06 on the NEW sensor, 2 steady end points
// (owner judged the middle point noisy while heating):
//   ref 32 C -> R 127211 ohm | ref 86 C -> R 11433 ohm
// (R back-computed from the v8 build R0=82000/B=3660, then Beta/R0 solved
// from lnR vs 1/T). Fit: R0 = 185000, Beta = 4890. Exact at both ends;
// the dropped mid-point (ref 98) predicts ~90 — re-verify steady multi-point.
constexpr float NTC_DEFAULT_R0   = 185000.0f;
constexpr float NTC_DEFAULT_BETA = 4890.0f;

// Live single-point trim (2026-09-30 22:39): v9 fit reads uniformly ~15 C LOW
// vs the reference thermometer in the working range, so add +15 AFTER the
// Beta conversion and AFTER the plausibility check (fault range still guards
// the raw sensor). Re-fit R0/Beta properly when steady multi-point data
// shows the error is not uniform.
const float NTC_CAL_OFFSET_C = 15.0f;

// Compile-time invariants for the boiler protection chain.
static_assert(SETPOINT_MAX_C < ABS_OVERTEMP_C,
              "setpoint must leave headroom below the SSR cut");
static_assert(ABS_OVERTEMP_C < NTC_MAX_TEMP_C,
              "cutoff must fire before the NTC plausible-range fault");
const uint32_t NTC_TIMEOUT_MS   = 2000;      // no valid sample -> fault

// --- Flowmeter ---
// Still initialised at boot (hardware is present), but the dose no longer
// depends on it: extraction runs on seconds, not on pulse counts.
const uint8_t FLOW_EDGE          = FALLING;  // confirmed by Flowmeter_Test (reads correctly)

// --- Startup / UI ---
const uint32_t PRIME_MS          = 5000;     // five-second prime, valve CLOSED
// Dose preamble, timed from the RUN press (see tickRunPredelay). Order matters:
// the puck is wetted first, then left to absorb water, then the line is
// pressurised, and only then does the extraction start.
//   phase 1  valve OPEN + pump ON    DOSE_PREWET_MS   (bloom / pre-wet)
//   phase 2  valve CLOSED, pump OFF  DOSE_SOAK_MS     (coffee absorbs water)
//   phase 3  pump ON, valve CLOSED   DOSE_PRESS_MS    (build pressure)
//   phase 4  valve OPENS -> flow phase (RUN_ACTIVE)
const uint32_t DOSE_PREWET_MS    = 2000;
const uint32_t DOSE_SOAK_MS      = 2000;
const uint32_t DOSE_PRESS_MS     = 2000;
const uint32_t RECORD_HOLD_SET_MS = 3000;    // hold SET 3s from idle -> record

// A single SHORT SET press from idle starts the flush/clean run. Bounded well
// below RECORD_HOLD_SET_MS so an accidental longer press does nothing at all:
// a stray press must not be able to start water flowing.
const uint32_t CLEAN_PRESS_MAX_MS = 700;

// Clean flush STOP gesture: a SET press-and-release whose held duration is in
// [FLUSH_STOP_MIN_MS, FLUSH_STOP_MAX_MS). A shorter tap is a debounce artifact;
// a longer press is not a stop.
const uint32_t FLUSH_STOP_MIN_MS = 100;
const uint32_t FLUSH_STOP_MAX_MS = 3000;

const uint32_t MAX_DOSE_TIME_MS  = 60000;    // extraction/recording/clean limit

// --- Local setpoint edit (device buttons, no phone needed) ---
const uint32_t SP_EDIT_HOLD_MS    = 5000;    // hold SET+RUN 5s then RELEASE
const uint32_t SP_EDIT_SAVE_MS    = 300;     // both buttons held -> save
const uint32_t SP_EDIT_TIMEOUT_MS = 20000;   // no keypress -> leave, discard
constexpr float SP_EDIT_STEP_C    = 1.0f;    // RUN +1 C, SET -1 C
// The edit step must be a whole number of setpoint steps, otherwise the value
// shown while editing would not be the value clampSetpoint() actually stores.
constexpr int SP_EDIT_STEP_TICKS = (int)(SP_EDIT_STEP_C / SETPOINT_STEP_C + 0.5f);
static_assert(SP_EDIT_STEP_TICKS * SETPOINT_STEP_C == SP_EDIT_STEP_C,
              "edit step must be a whole number of setpoint steps");
const uint32_t ABORT_HOLD_RUN_MS = 2000;     // hold RUN 2s -> abort during dose

// --- Dose presets ---
// Two slots. Shrinking this changes sizeof(PersistentConfig), so an EEPROM
// written by an older build fails the version/payloadSize check and is replaced
// by defaults() -- presets recorded under the old layout are lost.
constexpr uint8_t PRESET_COUNT = 2;

// --- Misc / UI blink ---
const uint8_t  SEGMENT_CODE[10] = {   // segment codes 0..9 (bits: A..G, DP)
  0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F
};
const uint8_t SEG_E = 0x79;  // fault prefix letter 'E'
const uint8_t SEG_N = 0x54;  // lowercase 'n' (segments c,e,g) -> "no preset"
const uint8_t SEG_O = 0x5C;  // lowercase 'o' (segments c,d,e,g)
const uint8_t SEG_OFF = 0x00;

// Display polarity. The verified hardware test (4X 7 Segment.txt) inverts the
// segment bytes (COMMON_CATHODE=1 -> ~vals) and shows correctly on this module,
// i.e. the module is driven active-LOW (common-anode style). We follow the
// verified test. Set to 0 ONLY if the module is truly common-cathode
// (segments active-HIGH, no inversion).
#define DISPLAY_INVERT_SEGMENTS 1

// Static frame is re-pushed every few ms even when unchanged: the 595 chain
// shares rails with relay/pump/SSR wiring and a sag/EMI hit can corrupt a
// latch — rewriting heals it faster than the eye can see. One push is
// 4x shiftOut (~0.3 ms), so 5 ms costs a few percent CPU.
const uint32_t DISPLAY_REFRESH_MS = 5;

// Cascaded 74HC595 chain, exactly as in the hardware test. The constructor
// handles pinMode of SDI/SCLK/LOAD and resets all outputs LOW at startup.
ShiftRegister74HC595<4> sr(PIN_SDI, PIN_SCLK, PIN_LOAD);

// Debounce
const uint32_t BTN_DEBOUNCE_MS = 25;

// ============================================================================
//  3. FAULT CODES  (COFFE_README section 11)
// ============================================================================
enum FaultCode : uint8_t {
  FAULT_NONE          = 0,
  FAULT_NTC_INVALID   = 1,   // ADC near open/short, NaN/inf, out of range
  FAULT_NTC_TIMEOUT   = 3,   // no valid sample within timeout
  FAULT_CRC           = 6,   // EEPROM begin/commit failure
  FAULT_STATE         = 8,   // unknown FSM state
  // NOTE: absolute over-temp is NOT a fault here — the SSR just cuts at
  // ABS_OVERTEMP_C (heatingPermission) and resumes automatically, no error.
};

// ============================================================================
//  4. DISPLAY  (3-wire 74HC595, 4 cascaded chips, one digit each)
//  Static drive, no multiplex scan. Frame pushes on change plus a periodic
//  refresh (DISPLAY_REFRESH_MS) to heal EMI/sag latch corruption.
// ============================================================================
class Display {
  public:
    void begin() {
      // SDI/SCLK/LOAD pinMode + reset-to-LOW are handled by the library's
      // constructor on the global 'sr' instance. Just clear the frame.
      blank();
      commit();
    }

    void blank() {
      for (uint8_t i = 0; i < 4; i++) frame[i] = SEG_OFF;
      commit();
    }

    void allSegments() {   // startup test: 8888
      for (uint8_t i = 0; i < 4; i++) frame[i] = 0x7F;  // segments A..G, no DP
      commit();
    }

    // A raw 4-digit value right-aligned; leadingZero controls blanking of leading 0.
    // dpMask bit i (i=0..3) lights the decimal point on digit i (0=leftmost).
    void number(uint16_t value, bool leadingZero, uint8_t dpMask = 0) {
      uint8_t d[4];
      d[0] = SEGMENT_CODE[(value / 1000) % 10];
      d[1] = SEGMENT_CODE[(value / 100) % 10];
      d[2] = SEGMENT_CODE[(value / 10) % 10];
      d[3] = SEGMENT_CODE[value % 10];
      if (!leadingZero) {
        if (value < 1000) d[0] = SEG_OFF;
        if (value < 100)  d[1] = SEG_OFF;
        if (value < 10)   d[2] = SEG_OFF;
      }
      for (uint8_t i = 0; i < 4; i++) if (dpMask & (0x01 << i)) d[i] |= 0x80;
      for (uint8_t i = 0; i < 4; i++) frame[i] = d[i];
      commit();
    }

    // Preset 1..9, no leading zeros, right-aligned.
    void preset(uint8_t n) {
      number(n, false);
    }

    // Elapsed seconds, saturated at 9999.
    void seconds(uint32_t s) {
      if (s > 9999) s = 9999;
      number((uint16_t)s, true);
    }

    // Temperature like "97.5" -> digits [9][7][5] with decimal point on the
    // digit immediately left of the ones digit (bit 2 of dpMask).
    void temperature(float tc) {
      // handle negative or out-of-range defensively
      int32_t t = lroundf(tc * 10.0f);
      if (t < 0) {
        number(0, true);
        return;
      }
      if (t > 9999) t = 9999;
      number((uint16_t)t, false, 0x04);   // DP on the 'tens' digit position -> X.X
    }

    // Fault "E<code>".
    void error(uint8_t code) {
      frame[0] = SEG_E;
      frame[1] = SEG_OFF;
      frame[2] = SEG_OFF;
      frame[3] = (code <= 9) ? SEGMENT_CODE[code] : SEGMENT_CODE[0];
      commit();
    }

    // "no" (right-aligned) shown when RUN is pressed but no preset is recorded.
    void noPreset() {
      frame[0] = SEG_OFF;
      frame[1] = SEG_OFF;
      frame[2] = SEG_N;
      frame[3] = SEG_O;
      commit();
    }

  private:
    uint8_t frame[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t last[4]  = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t lastPushMs = 0;

    // Push the frame immediately when it changed, and re-push every
    // DISPLAY_REFRESH_MS even when unchanged (heals 595 latch corruption).
    void commit() {
      uint32_t now = millis();
      bool same = true;
      for (uint8_t i = 0; i < 4; i++) if (frame[i] != last[i]) {
          same = false;
          break;
        }
      if (same && (now - lastPushMs) < DISPLAY_REFRESH_MS) return;
      #if DISPLAY_INVERT_SEGMENTS
      uint8_t out[4];
      for (uint8_t i = 0; i < 4; i++) out[i] = ~frame[i] & 0xFF;
      sr.setAll(out);
      #else
      sr.setAll(frame);   // active-HIGH (common-cathode), no inversion
      #endif
      for (uint8_t i = 0; i < 4; i++) last[i] = frame[i];
      lastPushMs = now;
    }
};

// ============================================================================
//  5. BUTTONS  (debounce + pressed/released/held events)
// ============================================================================
class Button {
  public:
    enum Event : uint8_t { EV_NONE = 0, EV_PRESSED, EV_RELEASED, EV_HELD };

    void begin(uint8_t pin, bool activeLow, uint32_t holdMs) {
      this->pin = pin; this->activeLow = activeLow; this->holdMs = holdMs;
      pinMode(pin, INPUT);
      lastStable = readRaw();
      lastChangeMs = 0;
      stable = lastStable;
      heldFired = false;
      qHead = qTail = 0;
    }

    // Call frequently from loop(). Non-blocking.
    void poll() {
      uint32_t now = millis();
      bool raw = readRaw();
      if (raw != lastStable) {
        lastStable = raw;
        lastChangeMs = now;
      }
      if ((now - lastChangeMs) >= BTN_DEBOUNCE_MS) {
        if (raw != stable) {
          stable = raw;
          if (stable == pressedLevel()) enqueue(EV_PRESSED);
          else {
            enqueue(EV_RELEASED);
            heldFired = false;
          }
        }
        if (stable == pressedLevel() && !heldFired && (now - lastChangeMs) >= holdMs) {
          enqueue(EV_HELD);
          heldFired = true;
        }
      }
    }

    bool pressed() const {
      return stable == pressedLevel();
    }
    bool held() const {
      return heldFired && pressed();
    }
    Event popEvent() {
      if (qHead == qTail) return EV_NONE;
      Event e = queue[qHead];
      qHead = (qHead + 1) % BTN_QUEUE_LEN;
      return e;
    }

  private:
    static const uint8_t BTN_QUEUE_LEN = 6;
    uint8_t pin; bool activeLow; uint32_t holdMs;
    bool lastStable, stable, heldFired;
    uint32_t lastChangeMs;
    Event queue[BTN_QUEUE_LEN];
    uint8_t qHead, qTail;

    bool readRaw() {
      return digitalRead(pin) ? true : false;
    }
    bool pressedLevel() const {
      return !activeLow;  // activeLow: LOW=false; activeHigh: HIGH=true
    }
    void enqueue(Event e) {
      uint8_t next = (qTail + 1) % BTN_QUEUE_LEN;
      if (next == qHead) return; // full, drop newest
      queue[qTail] = e; qTail = next;
    }
};

// ============================================================================
//  6. NTC SENSOR  (median filter + low-pass; keeps raw for fault detection)
// ============================================================================
class NtcSensor {
  public:
    void begin() {
      pinMode(PIN_NTC, INPUT);
      lastCtrlT = 0.0f;
      lastRawT = NAN;
      ctrlValid = false;
      sampleIdx = 0;
      lastSampleMs = 0;
      lastValidMs = 0;
      newPublish = false;
      fault = FAULT_NONE;
    }

    void setParams(float r0, float beta) {
      r0_ = r0;
      beta_ = beta;
    }

    // Non-blocking: called every loop; collects NTC_SAMPLE_COUNT samples spaced
    // NTC_SAMPLE_SPACE_MS apart, then filters and publishes a new control temp.
    // Windows auto-restart, so readings arrive continuously without an external
    // re-arm. fresh() reports whether a window published since it was last cleared.
    void sample() {
      uint32_t now = millis();
      if ((now - lastSampleMs) < NTC_SAMPLE_SPACE_MS) return;
      samples[sampleIdx++] = analogRead(PIN_NTC);
      lastSampleMs = now;
      if (sampleIdx >= NTC_SAMPLE_COUNT) {
        sampleIdx = 0;
        publishMedian(samples, NTC_SAMPLE_COUNT);
      }
    }

    // True only when a measurement window has published since clearFresh().
    bool fresh() const {
      return newPublish;
    }
    void clearFresh() {
      newPublish = false;
    }

    float controlTemp() const {
      return lastCtrlT;  // filtered control temperature
    }
    float rawTemp() const {
      return lastRawT;  // unfiltered, for fault checking
    }
    float resistanceOhm() const {
      return lastRes;  // last measured NTC resistance
    }
    bool valid() const {
      return ctrlValid;
    }
    FaultCode faultCode() const {
      return fault;
    }
    uint32_t measurementMs() const { return lastValidMs; }
    bool freshEnough(uint32_t now) const {
      return ctrlValid && (uint32_t)(now - lastValidMs) < NTC_TIMEOUT_MS;
    }

    // Compute median of a raw ADC buffer, convert, apply low-pass. Publishes.
    void publishMedian(int *buf, uint8_t n) {
      // Median rejects outliers.
      for (uint8_t i = 0; i < n; i++)       // simple insertion sort on copy
        for (uint8_t j = i + 1; j < n; j++)
          if (buf[j] < buf[i]) {
            int t = buf[i];
            buf[i] = buf[j];
            buf[j] = t;
          }
      int median = buf[n / 2];

      float r = adcToResistance(median);
      lastRes = r;
      float tRaw = (r > 0.0f && r < 1.0e9f) ? resistanceToTempC(r) : NAN;

      if (!isfinite(tRaw) || tRaw < NTC_MIN_TEMP_C || tRaw > NTC_MAX_TEMP_C) {
        fault = FAULT_NTC_INVALID;
        ctrlValid = false;   // stale control data must not remain valid
        newPublish = true;
        return;
      }
      fault = FAULT_NONE;
      float tCal = tRaw + NTC_CAL_OFFSET_C;   // v11 calibration: +15 C
      lastRawT = tCal;
      lastValidMs = millis();
      if (!ctrlValid) {
        lastCtrlT = tCal;
        ctrlValid = true;
      } else {
        lastCtrlT += NTC_ALPHA * (tCal - lastCtrlT);
      }
      newPublish = true;
    }

  private:
    float r0_ = NTC_DEFAULT_R0, beta_ = NTC_DEFAULT_BETA;
    float lastCtrlT = 0.0f, lastRawT = NAN, lastRes = NAN;
    bool ctrlValid = false;
    uint8_t sampleIdx = 0;
    uint32_t lastSampleMs = 0;
    uint32_t lastValidMs = 0;
    bool newPublish = false;
    FaultCode fault = FAULT_NONE;
    int samples[NTC_SAMPLE_COUNT];

    float adcToResistance(int raw) {
      float v = raw * (NTC_ADC_FS / 1023.0f);
      if (v <= 0.02f) return NAN;            // near short-circuit
      if (v >= NTC_ADC_FS - 0.02f) return NAN; // near open-circuit
      return NTC_R_SERIES * v / (NTC_VCC - v); // NTC on bottom leg
    }
    float resistanceToTempC(float r) {
      float invT = 1.0f / NTC_T0_K + (1.0f / beta_) * logf(r / r0_);
      return (1.0f / invT) - 273.15f;
    }
};

// ============================================================================
//  7. FLOW SENSOR  (volatile counter + IRAM ISR)
//  Initialised at boot; the dose no longer depends on it. The pulse counter is
//  written by the ISR for diagnostics only — nothing in the FSM reads it.
// ============================================================================
class FlowSensor {
  public:
    void begin(int pin, int edge) {
      FlowSensor::instance = this;
      pinMode(pin, INPUT_PULLUP);   // confirmed by Flowmeter_Test (reads correctly)
      attachInterrupt(digitalPinToInterrupt(pin), flowIsr, edge);
    }

  private:
    static FlowSensor *instance;
    static volatile uint32_t count;

    #if defined(IRAM_ATTR)
    static void IRAM_ATTR flowIsr() {
      if (instance) count++;
    }
    #elif defined(ICACHE_RAM_ATTR)
    static void ICACHE_RAM_ATTR flowIsr() {
      if (instance) count++;
    }
    #else
    static void flowIsr() {
      if (instance) count++;
    }
    #endif

};
FlowSensor *FlowSensor::instance = nullptr;
volatile uint32_t FlowSensor::count = 0;

// ============================================================================
//  8. PERSISTENCE  (EEPROM, magic/version/CRC validation, 2 presets of seconds)
// ============================================================================
struct DosePreset {
  uint32_t seconds;   // extraction seconds (timer-based dose)
  uint8_t  valid;
  uint8_t  reserved[3];
};

struct PersistentConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t payloadSize;
  float    temperatureSetpointC;
  float    ntcR0;
  float    ntcBeta;
  uint8_t  lastPreset;
  uint8_t  reserved[3];
  DosePreset presets[PRESET_COUNT];
  uint32_t crc32;
};

static const uint32_t CFG_MAGIC = 0x434F4545UL;   // "COEE"
static const uint16_t CFG_VERSION = 5;   // 5: NTC refit R0 185000/Beta 4890 new sensor (v9); force defaults so old stored R0/Beta cannot survive
static const size_t CFG_OFFSET = 0;

class Persistence {
  public:
    bool begin() {
      // ESP8266 EEPROM.begin() returns void; verify observable buffer setup.
      // The core does not expose its flashRead status through this API.
      EEPROM.begin(sizeof(PersistentConfig));
      if (EEPROM.length() < sizeof(PersistentConfig) ||
          EEPROM.getConstDataPtr() == nullptr) {
        defaults();
        return false;
      }
      load();
      return true;
    }

    void load() {
      EEPROM.get(CFG_OFFSET, cfg);
      if (!isValid()) {
        defaults();
        return;
      }
      // Preserve valid v5 calibration/presets, normalize only the setpoint.
      cfg.temperatureSetpointC = clampSetpoint(cfg.temperatureSetpointC);
      cfg.crc32 = calcCrc();   // normalization changes only the RAM record
    }

    bool isValid() const {
      if (cfg.magic != CFG_MAGIC) return false;
      if (cfg.version != CFG_VERSION) return false;
      if (cfg.payloadSize != sizeof(PersistentConfig) - sizeof(uint32_t)) return false; // exclude crc
      if (calcCrc() != cfg.crc32) return false;
      return valuesValid();
    }

    bool save() {
      if (!valuesValid()) return false;
      cfg.magic = CFG_MAGIC;
      cfg.version = CFG_VERSION;
      cfg.payloadSize = sizeof(PersistentConfig) - sizeof(uint32_t);
      cfg.crc32 = calcCrc();
      EEPROM.put(CFG_OFFSET, cfg);
      return EEPROM.commit();
    }

    void defaults() {
      cfg.magic = CFG_MAGIC;
      cfg.version = CFG_VERSION;
      cfg.payloadSize = sizeof(PersistentConfig) - sizeof(uint32_t);
      cfg.temperatureSetpointC = DEFAULT_SETPOINT_C;
      cfg.ntcR0 = NTC_DEFAULT_R0;
      cfg.ntcBeta = NTC_DEFAULT_BETA;
      cfg.lastPreset = 0;
      memset(cfg.reserved, 0, sizeof(cfg.reserved));
      for (uint8_t i = 0; i < PRESET_COUNT; i++) {
        cfg.presets[i].seconds = 0;
        cfg.presets[i].valid = 0;
        memset(cfg.presets[i].reserved, 0, sizeof(cfg.presets[i].reserved));
      }
      cfg.crc32 = calcCrc();
    }

    // --- accessors ---
    float& setpointC() {
      return cfg.temperatureSetpointC;
    }
    float ntcR0() const {
      return cfg.ntcR0;
    }
    float ntcBeta() const {
      return cfg.ntcBeta;
    }
    uint8_t lastPreset() const {
      return cfg.lastPreset;
    }
    void setLastPreset(uint8_t n) {
      cfg.lastPreset = n;
    }

    bool presetValid(uint8_t n) const {   // n is 1..PRESET_COUNT
      if (n < 1 || n > PRESET_COUNT) return false;
      return cfg.presets[n - 1].valid == 1 &&
             cfg.presets[n - 1].seconds > 0 &&
             cfg.presets[n - 1].seconds <= MAX_DOSE_TIME_MS / 1000;
    }
    uint32_t presetSeconds(uint8_t n) const {
      if (!presetValid(n)) return 0;
      return cfg.presets[n - 1].seconds;
    }

    void setPreset(uint8_t n, uint32_t seconds) {
      if (n < 1 || n > PRESET_COUNT) return;
      // A zero / invalid recording must NOT overwrite a valid preset (spec 8.3).
      if (seconds == 0 || seconds > MAX_DOSE_TIME_MS / 1000) return;
      cfg.presets[n - 1].seconds = seconds;
      cfg.presets[n - 1].valid = 1;
      memset(cfg.presets[n - 1].reserved, 0, sizeof(cfg.presets[n - 1].reserved));
    }

    // Clamp into [SETPOINT_MIN_C, SETPOINT_MAX_C], snap to SETPOINT_STEP_C.
    float clampSetpoint(float c) const {
      if (!isfinite(c)) return DEFAULT_SETPOINT_C;
      if (c < SETPOINT_MIN_C) c = SETPOINT_MIN_C;
      if (c > SETPOINT_MAX_C) c = SETPOINT_MAX_C;
      const float inv = 1.0f / SETPOINT_STEP_C;
      float s = roundf(c * inv) / inv;              // snap
      if (s > SETPOINT_MAX_C) s = SETPOINT_MAX_C;   // snap can overshoot < 1 step
      if (s < SETPOINT_MIN_C) s = SETPOINT_MIN_C;
      return s;
    }

    const PersistentConfig& cfgRef() const {
      return cfg;
    }
    void forceInvalid() {
      cfg.magic = 0;
    }

  private:
    PersistentConfig cfg;

    bool valuesValid() const {
      if (!isfinite(cfg.temperatureSetpointC) ||
          cfg.temperatureSetpointC < SETPOINT_MIN_C ||
          cfg.temperatureSetpointC > SETPOINT_MAX_C) return false;
      // These are input bounds, not a certificate of physical calibration.
      if (!isfinite(cfg.ntcR0) || cfg.ntcR0 < 1000.0f || cfg.ntcR0 > 1000000.0f ||
          !isfinite(cfg.ntcBeta) || cfg.ntcBeta < 800.0f || cfg.ntcBeta > 6000.0f)
        return false;
      if (cfg.lastPreset > PRESET_COUNT) return false;
      for (uint8_t i = 0; i < PRESET_COUNT; i++) {
        const DosePreset& p = cfg.presets[i];
        if (p.valid > 1) return false;
        if (p.valid == 1 && (p.seconds == 0 || p.seconds > MAX_DOSE_TIME_MS / 1000))
          return false;
      }
      return true;
    }

    uint32_t calcCrc() const {
      const uint8_t *p = reinterpret_cast<const uint8_t*>(&cfg);
      uint32_t len = sizeof(PersistentConfig) - sizeof(uint32_t);
      return crc32(p, len);
    }
    static uint32_t crc32(const uint8_t *data, uint32_t len) {
      uint32_t crc = 0xFFFFFFFFUL;
      for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
          crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
      }
      return ~crc;
    }
    static void memset(void *p, int v, size_t n) {
      for (size_t i = 0; i < n; i++) ((uint8_t * )p)[i] = (uint8_t)v;
    }
};

// ============================================================================
//  9. THERMOSTAT  (on/off with 1 C hysteresis; SSR steady ON, no PWM)
// ============================================================================
class Thermostat {
  public:
    void init(float sp) {
      setpoint = sp;
      heaterOn_ = false;
    }
    void setSetpoint(float sp) {
      setpoint = sp;
    }
    float getSetpoint() const {
      return setpoint;
    }

    // Update the relay state from the measured (validated) temperature.
    // ON  while temp <  setpoint - 0.5 C
    // OFF while temp >= setpoint + 0.5 C   (total dead-band = 1 C)
    void update(float tempC) {
      const float halfHyst = THERMOSTAT_HYSTERESIS_C * 0.5f;
      if (heaterOn_) {
        if (tempC >= setpoint + halfHyst) heaterOn_ = false;
      } else {
        if (tempC < setpoint - halfHyst) heaterOn_ = true;
      }
    }

    bool heaterOn() const {
      return heaterOn_;
    }

    // Recovery ends at setpoint, below the normal OFF threshold (+0.5 C).
    // Clear the old latch so normal hysteresis cannot turn heat back on there.
    void clearHeatRequest() {
      heaterOn_ = false;
    }

  private:
    float setpoint;
    bool heaterOn_;
};

// ============================================================================
//  11. MAIN FSM
// ============================================================================
enum FsmState : uint8_t {
  BOOT_SAFE,
  STARTUP_PRIME,
  HEATING_IDLE,
  READY_IDLE,
  SETPOINT_EDIT,
  CLEAN_FLUSH,
  PRESET_SELECT_RUN,
  PRESET_SELECT_RECORD,
  PRESET_RECORD_READY,
  PRESET_RECORD_ACTIVE,
  RUN_PUMP_PREDELAY,
  RUN_ACTIVE,
  FAULT_LATCHED
};

class CoffeeMachine {
  public:
    void begin() {
      // Actuators default OFF (fail-closed, spec §12).
      pinMode(PIN_PUMP, OUTPUT);  digitalWrite(PIN_PUMP, LOW);
      pinMode(PIN_VALVE, OUTPUT); digitalWrite(PIN_VALVE, LOW);
      pinMode(PIN_SSR, OUTPUT);   digitalWrite(PIN_SSR, LOW);
      pinMode(PIN_LED_SET, OUTPUT); digitalWrite(PIN_LED_SET, LOW);
      pinMode(PIN_LED_RUN, OUTPUT); digitalWrite(PIN_LED_RUN, LOW);

      disp.begin();
      disp.preset(FW_VERSION);   // BOOT_SAFE shows current source version
      setBtn.begin(PIN_SETB, true, RECORD_HOLD_SET_MS);   // active LOW
      runBtn.begin(PIN_RUNB, false, ABORT_HOLD_RUN_MS);   // active HIGH
      flow.begin(PIN_FLOW, FLOW_EDGE);
      bool storageReady = pers.begin();
      ntc.begin();
      ntc.setParams(pers.ntcR0(), pers.ntcBeta());

      thermostat.init(pers.setpointC());

      state = BOOT_SAFE;
      fault = FAULT_NONE;
      pumpOn_ = false;
      recoveringHeat_ = false;
      recoveryStable_ = false;
      stateEnteredMs = millis();
      startupSensorsChecked = false;
      if (!storageReady) latchFault(FAULT_CRC);
    }

    // Main non-blocking tick. Must be called frequently from loop().
    void tick() {
      setBtn.poll();
      runBtn.poll();
      ntc.sample();
      // Capture after sample(): a newly published timestamp must not be
      // compared against an older snapshot (unsigned age would underflow).
      uint32_t now = millis();
      ntcTick(now);

      // Cut an unsafe heater before any FSM work (including EEPROM writes).
      // Normal heat demand is applied AFTER transitions, avoiding stale pump/
      // thermostat writes at the start or end of a brew cycle.
      if (!heatingPermission()) setActuatorSSR(false);

      switch (state) {
        case BOOT_SAFE:            tickBootSafe(now); break;
        case STARTUP_PRIME:        tickPrime(now); break;
        case HEATING_IDLE:         tickHeatingIdle(now); break;
        case READY_IDLE:           tickReadyIdle(now); break;
        case SETPOINT_EDIT:        tickSetpointEdit(now); break;
        case CLEAN_FLUSH:          tickCleanFlush(now); break;
        case PRESET_SELECT_RUN:    tickPresetSelectRun(now); break;
        case PRESET_SELECT_RECORD: tickPresetSelectRecord(now); break;
        case PRESET_RECORD_READY:  tickPresetRecordReady(now); break;
        case PRESET_RECORD_ACTIVE: tickPresetRecordActive(now); break;
        case RUN_PUMP_PREDELAY:    tickRunPredelay(now); break;
        case RUN_ACTIVE:           tickRunActive(now); break;
        case FAULT_LATCHED:        tickFaultLatched(now); break;
        default:                   failClosed(); break;
      }
      applyHeating();
      applyLeds(now);
    }

  private:
    Display disp;
    Button setBtn, runBtn;
    NtcSensor ntc;
    FlowSensor flow;
    Persistence pers;
    Thermostat thermostat;

    FsmState state;
    FaultCode fault;
    uint32_t stateEnteredMs;
    bool startupSensorsChecked;

    // --- state-local data ---
    uint8_t selectRunIdx, selectRecordIdx;
    uint32_t doseStartMs, recordStartMs;
    uint32_t doseValveOpenMs = 0;   // 0 = dose flow phase not started
    uint32_t recValveOpenMs = 0;
    bool recFlowStarted = false;   // timestamp zero is valid at millis rollover
    uint32_t cleanStartMs = 0;      // flush/clean run start
    uint32_t flushPressStartMs = 0; // SET press-start during clean flush (stop gesture)
    uint8_t currentPreset;
    bool pumpOn_ = false;   // prime retains the v11 pump-forced heating request
    bool recoveringHeat_ = false;
    bool recoveryStable_ = false;
    uint32_t recoveryStableSince_ = 0, recoverySampleMs_ = 0;
    float recoverySetpoint_ = DEFAULT_SETPOINT_C;
    float recoveryMinT_ = 0.0f, recoveryMaxT_ = 0.0f;

    static bool brewingState(FsmState s) {
      return s == RUN_PUMP_PREDELAY || s == RUN_ACTIVE ||
             s == PRESET_RECORD_ACTIVE || s == CLEAN_FLUSH;
    }

    void applyHeating() {
      if (!heatingPermission()) {
        recoveryStable_ = false;
        setActuatorSSR(false);
        return;
      }
      if (brewingState(state)) {
        // The complete cycle requests heat, including the pump-off soak.
        setActuatorSSR(true);
        return;
      }
      if (recoveringHeat_) {
        const float sp = thermostat.getSetpoint();
        const float temp = ntc.controlTemp();
        // A hot end stops heat, not monitoring. A later dip reheats at set.
        thermostat.clearHeatRequest();
        setActuatorSSR(temp < sp);
        if (sp != recoverySetpoint_) {
          recoveryStable_ = false;
          recoverySetpoint_ = sp;
          recoverySampleMs_ = ntc.measurementMs();
        }
        if (temp < sp || temp > sp + THERMOSTAT_HYSTERESIS_C * 0.5f) {
          recoveryStable_ = false;
        }
        const uint32_t sampleMs = ntc.measurementMs();
        if (sampleMs != recoverySampleMs_) {
          // A gap cannot count as observed stability, even if a new sample
          // arrives before the watchdog sees the old one expire.
          if (uint32_t(sampleMs - recoverySampleMs_) >= NTC_TIMEOUT_MS)
            recoveryStable_ = false;
          recoverySampleMs_ = sampleMs;
          if (temp >= sp && temp <= sp + THERMOSTAT_HYSTERESIS_C * 0.5f) {
            if (recoveryStable_) {
              if (temp < recoveryMinT_) recoveryMinT_ = temp;
              if (temp > recoveryMaxT_) recoveryMaxT_ = temp;
              if (recoveryMaxT_ - recoveryMinT_ > RECOVERY_MAX_SWING_C ||
                  recoveryMaxT_ - temp > RECOVERY_MAX_DROP_C)
                recoveryStable_ = false;
            }
            if (!recoveryStable_) {
              recoveryStable_ = true;
              recoveryStableSince_ = sampleMs;
              recoveryMinT_ = recoveryMaxT_ = temp;
            } else if (uint32_t(sampleMs - recoveryStableSince_) >= RECOVERY_STABLE_MS) {
              recoveringHeat_ = false;
              recoveryStable_ = false;
            }
          }
        }
        return;
      }
      thermostat.update(ntc.controlTemp());
      setActuatorSSR(thermostat.heaterOn() || pumpOn_);
    }

    // ---------- helpers ----------
    bool elapsed(uint32_t start, uint32_t ms) {
      return (millis() - start) >= ms;
    }
    void enterState(FsmState s) {
      if (s == FAULT_LATCHED || brewingState(s)) {
        recoveringHeat_ = false;
        recoveryStable_ = false;
      } else if (brewingState(state)) {
        recoveringHeat_ = true;
        recoveryStable_ = false;
        recoverySampleMs_ = ntc.measurementMs();
        recoverySetpoint_ = thermostat.getSetpoint();
      }
      state = s;
      stateEnteredMs = millis();
    }
    uint8_t firstValidPreset() const {
      for (uint8_t n = 1; n <= PRESET_COUNT; n++) if (pers.presetValid(n)) return n;
      return 0;
    }

    // Enter an idle state after flushing stale button events so a leftover press
    // from a state that did not consume its buttons (e.g. RUN_ACTIVE) cannot
    // trigger a spurious transition on the next idle tick.
    void enterIdle(FsmState s) {
      while (setBtn.popEvent() != Button::EV_NONE) {}
      while (runBtn.popEvent() != Button::EV_NONE) {}
      recHoldStart = 0;
      editHoldStart = 0;
      // A SET still held here is the tail of the press that just left the
      // previous state; ignore it until released (prevents a stop/save press
      // from starting a spurious flush back in idle).
      setHeldOnEnter = setBtn.pressed();
      enterState(s);
    }

    void failClosed() {   // unknown state -> everything OFF (spec §12)
      latchFault(FAULT_STATE);
    }

    void latchFault(FaultCode c) {
      setActuatorPump(false); setActuatorValve(false); setActuatorSSR(false);
      if (fault == FAULT_NONE) fault = c;
      enterState(FAULT_LATCHED);
    }

    void setActuatorPump(bool on) {
      pumpOn_ = on;
      digitalWrite(PIN_PUMP, on ? HIGH : LOW);
    }
    void setActuatorValve(bool on) {
      digitalWrite(PIN_VALVE, on ? HIGH : LOW);
    }
    void setActuatorSSR(bool on) {
      digitalWrite(PIN_SSR, on ? HIGH : LOW);
    }

    // Thermal / safety supervision (independent of UI state, spec §8).
    void ntcTick(uint32_t now) {
      if (!startupSensorsChecked || fault != FAULT_NONE) return;
      FaultCode fc = ntc.faultCode();
      if (fc != FAULT_NONE) {
        latchFault(fc);
      } else if (!ntc.freshEnough(now)) {
        latchFault(FAULT_NTC_TIMEOUT);
      }
    }

    bool heatingPermission() const {
      // Fail closed for fault, BOOT_SAFE and unknown states, including the
      // final SSR write after the FSM. UI transitions cannot grant permission.
      switch (state) {
        case STARTUP_PRIME: case HEATING_IDLE: case READY_IDLE:
        case SETPOINT_EDIT: case CLEAN_FLUSH: case PRESET_SELECT_RUN:
        case PRESET_SELECT_RECORD: case PRESET_RECORD_READY:
        case PRESET_RECORD_ACTIVE: case RUN_PUMP_PREDELAY: case RUN_ACTIVE:
          break;
        default: return false;
      }
      return fault == FAULT_NONE && ntc.faultCode() == FAULT_NONE &&
             ntc.freshEnough(millis()) &&
             isfinite(ntc.rawTemp()) && isfinite(ntc.controlTemp()) &&
             ntc.rawTemp() <= ABS_OVERTEMP_C && ntc.controlTemp() <= ABS_OVERTEMP_C;
    }

    // ---------- tick handlers ----------
    // 2 Hz blink shared by the startup purge and the heat-up phase. It stops
    // only when the boiler reaches the setpoint (-> READY_IDLE).
    void dispBlink(uint32_t now) {
      if (now % 500 < 250) disp.allSegments(); else disp.blank();
    }

    void tickBootSafe(uint32_t /*now*/) {
      setActuatorPump(false); setActuatorValve(false); setActuatorSSR(false);
      // Display intentionally untouched here: it keeps showing FW_VERSION
      // from begin() until STARTUP_PRIME takes over.
      // Validate persistent + sensor (spec 8.1 step 2). Non-blocking: wait for a
      // valid sample or timeout.
      if (!startupSensorsChecked) {
        if (ntc.fresh()) {
          ntc.clearFresh();
          if (!ntc.valid()) {
            latchFault(FAULT_NTC_INVALID);
            return;
          }
          startupSensorsChecked = true;
          enterState(STARTUP_PRIME);
        } else if (elapsed(stateEnteredMs, NTC_TIMEOUT_MS)) {
          latchFault(FAULT_NTC_TIMEOUT);
        }
        return;
      }
      // (fallback) if already validated, proceed
      enterState(STARTUP_PRIME);
    }

    void tickPrime(uint32_t now) {
      // Five seconds of pump ON with the valve CLOSED: circulate and heat without
      // dumping water (owner decision; the earlier pump+valve purge is gone).
      setActuatorPump(true);
      setActuatorValve(false);
      dispBlink(now);
      if (elapsed(stateEnteredMs, PRIME_MS)) {
        setActuatorValve(false);
        setActuatorPump(false);
        enterIdle(HEATING_IDLE);
      }
    }

    void tickHeatingIdle(uint32_t now) {
      // Keep blinking until the boiler actually reaches the setpoint; it stops
      // only on the READY transition below (setpoint - READY_BAND_C).
      dispBlink(now);
      // ready check
      if (readyNow(thermostat.getSetpoint())) {
        // Same idle UI, different thermal indication: retain held gestures.
        enterState(READY_IDLE);
        disp.number(0, true);
      }
      handleIdleGestures(now);
    }

    // ---------- flush / clean run: single short SET press from idle ----------
    void enterCleanFlush(uint32_t now) {
      cleanStartMs = now;
      flushPressStartMs = 0;
      // The press that started this run is already consumed; flush the rest so a
      // bounce cannot be read as the stop press.
      while (setBtn.popEvent() != Button::EV_NONE) {}
      while (runBtn.popEvent() != Button::EV_NONE) {}
      setActuatorValve(true);
      setActuatorPump(true);
      enterState(CLEAN_FLUSH);
    }

    void tickCleanFlush(uint32_t now) {
      disp.seconds((now - cleanStartMs) / 1000);
      setActuatorValve(true);
      setActuatorPump(true);
      // RUN is inert here.
      while (runBtn.popEvent() != Button::EV_NONE) {}

      // Stop gesture: a SET press-and-release whose held duration is in
      // [FLUSH_STOP_MIN_MS, FLUSH_STOP_MAX_MS).
      Button::Event se = setBtn.popEvent();
      if (se == Button::EV_PRESSED) {
        flushPressStartMs = now;
      } else if (se == Button::EV_RELEASED && flushPressStartMs != 0) {
        uint32_t held = now - flushPressStartMs;
        flushPressStartMs = 0;
        if (held >= FLUSH_STOP_MIN_MS && held < FLUSH_STOP_MAX_MS) {
          setActuatorValve(false);
          setActuatorPump(false);
          enterIdleAfterDose();
          return;
        }
      }

      if (elapsed(cleanStartMs, MAX_DOSE_TIME_MS)) {
        setActuatorValve(false);
        setActuatorPump(false);
        enterIdleAfterDose();
      }
    }

    void tickReadyIdle(uint32_t now) {
      if (!readyNow(thermostat.getSetpoint())) {
        enterState(HEATING_IDLE);
        dispBlink(now);
        handleIdleGestures(now);
        return;
      }
      // Do NOT show the setpoint/temperature here (Anh wants it hidden). Idle
      // shows 0000 in both states; the RUN LED (steady ON) indicates READY.
      disp.number(0, true);
      handleIdleGestures(now);
    }

    // Single definition of "the boiler has reached the setpoint".
    bool readyNow(float sp) const {
      return fault == FAULT_NONE && ntc.faultCode() == FAULT_NONE &&
             ntc.freshEnough(millis()) && isfinite(ntc.rawTemp()) &&
             isfinite(ntc.controlTemp()) &&
             ntc.rawTemp() <= ABS_OVERTEMP_C && ntc.controlTemp() <= ABS_OVERTEMP_C &&
             ntc.controlTemp() >= (sp - READY_BAND_C);
    }

    // ---------- local setpoint edit: hold SET+RUN 5s, release ----------
    void enterSetpointEdit() {
      editSetpoint = thermostat.getSetpoint();
      editBothStartMs = 0;
      editLastKeyMs = millis();
      while (setBtn.popEvent() != Button::EV_NONE) {}
      while (runBtn.popEvent() != Button::EV_NONE) {}
      enterState(SETPOINT_EDIT);
    }

    void leaveSetpointEdit(bool save) {
      if (save) {
        float v = pers.clampSetpoint(editSetpoint);   // bounds + 0.5 snap
        if (v != pers.setpointC()) {
          pers.setpointC() = v;
          thermostat.setSetpoint(v);
          thermostat.clearHeatRequest();
          // Flash is synchronous: decide with the new target BEFORE commit.
          applyHeating();
          if (!pers.save()) {
            latchFault(FAULT_CRC);
            return;
          }
        }
        thermostat.setSetpoint(v);
      }
      enterIdle(readyNow(thermostat.getSetpoint()) ? READY_IDLE : HEATING_IDLE);
    }

    void tickSetpointEdit(uint32_t now) {
      // Blink, so the value can never be mistaken for the idle 0000.
      if ((now % 500) < 250) disp.temperature(editSetpoint);
      else                   disp.blank();

      if (setBtn.pressed() && runBtn.pressed()) {
        // Both buttons held -> save. Drop the individual events first: a
        // near-simultaneous press must not also count as +1 / -1.
        while (setBtn.popEvent() != Button::EV_NONE) {}
        while (runBtn.popEvent() != Button::EV_NONE) {}
        editLastKeyMs = now; // a save gesture is activity, not an abandoned edit
        if (editBothStartMs == 0) editBothStartMs = now;
        if ((now - editBothStartMs) >= SP_EDIT_SAVE_MS) {
          leaveSetpointEdit(true);
          return;
        }
      } else {
        editBothStartMs = 0;
        Button::Event se = setBtn.popEvent();
        Button::Event re = runBtn.popEvent();
        if (re == Button::EV_PRESSED) {
          editSetpoint += SP_EDIT_STEP_C;
          editLastKeyMs = now;
        }
        if (se == Button::EV_PRESSED) {
          editSetpoint -= SP_EDIT_STEP_C;
          editLastKeyMs = now;
        }
        if (editSetpoint > SETPOINT_MAX_C) editSetpoint = SETPOINT_MAX_C;
        if (editSetpoint < SETPOINT_MIN_C) editSetpoint = SETPOINT_MIN_C;
      }

      // Abandoned edit: leave idle WITHOUT saving (predictable, never silent).
      if ((now - editLastKeyMs) >= SP_EDIT_TIMEOUT_MS) leaveSetpointEdit(false);
    }

    void handleIdleGestures(uint32_t now) {
      // SET-held and SET+RUN-held behaviors are driven by pressed()/timers, not
      // SET events, so drain SET's queue every idle tick to avoid overflow.
      while (setBtn.popEvent() != Button::EV_NONE) {}

      bool setPressed = setBtn.pressed();
      bool runPressed = runBtn.pressed();

      // A SET still held when we entered idle (tail of a stop/save press) must
      // not arm a fresh gesture; wait until it is released once.
      if (setHeldOnEnter && !setPressed) setHeldOnEnter = false;

      // RUN pressed ALONE -> run-preset selection. It MUST NOT fire while SET is
      // also held: SET+RUN is the 5 s setpoint-edit hold handled below.
      if (runBtn.popEvent() == Button::EV_PRESSED && !setPressed) {
        selectRunIdx = pers.lastPreset();
        // Fall back to the first valid preset so RUN never reports "no" while
        // any preset exists (lastPreset may be stale after a fresh record).
        if (selectRunIdx < 1 || selectRunIdx > PRESET_COUNT || !pers.presetValid(selectRunIdx))
          selectRunIdx = firstValidPreset();
        enterState(PRESET_SELECT_RUN);
        return;
      }

      // SET+RUN held for SP_EDIT_HOLD_MS, then released -> local setpoint edit.
      if (!setHeldOnEnter && setPressed && runPressed) {
        if (editHoldStart == 0) editHoldStart = now;
      } else {
        if (editHoldStart != 0 && (now - editHoldStart) >= SP_EDIT_HOLD_MS) {
          editHoldStart = 0;
          enterSetpointEdit();
          return;
        }
        editHoldStart = 0;
      }

      // SET held 3s -> record a preset (RUN not held)
      if (!setHeldOnEnter && setPressed && !runPressed) {
        if (recHoldStart == 0) recHoldStart = now;
        if ((now - recHoldStart) >= RECORD_HOLD_SET_MS) {
          recHoldStart = 0;
          enterState(PRESET_SELECT_RECORD);
          selectRecordIdx = 1;
          return;
        }
      } else {
        // SET released before RECORD_HOLD_SET_MS and not a record gesture ->
        // flush/clean run. A press longer than CLEAN_PRESS_MAX_MS does nothing:
        // an accidental long press must not start water flowing by itself.
        if (recHoldStart != 0 && !setPressed && !runPressed) {
          uint32_t held = now - recHoldStart;
          recHoldStart = 0;
          if (held <= CLEAN_PRESS_MAX_MS) {
            enterCleanFlush(now);
            return;
          }
        }
        recHoldStart = 0;
      }
    }

    // Hold-entry timers, reset to 0 when the gesture is released.
    uint32_t editHoldStart = 0;
    uint32_t recHoldStart = 0;
    // True when SET was still held as we entered idle (tail of a stop/save
    // press). Suppresses fresh SET gestures until the button is released once.
    bool setHeldOnEnter = false;

    // --- local setpoint edit ---
    float    editSetpoint = 0.0f;
    uint32_t editLastKeyMs = 0;
    uint32_t editBothStartMs = 0;

    void tickPresetSelectRun(uint32_t now) {
      // display selected valid preset; SET advances, RUN confirms
      Button::Event e;
      if (selectRunIdx == 0) {
        // no valid preset to run; show "no" and wait for a button press
        disp.noPreset();
        if ((e = runBtn.popEvent()) != Button::EV_NONE || (e = setBtn.popEvent()) != Button::EV_NONE) {
          enterIdleAfterDose();
        }
        return;
      }
      disp.preset(selectRunIdx);
      if ((e = setBtn.popEvent()) == Button::EV_PRESSED) {
        // advance to next valid preset
        uint8_t t = selectRunIdx;
        for (int i = 0; i < PRESET_COUNT; i++) {
          uint8_t n = ((t - 1) + 1) % PRESET_COUNT + 1;
          if (pers.presetValid(n)) {
            selectRunIdx = n;
            break;
          }
          t = n;
        }
      } else if ((e = runBtn.popEvent()) == Button::EV_PRESSED) {
        currentPreset = selectRunIdx;
        doseStartMs = now;           // the elapsed clock starts at the RUN press
        doseValveOpenMs = 0;
        enterState(RUN_PUMP_PREDELAY);
      }
    }

    void tickPresetSelectRecord(uint32_t now) {
      // display flashing preset; RUN advances 1..PRESET_COUNT, SET confirms slot
      if (now % 500 < 250) disp.preset(selectRecordIdx); else disp.blank();
      Button::Event e;
      if ((e = runBtn.popEvent()) == Button::EV_PRESSED) {
        selectRecordIdx = (selectRecordIdx % PRESET_COUNT) + 1;
      } else if ((e = setBtn.popEvent()) == Button::EV_PRESSED) {
        enterState(PRESET_RECORD_READY);
      }
    }

    void tickPresetRecordReady(uint32_t now) {
      // display selected slot steady, SET LED fast flash; RUN starts recording
      disp.preset(selectRecordIdx);
      Button::Event e;
      if ((e = runBtn.popEvent()) == Button::EV_PRESSED) {
        // Same preamble as a dose (wet 2s, soak 2s, press 2s, then valve opens),
        // but driven by tickPresetRecordActive so the clock can start at the RUN
        // press while the actuators follow the phases.
        recordStartMs = now;
        recValveOpenMs = 0;
        recFlowStarted = false;
        setActuatorPump(false);
        setActuatorValve(false);
        enterState(PRESET_RECORD_ACTIVE);
      } else if ((e = setBtn.popEvent()) == Button::EV_PRESSED) {
        enterIdleAfterDose();
      }
    }

    void tickPresetRecordActive(uint32_t now) {
      uint32_t secs = (now - recordStartMs) / 1000;
      disp.seconds(secs);
      // Timeout wins over a simultaneous RUN save: an unconfirmed/overlong
      // recording must never overwrite a valid slot.
      if (recFlowStarted && elapsed(recValveOpenMs, MAX_DOSE_TIME_MS)) {
        setActuatorValve(false);
        setActuatorPump(false);
        recFlowStarted = false;
        enterIdleAfterDose();
        return;
      }
      Button::Event e;
      // SET = STOP without saving. SET already means "cancel" in the record flow
      // (see PRESET_RECORD_READY), so this keeps one meaning for the button.
      // (The leftover-press flush leak is handled by enterIdle()'s guard.)
      if ((e = setBtn.popEvent()) == Button::EV_PRESSED) {
        // close valve first, then pump (confirmed timing, spec 8.3)
        setActuatorValve(false);
        setActuatorPump(false);
        recValveOpenMs = 0;
        recFlowStarted = false;
        enterIdleAfterDose();
        return;
      }

      uint32_t t = now - recordStartMs;
      if (t < DOSE_PREWET_MS) {
        setActuatorValve(true);
        setActuatorPump(true);
      } else if (t < DOSE_PREWET_MS + DOSE_SOAK_MS) {
        setActuatorValve(false);
        setActuatorPump(false);
      } else if (t < DOSE_PREWET_MS + DOSE_SOAK_MS + DOSE_PRESS_MS) {
        setActuatorValve(false);
        setActuatorPump(true);
      } else {
        if (!recFlowStarted) {          // zero is a valid clock value
          recValveOpenMs = now;
          recFlowStarted = true;
        }
        setActuatorValve(true);
        setActuatorPump(true);
      }

      if ((e = runBtn.popEvent()) == Button::EV_PRESSED) {
        // stop recording: close valve first, then pump (confirmed timing, spec 8.3)
        setActuatorValve(false);
        setActuatorPump(false);
        // learnedSeconds is the FLOW duration (valve open -> stop). The preamble
        // is not part of the dose, unlike the display clock.
        uint32_t flowSecs = recFlowStarted ? ((now - recValveOpenMs) / 1000) : 0;
        recValveOpenMs = 0;
        recFlowStarted = false;
        enterIdleAfterDose();
        if (flowSecs > 0) {
          bool changed = pers.presetSeconds(selectRecordIdx) != flowSecs ||
                         pers.lastPreset() != selectRecordIdx;
          pers.setPreset(selectRecordIdx, flowSecs);
          pers.setLastPreset(selectRecordIdx);
          if (changed && !pers.save()) {
            latchFault(FAULT_CRC);
            return;
          }
        }
      }
    }

    // Where to land after a dose. Never claim READY while the boiler is not:
    // the READY LED means "ready to brew" and must not lie.
    void enterIdleAfterDose() {
      enterIdle(readyNow(thermostat.getSetpoint()) ? READY_IDLE : HEATING_IDLE);
      // Decide the post-cycle output immediately, also before a flash commit.
      applyHeating();
    }

    void tickRunPredelay(uint32_t now) {
      // Dose preamble. doseStartMs is the RUN press, so the elapsed clock is
      // already running (owner request: count from RUN, not from the valve).
      //   phase 1: valve OPEN + pump ON    DOSE_PREWET_MS  (bloom / pre-wet)
      //   phase 2: valve CLOSED, pump OFF  DOSE_SOAK_MS    (coffee absorbs)
      //   phase 3: pump ON, valve CLOSED   DOSE_PRESS_MS   (build pressure)
      //   then   : valve OPENS -> flow phase, RUN_ACTIVE
      disp.seconds((now - doseStartMs) / 1000);

      // SET = STOP from the first instant the pump runs. Single press, no hold,
      // no fault raised: a manual stop is not an error. (The leftover-press
      // flush leak is handled by enterIdle()'s guard.)
      if (setBtn.popEvent() == Button::EV_PRESSED || runBtn.held()) {
        abortDose();
        return;
      }

      uint32_t t = now - doseStartMs;
      if (t < DOSE_PREWET_MS) {
        // Wet the puck: valve open, pump running.
        setActuatorValve(true);
        setActuatorPump(true);
        return;
      }
      if (t < DOSE_PREWET_MS + DOSE_SOAK_MS) {
        // Let the coffee absorb water: pump/valve OFF; brew heat stays requested.
        setActuatorValve(false);
        setActuatorPump(false);
        return;
      }
      if (t < DOSE_PREWET_MS + DOSE_SOAK_MS + DOSE_PRESS_MS) {
        // Build pressure against a closed valve.
        setActuatorValve(false);
        setActuatorPump(true);
        return;
      }
      // Extraction: the valve opens and the extraction-seconds clock starts here.
      doseValveOpenMs = now;
      setActuatorValve(true);
      setActuatorPump(true);
      enterState(RUN_ACTIVE);
    }

    void tickRunActive(uint32_t now) {
      setActuatorPump(true);
      setActuatorValve(true);
      uint32_t secs = (now - doseStartMs) / 1000;
      disp.seconds(secs);

      // SET = STOP while brewing. An explicit operator stop must always exist.
      // (The leftover-press flush leak is handled by enterIdle()'s guard.)
      if (setBtn.popEvent() == Button::EV_PRESSED) {
        abortDose();
        return;
      }

      // user abort: hold RUN for 2s (spec 8.4)
      if (runBtn.held()) {
        abortDose();
        return;
      }

      uint32_t targetSecs = pers.presetSeconds(currentPreset);

      // max-dose-time safety net (measured from the valve opening).
      if (elapsed(doseValveOpenMs, MAX_DOSE_TIME_MS)) {
        setActuatorValve(false); setActuatorPump(false);
        enterIdleAfterDose();
        return;
      }
      // dose complete: extraction seconds elapsed since the valve opened.
      if (targetSecs > 0 && elapsed(doseValveOpenMs, targetSecs * 1000UL)) {
        setActuatorValve(false);
        setActuatorPump(false);
        enterIdleAfterDose();
        if (pers.lastPreset() != currentPreset) {
          pers.setLastPreset(currentPreset);
          if (!pers.save()) {
            latchFault(FAULT_CRC);
            return;
          }
        }
      }
    }

    void abortDose() {
      setActuatorValve(false);
      setActuatorPump(false);
      enterIdleAfterDose();
    }

    void tickFaultLatched(uint32_t /*now*/) {
      setActuatorPump(false); setActuatorValve(false); setActuatorSSR(false);
      disp.error(fault != FAULT_NONE ? fault : FAULT_NTC_INVALID);
      // Fault latches until explicit reset. No automatic clearing.
    }

    // ---------- LEDs (spec section 9) ----------
    void applyLeds(uint32_t now) {
      bool setOn = false, runOn = false;

      switch (state) {
        case STARTUP_PRIME:
          setOn = runOn = (now % 500) < 250;   // flash together at 2 Hz
          break;
        case HEATING_IDLE:
          runOn = (now % 1000) < 500;          // slow flash 1 Hz
          setOn = false;
          break;
        case READY_IDLE:
        case PRESET_SELECT_RUN:
          runOn = true; setOn = false;
          break;
        case SETPOINT_EDIT:
          // Mirror the buttons, so every press is visibly registered.
          setOn = setBtn.pressed();
          runOn = runBtn.pressed();
          break;
        case CLEAN_FLUSH:
          // Both LEDs steady: distinct from a dose (RUN steady) and from idle.
          setOn = true;
          runOn = true;
          break;
        case PRESET_SELECT_RECORD:
          setOn = (now % 1000) < 500;          // slow flash
          runOn = false;
          break;
        case PRESET_RECORD_READY:
          setOn = (now % 250) < 125;           // fast flash
          runOn = true;
          break;
        case PRESET_RECORD_ACTIVE:
          setOn = true;
          runOn = (now % 250) < 125;           // fast flash
          break;
        case RUN_PUMP_PREDELAY:
          runOn = true; setOn = false;
          break;
        case RUN_ACTIVE:
          runOn = true; setOn = false;
          break;
        case FAULT_LATCHED:
          // alternate fast flash
          if ((now % 250) < 125) {
            setOn = true;
            runOn = false;
          }
          else {
            setOn = false;
            runOn = true;
          }
          break;
        default:
          setOn = runOn = false;
          break;
      }
      digitalWrite(PIN_LED_SET, setOn ? HIGH : LOW);
      digitalWrite(PIN_LED_RUN, runOn ? HIGH : LOW);
    }
};

// ============================================================================
//  12. SETUP & LOOP
// ============================================================================
CoffeeMachine machine;

void setup() {
  // NOTE: production firmware intentionally does NOT call Serial.begin()
  // because GPIO1/GPIO3 are used for the two LEDs (COFFE_README §2.2).
  machine.begin();
}

void loop() {
  // Return frequently to feed the ESP8266 watchdog. No delay() in control path.
  machine.tick();
}
