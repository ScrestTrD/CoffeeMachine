#pragma once
#include <array>
extern std::array<uint8_t,4> displayFrame;
template<int N> struct ShiftRegister74HC595 {
  ShiftRegister74HC595(int,int,int) {}
  void setAll(uint8_t* p) { for(int i=0;i<N;++i) displayFrame[i]=p[i]; }
};
