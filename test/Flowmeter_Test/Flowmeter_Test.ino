/*
  Flowmeter_Test — NodeMCU ESP8266 + flowmeter (D1/GPIO5) -> 4x7SEG (4x74HC595)
  Wing: hardware | Topic: flowmeter-test | Updated: 2026-09-04
  CHỈ TEST cảm biến lưu lượng: đếm xung + hiển thị số xung / thể tích trên 4x7SEG.

  =========== WIRING (NodeMCU) ===========
    Flowmeter signal -> D1 (GPIO5)      (interrupt; edge + pull-up phải verify theo sensor)
    74HC595:  D2 (GPIO4)=SDI|data,  D3 (GPIO0)=SCLK|clock,  D4 (GPIO2)=LOAD|latch
    RUN (D8/GPIO15) -> GND-... reset counter (active HIGH, external 10k pull-DOWN)
    SET (D0/GPIO16) -> toggle mode    (active LOW,  external 10k pull-UP)
  ======================================

  Display reuse: same polarity as the production firmware (segments INVERTED,
  i.e. the module is driven active-LOW). `sr.setAll()` only when value changes.

  Modes (SET toggles):
    0 = số xung       (0..9999)
    1 = thể tích (L)  (xung / 5880, hiển thị 3 số lẻ, VD "0.588", "1.000")
*/

#include <ShiftRegister74HC595.h>

// ------------------- PIN ASSIGNMENTS -------------------
#define PIN_SDI   D2   // GPIO4 -> 74HC595 data
#define PIN_SCLK  D3   // GPIO0 -> 74HC595 clock
#define PIN_LOAD  D4   // GPIO2 -> 74HC595 latch
#define PIN_FLOW  D1   // GPIO5 -> flowmeter interrupt
#define PIN_SET   D0   // GPIO16 -> SET button (LOW = pressed)
#define PIN_RUN   D8   // GPIO15 -> RUN button (HIGH = pressed)

// ------------------- FLOW CONFIG -------------------
#define FLOW_EDGE       FALLING              // TBD: RISING / FALLING / CHANGE theo sensor
#define PULSES_PER_LITRE 5880.0f             // spec 6: 1 litre = 5880 pulses

// ------------------- DISPLAY CONFIG -------------------
#define DISPLAY_INVERT 1                     // 1 = invert segment (active-LOW, như firmware chính)
#define BTN_DEBOUNCE_MS 25

ShiftRegister74HC595<4> sr(PIN_SDI, PIN_SCLK, PIN_LOAD);

const uint8_t segCode[10] = {   // bit0=A ... bit6=G, bit7=DP (active-HIGH)
  0x3F, 0x06, 0x5B, 0x4F, 0x66,
  0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// ------------------- FLOW ISR (minimal: increment only) -------------------
volatile uint32_t pulseCount = 0;

#if defined(IRAM_ATTR)
  static void IRAM_ATTR flowIsr() { pulseCount++; }
#elif defined(ICACHE_RAM_ATTR)
  static void ICACHE_RAM_ATTR flowIsr() { pulseCount++; }
#else
  static void flowIsr() { pulseCount++; }
#endif

// ------------------- DISPLAY HELPERS -------------------
// Build 4 bytes [0]=nghìn(trái) ... [3]=đơn vị(phải), optional DP, then push to 595.
// dpMask bit i lights DP on digit i (i=0=trái nhất).
void showValue(uint16_t value, bool leadingZero, uint8_t dpMask = 0) {
  uint8_t d[4];
  d[0] = segCode[(value / 1000) % 10];
  d[1] = segCode[(value / 100) % 10];
  d[2] = segCode[(value / 10) % 10];
  d[3] = segCode[value % 10];
  if (!leadingZero) {
    if (value < 1000) d[0] = 0x00;
    if (value < 100)  d[1] = 0x00;
    if (value < 10)   d[2] = 0x00;
  }
  for (uint8_t i = 0; i < 4; i++) if (dpMask & (0x01 << i)) d[i] |= 0x80;

#if DISPLAY_INVERT
  for (uint8_t i = 0; i < 4; i++) d[i] = ~d[i] & 0xFF;
#endif
  sr.setAll(d);
}

void showBlank() {
  uint8_t d[4] = {0, 0, 0, 0};
#if DISPLAY_INVERT
  for (uint8_t i = 0; i < 4; i++) d[i] = ~d[i] & 0xFF;
#endif
  sr.setAll(d);
}

void showAll() {
  uint8_t d[4] = {0x7F, 0x7F, 0x7F, 0x7F};   // A-G, no DP (=> "8888")
#if DISPLAY_INVERT
  for (uint8_t i = 0; i < 4; i++) d[i] = ~d[i] & 0xFF;
#endif
  sr.setAll(d);
}

// ------------------- BUTTON DEBOUNCE -------------------
struct DebBtn {
  bool stable, lastRaw;
  uint32_t lastChangeMs;
  void begin(uint8_t p, bool activeLow) { pin = p; low = activeLow; pinMode(p, INPUT); stable = lastRaw = digitalRead(p); lastChangeMs = 0; }
  bool pressed() const { return stable == low; }      // low==true means "active" level
  void poll() {
    bool raw = digitalRead(pin);
    if (raw != lastRaw) { lastRaw = raw; lastChangeMs = millis(); }
    if (millis() - lastChangeMs >= BTN_DEBOUNCE_MS) stable = raw;
  }
  uint8_t pin; bool low;
};

DebBtn btnSet, btnRun;

// ------------------- SETUP -------------------
void setup() {
  // display driver init (library sets pinMode internally)
  pinMode(PIN_FLOW, INPUT_PULLUP);      // TBD: chỉ khi sensor cho phép pull-up nội bộ
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowIsr, FLOW_EDGE);

  btnSet.begin(PIN_SET, LOW);   // active LOW -> pressed() when LOW
  btnRun.begin(PIN_RUN, HIGH);  // active HIGH -> pressed() when HIGH

  pulseCount = 0;

  // boot test: 8888
  showAll();
  delay(700);
  showBlank();
  delay(200);
}

// ------------------- LOOP -------------------
uint8_t mode = 0;               // 0 = xung, 1 = lít
bool runWasPressed = false, setWasPressed = false;
uint32_t lastShown = 0xFFFFFFFF;

void loop() {
  btnSet.poll();
  btnRun.poll();

  // SET toggles mode (on press)
  if (btnSet.pressed() && !setWasPressed) {
    mode = (mode + 1) & 1;
    lastShown = 0xFFFFFFFF;    // force display refresh
  }
  setWasPressed = btnSet.pressed();

  // RUN resets counter (on press)
  if (btnRun.pressed() && !runWasPressed) {
    noInterrupts();
    pulseCount = 0;
    interrupts();
    lastShown = 0xFFFFFFFF;
  }
  runWasPressed = btnRun.pressed();

  // read count atomically
  uint32_t pulses;
  noInterrupts(); pulses = pulseCount; interrupts();

  // display
  if (mode == 0) {
    uint16_t v = (pulses > 9999) ? 9999 : (uint16_t)pulses;
    if (v != lastShown) {
      showValue(v, true);
      lastShown = v;
    }
  } else {
    // thể tích lít = pulses/5880, hiện "x.xxx"
    uint16_t scaled = (uint16_t)((pulses * 1000UL) / (uint32_t)PULSES_PER_LITRE);
    if (scaled > 9999) scaled = 9999;
    if (scaled != lastShown) {
      showValue(scaled, true, 0x01);   // DP on leftmost digit -> "x.xxx"
      lastShown = scaled;
    }
  }
}
