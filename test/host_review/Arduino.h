#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define A0 17
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define FALLING 3
struct PinWrite { int pin; int value; uint32_t ms; };
extern uint32_t fakeMs;
extern int pins[32];
extern int mockAdc;
extern uint32_t mockAnalogAdvanceMs;
extern std::vector<PinWrite> writes;
inline uint32_t millis() { return fakeMs; }
inline void pinMode(int, int) {}
inline int digitalRead(int p) { return pins[p]; }
inline void digitalWrite(int p, int v) { pins[p]=v; writes.push_back({p,v,fakeMs}); }
inline int analogRead(int) { fakeMs += mockAnalogAdvanceMs; return mockAdc; }
inline int digitalPinToInterrupt(int p) { return p; }
inline void attachInterrupt(int, void(*)(), int) {}
