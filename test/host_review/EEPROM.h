#pragma once
#include <array>
#include <cstring>
#include <cassert>
struct MockEEPROM {
  std::array<uint8_t,512> flash, pending;
  bool beginOk=true, commitOk=true;
  unsigned commits=0, puts=0;
  size_t bufferSize=0; bool bufferPresent=true;
  MockEEPROM() { erase(); }
  void erase() {
    flash.fill(255); pending=flash; beginOk=commitOk=true; commits=puts=0;
    bufferSize=0; bufferPresent=true;
  }
  void begin(size_t size) {
    assert(size<=flash.size()); pending=flash; bufferSize=beginOk?size:0;
  }
  size_t length() { return bufferSize; }
  const uint8_t* getConstDataPtr() const { return bufferPresent?pending.data():nullptr; }
  template<class T> void get(size_t offset,T& v) {
    assert(offset+sizeof(T)<=pending.size());
    std::memcpy(&v,pending.data()+offset,sizeof(T));
  }
  template<class T> void put(size_t offset,const T& v) {
    assert(offset+sizeof(T)<=pending.size());
    std::memcpy(pending.data()+offset,&v,sizeof(T)); ++puts;
  }
  bool commit() { ++commits; if (!commitOk) return false; flash=pending; return true; }
};
static MockEEPROM EEPROM;
