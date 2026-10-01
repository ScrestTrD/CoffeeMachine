// Regression assertions on production code with mock I/O, not a hardware build.
// Private access is limited to arranging faults/FSM fixtures and EEPROM bytes.
#include "Arduino.h"
#include "EEPROM.h"
#include "ShiftRegister74HC595.h"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <limits>
uint32_t fakeMs=1000;
int pins[32]={};
int mockAdc=250;
uint32_t mockAnalogAdvanceMs=0;
std::vector<PinWrite> writes;
std::array<uint8_t,4> displayFrame;
#ifndef COFFEE_FIRMWARE
#define COFFEE_FIRMWARE "../../CoffeeMachine/CoffeeMachine.ino"
#endif
#define private public
#include COFFEE_FIRMWARE
#undef private

unsigned total=0, failed=0;
void check(bool ok, const char* why) { if(!ok) throw std::runtime_error(why); }
void test(const char* name, const std::function<void()>& f) {
  ++total;
  try { f(); std::cout<<"PASS "<<name<<"\n"; }
  catch(const std::exception& e) { ++failed; std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n"; }
}
void resetIo(bool erase=true) {
  fakeMs=1000; mockAdc=250; mockAnalogAdvanceMs=0;
  for(int& p:pins) p=LOW;
  pins[PIN_SETB]=HIGH; pins[PIN_RUNB]=LOW;
  writes.clear(); if(erase) EEPROM.erase();
}
void sensor(CoffeeMachine& m, float filtered=80, float unfiltered=80) {
  m.ntc.ctrlValid=true; m.ntc.lastCtrlT=filtered; m.ntc.lastRawT=unfiltered;
  m.ntc.fault=FAULT_NONE;
#ifndef COFFEE_LEGACY_BASELINE
  m.ntc.lastValidMs=fakeMs;
#endif
}
void init(CoffeeMachine& m) {
  resetIo(); m.begin(); m.startupSensorsChecked=true; sensor(m);
}
void tickAt(CoffeeMachine& m,uint32_t t,bool refresh=true) {
  fakeMs=t; m.ntc.lastSampleMs=t; // Simulate a paused ADC sampler.
#ifndef COFFEE_LEGACY_BASELINE
  if(refresh) m.ntc.lastValidMs=t;
#else
  (void)refresh;
#endif
  m.tick();
}
bool allOff() { return pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW && pins[PIN_SSR]==LOW; }
bool noActuatorHighWrite() {
  for(const auto& w:writes)
    if((w.pin==PIN_PUMP || w.pin==PIN_VALVE || w.pin==PIN_SSR) && w.value==HIGH) return false;
  return true;
}
void pressEvent(Button& b,int p,bool activeLow) {
  pins[p]=activeLow?LOW:HIGH;
  b.stable=b.lastStable=pins[p]; b.lastChangeMs=fakeMs; b.heldFired=false;
  b.qHead=b.qTail=0; b.enqueue(Button::EV_PRESSED);
}
void setupRecord(CoffeeMachine& m,uint32_t flowStart=10000) {
  m.state=PRESET_RECORD_ACTIVE; m.selectRecordIdx=1;
  m.recordStartMs=flowStart-6000; m.recValveOpenMs=flowStart;
#ifndef COFFEE_LEGACY_BASELINE
  m.recFlowStarted=true;
#endif
  m.pers.setPreset(1,25); m.pers.setLastPreset(1);
}
void seedFlash(PersistentConfig cfg) {
  cfg.magic=CFG_MAGIC; cfg.version=CFG_VERSION;
  cfg.payloadSize=sizeof(PersistentConfig)-sizeof(uint32_t);
  cfg.crc32=Persistence::crc32(reinterpret_cast<const uint8_t*>(&cfg),
                            sizeof(PersistentConfig)-sizeof(uint32_t));
  std::memcpy(EEPROM.flash.data(),&cfg,sizeof(cfg)); EEPROM.pending=EEPROM.flash;
}
PersistentConfig goodConfig() {
  Persistence p; p.defaults(); return p.cfgRef();
}

int main() {
  // These cases also run against the v6 snapshot to prove regression sensitivity.
  test("G01 unknown-state remains OFF through final write and subsequent ticks", []{
    CoffeeMachine m; init(m); m.state=static_cast<FsmState>(255); writes.clear();
    tickAt(m,1100); tickAt(m,1101);
    check(m.state==FAULT_LATCHED && allOff() && noActuatorHighWrite(),"unknown state enabled actuator");
  });
  test("G02 recording timeout preserves old preset and avoids commit", []{
    CoffeeMachine m; init(m); setupRecord(m); tickAt(m,70000);
    check(m.state!=PRESET_RECORD_ACTIVE && pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,
          "recording still flowing after 60s");
    check(m.pers.presetSeconds(1)==25 && EEPROM.commits==0,"timeout overwrote confirmed preset");
  });
  test("G03 READY demotes after cooling", []{
    CoffeeMachine m; init(m); m.state=READY_IDLE; tickAt(m,1100);
    check(m.state==HEATING_IDLE && displayFrame[3]==0x80,"cold machine still READY");
  });
  test("G03 cancelling record-ready while cold does not report READY", []{
    CoffeeMachine m; init(m); m.state=PRESET_RECORD_READY;
    pressEvent(m.setBtn,PIN_SETB,true); tickAt(m,1000);
    check(m.state==HEATING_IDLE,"cancel reported READY with cold NTC");
  });
  test("G04 RUN held 2s aborts preamble using real debounce", []{
    CoffeeMachine m; init(m); m.state=RUN_PUMP_PREDELAY; m.doseStartMs=1000;
    pins[PIN_RUNB]=HIGH;
    tickAt(m,1001); tickAt(m,1026); tickAt(m,3000);
    check(m.state==RUN_PUMP_PREDELAY,"RUN aborted before hold threshold");
    tickAt(m,3001);
    check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,
          "RUN hold ignored in preamble");
  });

#ifndef COFFEE_LEGACY_BASELINE
  test("G01 FAULT_LATCHED without a code is still heater-locked", []{
    CoffeeMachine m; init(m); m.state=FAULT_LATCHED; m.fault=FAULT_NONE;
    writes.clear(); tickAt(m,1100);
    check(allOff() && noActuatorHighWrite(),"fault state restored SSR");
  });
  test("G01 NTC fault takes precedence over running pump", []{
    CoffeeMachine m; init(m); m.state=RUN_ACTIVE; m.currentPreset=1;
    m.ntc.fault=FAULT_NTC_INVALID; m.setActuatorPump(true); m.setActuatorValve(true);
    writes.clear(); tickAt(m,1100);
    check(m.fault==FAULT_NTC_INVALID && allOff() && noActuatorHighWrite(),"sensor fault failed closed");
  });
  test("G01 unknown-state error is E8", []{
    CoffeeMachine m; init(m); m.state=static_cast<FsmState>(254); tickAt(m,1100);
    check(m.fault==FAULT_STATE,"unknown state had no persistent error");
  });
  test("G01 BOOT_SAFE prevents all actuator HIGH writes", []{
    resetIo(); CoffeeMachine m; m.begin(); writes.clear(); fakeMs=1001; m.tick();
    check(allOff() && noActuatorHighWrite(),"boot energized actuator before validation");
  });
  test("G02 recording still runs at 59.999s and cancels exactly at 60s", []{
    CoffeeMachine m; init(m); setupRecord(m); tickAt(m,69999);
    check(m.state==PRESET_RECORD_ACTIVE && pins[PIN_VALVE]==HIGH,"recording cut early");
    tickAt(m,70000);
    check(m.state==HEATING_IDLE && pins[PIN_VALVE]==LOW && EEPROM.commits==0,"60s cutoff failed");
  });
  test("G02 timeout wins over simultaneous RUN save", []{
    CoffeeMachine m; init(m); setupRecord(m); fakeMs=70000;
    pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,70000);
    check(m.pers.presetSeconds(1)==25 && EEPROM.commits==0,"late RUN committed timed-out record");
  });
  test("G02 recording extraction may start at clock zero", []{
    CoffeeMachine m; init(m); m.state=PRESET_RECORD_ACTIVE; m.selectRecordIdx=1;
    m.recordStartMs=uint32_t(0)-6000; m.recFlowStarted=false; m.pers.setPreset(1,25);
    tickAt(m,0); tickAt(m,59999);
    check(m.state==PRESET_RECORD_ACTIVE,"zero-clock extraction stopped early");
    tickAt(m,60000);
    check(m.state==HEATING_IDLE && m.pers.presetSeconds(1)==25,"zero-clock extraction escaped limit");
  });
  test("G02 recording timeout crosses millis wrap", []{
    CoffeeMachine m; init(m); const uint32_t start=UINT32_MAX-30000;
    setupRecord(m,start); tickAt(m,start+59999u);
    check(m.state==PRESET_RECORD_ACTIVE,"wrap cutoff early");
    tickAt(m,start+60000u);
    check(m.state==HEATING_IDLE && EEPROM.commits==0,"wrap timeout missed");
  });
  test("G02 SET cancels record without modifying flash", []{
    CoffeeMachine m; init(m); setupRecord(m); fakeMs=12000;
    pressEvent(m.setBtn,PIN_SETB,true); tickAt(m,12000);
    check(m.state==HEATING_IDLE && m.pers.presetSeconds(1)==25 && EEPROM.commits==0,"SET saved recording");
  });
  test("G02 zero-second RUN record does not overwrite slot", []{
    CoffeeMachine m; init(m); setupRecord(m); fakeMs=10500;
    pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,10500);
    check(m.pers.presetSeconds(1)==25 && EEPROM.commits==0,"zero seconds overwrote preset");
  });
  test("G02 explicit valid recording saves extraction seconds only", []{
    CoffeeMachine m; init(m); setupRecord(m); fakeMs=19000;
    pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,19000);
    check(m.pers.presetSeconds(1)==9 && EEPROM.commits==1,"record included preamble or failed save");
    CoffeeMachine reboot; reboot.begin();
    check(reboot.pers.presetSeconds(1)==9,"confirmed preset lost after restart");
  });
  test("G03 READY threshold is inclusive", []{
    CoffeeMachine m; init(m); sensor(m,95.5f,95.5f); m.state=HEATING_IDLE;
    tickAt(m,1100); check(m.state==READY_IDLE,"threshold did not enter READY");
    tickAt(m,1101); check(m.state==READY_IDLE,"READY threshold unstable");
  });
  test("G03 no-preset exit recalculates cold readiness", []{
    CoffeeMachine m; init(m); m.state=PRESET_SELECT_RUN; m.selectRunIdx=0;
    pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,1000);
    check(m.state==HEATING_IDLE,"no-preset exit falsely READY");
  });
  test("G03 READY refuses unfiltered overtemperature", []{
    CoffeeMachine m; init(m); sensor(m,100,146); m.state=READY_IDLE; tickAt(m,1100);
    check(m.state==HEATING_IDLE && pins[PIN_SSR]==LOW,"overtemp READY");
  });
  test("G04 SET stops preamble after debounce, release never starts clean", []{
    CoffeeMachine m; init(m); m.state=RUN_PUMP_PREDELAY; m.doseStartMs=1000;
    tickAt(m,1000); pins[PIN_SETB]=LOW; tickAt(m,1100); tickAt(m,1125);
    check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,"SET did not stop");
    pins[PIN_SETB]=HIGH; tickAt(m,1200); tickAt(m,1225); tickAt(m,1300);
    check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW,"stop release triggered clean");
  });
  test("G04 RUN hold aborts active extraction", []{
    CoffeeMachine m; init(m); m.state=RUN_ACTIVE; m.currentPreset=1;
    m.pers.setPreset(1,25); m.doseStartMs=1000; m.doseValveOpenMs=7000;
    pins[PIN_RUNB]=HIGH; tickAt(m,7000); tickAt(m,7025); tickAt(m,9000);
    check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW && EEPROM.commits==0,"RUN did not abort active dose");
  });
  test("G05 boot validates actual seven-sample window before prime", []{
    resetIo(); fakeMs=0; CoffeeMachine m; m.begin();
    for(uint32_t t=6;t<=36;t+=6) { fakeMs=t; m.tick(); check(allOff(),"boot sample window incomplete"); }
    fakeMs=42; m.tick(); check(m.state==STARTUP_PRIME,"good sample did not unlock prime");
    fakeMs=48; m.tick(); check(pins[PIN_PUMP]==HIGH && pins[PIN_VALVE]==LOW,"prime outputs incorrect");
  });
  test("G05 boot without publish times out at 2s", []{
    resetIo(); fakeMs=0; CoffeeMachine m; m.begin();
    tickAt(m,1999,false); check(m.state==BOOT_SAFE,"boot timeout early");
    tickAt(m,2000,false); check(m.fault==FAULT_NTC_TIMEOUT && allOff(),"boot timeout missed");
  });
  test("G05 runtime age 1999ms accepted, 2000ms latches E3", []{
    CoffeeMachine m; init(m); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    tickAt(m,2999,false); check(m.fault==FAULT_NONE && pins[PIN_PUMP]==HIGH,"age check early");
    tickAt(m,3000,false); check(m.fault==FAULT_NTC_TIMEOUT && allOff(),"stale sensor did not latch");
  });
  test("G05 freshness survives millis wrap", []{
    CoffeeMachine m; init(m); m.state=HEATING_IDLE; m.ntc.lastValidMs=UINT32_MAX-1000;
    tickAt(m,998,false); check(m.fault==FAULT_NONE,"wrapped sample expired early");
    tickAt(m,999,false); check(m.fault==FAULT_NTC_TIMEOUT && allOff(),"wrapped expiry missed");
  });
  for(int adc:{0,1023}) test(adc==0?"G05 runtime short latches E1":"G05 runtime open latches E1",[adc]{
    CoffeeMachine m; init(m); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    int buf[7]={adc,adc,adc,adc,adc,adc,adc}; m.ntc.publishMedian(buf,7);
    writes.clear(); tickAt(m,1100);
    check(m.fault==FAULT_NTC_INVALID && allOff() && noActuatorHighWrite(),"bad ADC left actuators on");
  });
  test("G05 invalid beta conversion latches E1, never bogus control temp", []{
    CoffeeMachine m; init(m); m.state=HEATING_IDLE; m.ntc.setParams(NAN,3950);
    int buf[7]={250,250,250,250,250,250,250}; m.ntc.publishMedian(buf,7);
    tickAt(m,1100); check(m.fault==FAULT_NTC_INVALID && allOff(),"NaN conversion accepted");
  });
  test("G05 fresh sample timestamp is captured after analogRead", []{
    resetIo(); fakeMs=0; CoffeeMachine m; m.begin(); mockAnalogAdvanceMs=1;
    for(uint32_t t=6;t<=42;t+=6) { fakeMs=t; m.tick(); }
    check(m.state==STARTUP_PRIME && m.fault==FAULT_NONE,"new timestamp compared to old clock");
    fakeMs=48; m.tick(); check(m.fault==FAULT_NONE && pins[PIN_PUMP]==HIGH,"new sample underflowed age");
  });
  test("G06 valid v5 calibration and presets preserved, setpoint snapped", []{
    resetIo(); PersistentConfig cfg=goodConfig(); cfg.temperatureSetpointC=97.3f;
    cfg.ntcR0=65000; cfg.ntcBeta=4100; cfg.presets[1].valid=1; cfg.presets[1].seconds=30;
    cfg.lastPreset=2; seedFlash(cfg); CoffeeMachine m; m.begin();
    check(m.pers.isValid(),"normalized RAM record failed CRC");
    check(m.pers.setpointC()==97.5f && m.pers.ntcR0()==65000 && m.pers.ntcBeta()==4100,
          "load overwrote valid custom calibration");
    check(m.pers.presetSeconds(2)==30 && m.pers.lastPreset()==2 && EEPROM.commits==0,
          "v5 load lost presets or wrote flash");
  });
  struct InvalidCase { const char* name; std::function<void(PersistentConfig&)> change; };
  const InvalidCase badCases[]={
    {"NaN setpoint",[](PersistentConfig& c){c.temperatureSetpointC=NAN;}},
    {"infinite setpoint",[](PersistentConfig& c){c.temperatureSetpointC=INFINITY;}},
    {"low setpoint",[](PersistentConfig& c){c.temperatureSetpointC=89;}},
    {"high setpoint",[](PersistentConfig& c){c.temperatureSetpointC=141;}},
    {"NaN R0",[](PersistentConfig& c){c.ntcR0=NAN;}},
    {"zero R0",[](PersistentConfig& c){c.ntcR0=0;}},
    {"high R0",[](PersistentConfig& c){c.ntcR0=1000001;}},
    {"infinite beta",[](PersistentConfig& c){c.ntcBeta=INFINITY;}},
    {"low beta",[](PersistentConfig& c){c.ntcBeta=799;}},
    {"high beta",[](PersistentConfig& c){c.ntcBeta=6001;}},
    {"invalid lastPreset",[](PersistentConfig& c){c.lastPreset=3;}},
    {"invalid valid flag",[](PersistentConfig& c){c.presets[0].valid=2;}},
    {"zero valid seconds",[](PersistentConfig& c){c.presets[0].valid=1;c.presets[0].seconds=0;}},
    {"overlong seconds",[](PersistentConfig& c){c.presets[0].valid=1;c.presets[0].seconds=61;}},
    {"overflow seconds",[](PersistentConfig& c){c.presets[0].valid=1;c.presets[0].seconds=UINT32_MAX;}}
  };
  for(const auto& item:badCases) {
    const std::string label=std::string("G06 reject CRC-valid ")+item.name;
    test(label.c_str(),[&item]{
      resetIo(); PersistentConfig cfg=goodConfig(); item.change(cfg); seedFlash(cfg);
      CoffeeMachine m; m.begin();
      check(m.pers.setpointC()==DEFAULT_SETPOINT_C && m.pers.ntcR0()==NTC_DEFAULT_R0 &&
            !m.pers.presetValid(1) && !m.pers.presetValid(2) && EEPROM.commits==0,
            "CRC-valid invalid values survived load");
    });
  }
  test("G06 bad CRC falls back without flash writes", []{
    resetIo(); seedFlash(goodConfig()); EEPROM.flash[offsetof(PersistentConfig,crc32)]^=1; CoffeeMachine m; m.begin();
    check(m.pers.setpointC()==DEFAULT_SETPOINT_C && EEPROM.commits==0,"CRC fallback wrote flash");
  });
  test("G06 incompatible version falls back", []{
    resetIo(); PersistentConfig cfg=goodConfig(); seedFlash(cfg);
    uint16_t old=CFG_VERSION-1; std::memcpy(EEPROM.flash.data()+offsetof(PersistentConfig,version),&old,sizeof(old));
    CoffeeMachine m; m.begin(); check(!m.pers.presetValid(1) && EEPROM.commits==0,"old layout accepted");
  });
  test("G06 invalid preset index is safe, zero/overlong cannot overwrite", []{
    Persistence p; p.defaults(); p.setPreset(1,25); p.setPreset(1,0); p.setPreset(1,61);
    check(p.presetSeconds(1)==25 && p.presetSeconds(0)==0 && p.presetSeconds(3)==0,"preset bounds unsafe");
  });
  test("G06 invalid save has no write side effects", []{
    resetIo(); Persistence p; p.defaults(); p.setpointC()=NAN;
    check(!p.save() && EEPROM.commits==0 && EEPROM.puts==0,"invalid config reached EEPROM");
    check(p.clampSetpoint(NAN)==DEFAULT_SETPOINT_C,"NaN clamp leaked");
  });
  test("G06 EEPROM begin failure latches E6 and keeps outputs OFF", []{
    resetIo(); EEPROM.beginOk=false; CoffeeMachine m; m.begin();
    writes.clear(); tickAt(m,1100);
    check(m.fault==FAULT_CRC && allOff() && noActuatorHighWrite(),"EEPROM begin failure not closed");
  });
  test("G06 missing EEPROM buffer latches E6", []{
    resetIo(); EEPROM.bufferPresent=false; CoffeeMachine m; m.begin();
    check(m.fault==FAULT_CRC && allOff(),"null EEPROM buffer not detected");
  });
  test("G06 input bounds accept boundary config and 60s preset", []{
    for(float sp:{90.0f,140.0f}) {
      resetIo(); PersistentConfig cfg=goodConfig(); cfg.temperatureSetpointC=sp;
      cfg.presets[0].valid=1; cfg.presets[0].seconds=60;
      cfg.ntcR0=1000;cfg.ntcBeta=800;seedFlash(cfg);CoffeeMachine m;m.begin();
      check(m.pers.setpointC()==sp && m.pers.presetSeconds(1)==60,"valid boundary rejected");
    }
  });
  test("G06 recording commit failure latches E6 and reboot restores old flash", []{
    resetIo(); PersistentConfig cfg=goodConfig(); cfg.presets[0].valid=1; cfg.presets[0].seconds=25;
    cfg.lastPreset=1; seedFlash(cfg); CoffeeMachine m; m.begin();
    m.startupSensorsChecked=true; sensor(m); setupRecord(m); EEPROM.commitOk=false;
    fakeMs=19000; pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,19000);
    check(m.fault==FAULT_CRC && allOff() && EEPROM.commits==1,"record commit failure not latched");
    EEPROM.commitOk=true; CoffeeMachine reboot; reboot.begin();
    check(reboot.pers.presetSeconds(1)==25,"failed commit silently changed durable preset");
  });
  test("G06 setpoint commit failure latches E6", []{
    CoffeeMachine m; init(m); m.state=SETPOINT_EDIT; m.editSetpoint=99.5;
    EEPROM.commitOk=false; m.leaveSetpointEdit(true);
    check(m.fault==FAULT_CRC && allOff(),"setpoint save failure hidden");
  });
  test("G06 completed-dose lastPreset commit failure latches E6", []{
    CoffeeMachine m; init(m); m.state=RUN_ACTIVE; m.currentPreset=1; m.pers.setPreset(1,2);
    m.doseValveOpenMs=1000; m.doseStartMs=uint32_t(1000)-6000;
    EEPROM.commitOk=false; tickAt(m,3000);
    check(m.fault==FAULT_CRC && allOff(),"completed-dose save failure hidden");
  });
  test("G06 repeated same preset completion does not wear flash", []{
    CoffeeMachine m; init(m); m.pers.setPreset(1,2); m.pers.setLastPreset(1);
    for(uint32_t start:{1000u,10000u}) {
      m.state=RUN_ACTIVE; m.currentPreset=1; m.doseValveOpenMs=start; m.doseStartMs=start-6000;
      tickAt(m,start+2000);
    }
    check(EEPROM.commits==0 && EEPROM.puts==0,"unchanged lastPreset wrote flash");
  });
  test("G06 unchanged edit and unchanged record avoid commits", []{
    CoffeeMachine m; init(m); m.editSetpoint=m.pers.setpointC(); m.leaveSetpointEdit(true);
    setupRecord(m); fakeMs=35000; pressEvent(m.runBtn,PIN_RUNB,false); tickAt(m,35000);
    check(EEPROM.commits==0,"unchanged data rewrote EEPROM");
  });
  test("G07 unfiltered overtemp cuts SSR despite cool filter and running pump", []{
    CoffeeMachine m; init(m); sensor(m,100,146); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    writes.clear(); tickAt(m,1100);
    check(pins[PIN_SSR]==LOW && pins[PIN_PUMP]==HIGH && pins[PIN_VALVE]==HIGH && m.fault==FAULT_NONE,
          "raw cutoff missed or incorrectly stopped hydraulics");
    for(const auto& w:writes) if(w.pin==PIN_SSR) check(w.value==LOW,"transient SSR HIGH above raw cutoff");
  });
  test("G07 recovery waits for both raw and filtered temperatures", []{
    CoffeeMachine m; init(m); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    sensor(m,146,140); tickAt(m,1100); check(pins[PIN_SSR]==LOW,"hot filter ignored");
    sensor(m,145,145); tickAt(m,1200);
    check(pins[PIN_SSR]==HIGH && m.fault==FAULT_NONE,"145 boundary did not recover");
  });
  test("G07 real ADC publish crosses raw cutoff before low-pass catches up", []{
    CoffeeMachine m; init(m); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    const float t=150.0f-NTC_CAL_OFFSET_C;
    const float r=NTC_DEFAULT_R0*expf(NTC_DEFAULT_BETA*(1.0f/(t+273.15f)-1.0f/NTC_T0_K));
    const int adc=lroundf(1023.0f*r/(NTC_R_SERIES+r));
    int buf[7]={adc,adc,adc,adc,adc,adc,adc}; m.ntc.publishMedian(buf,7);
    check(m.ntc.controlTemp()<145 && m.ntc.rawTemp()>145,"thermal fixture not straddling cutoff");
    tickAt(m,1100); check(pins[PIN_SSR]==LOW && m.fault==FAULT_NONE,"published raw cutoff missed");
  });
  test("G08 thermostat hysteresis and pump-force behavior", []{
    Thermostat t; t.init(97.5); t.update(97); check(!t.heaterOn(),"ON at lower equality");
    t.update(96.9); check(t.heaterOn(),"did not heat below lower boundary");
    t.update(97.9); check(t.heaterOn(),"deadband not retained");
    t.update(98); check(!t.heaterOn(),"upper equality did not cut");
    CoffeeMachine m; init(m); sensor(m,105,105); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    tickAt(m,1100); check(pins[PIN_SSR]==HIGH,"pump did not force heat below cutoff");
  });
  test("G08 prime finishes after 5s with valve always closed", []{
    CoffeeMachine m; init(m); m.state=STARTUP_PRIME; m.stateEnteredMs=1000;
    tickAt(m,5999); check(pins[PIN_PUMP]==HIGH && pins[PIN_VALVE]==LOW,"prime stopped early");
    tickAt(m,6000); check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,"prime duration wrong");
  });
  test("G08 wet soak press and extraction timer boundaries", []{
    CoffeeMachine m; init(m); m.pers.setPreset(1,2); m.currentPreset=1;
    m.state=RUN_PUMP_PREDELAY; m.doseStartMs=1000;
    tickAt(m,2999); check(pins[PIN_PUMP]==HIGH && pins[PIN_VALVE]==HIGH,"wet phase wrong");
    tickAt(m,3000); check(pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,"soak phase wrong");
    tickAt(m,5000); check(pins[PIN_PUMP]==HIGH && pins[PIN_VALVE]==LOW,"press phase wrong");
    tickAt(m,7000); check(m.state==RUN_ACTIVE && pins[PIN_VALVE]==HIGH,"flow phase wrong");
    tickAt(m,8999); check(m.state==RUN_ACTIVE,"preset counted preamble");
    tickAt(m,9000); check(m.state==HEATING_IDLE && pins[PIN_PUMP]==LOW && EEPROM.commits==1,"dose completion wrong");
  });
  test("G08 run extraction timeout at 60s crosses rollover", []{
    CoffeeMachine m; init(m); m.pers.setPreset(1,60); m.currentPreset=1; m.state=RUN_ACTIVE;
    const uint32_t start=UINT32_MAX-30000; m.doseValveOpenMs=start; m.doseStartMs=start-6000;
    tickAt(m,start+59999u); check(m.state==RUN_ACTIVE,"run timeout early");
    tickAt(m,start+60000u); check(m.state==HEATING_IDLE && pins[PIN_VALVE]==LOW,"wrapped run cutoff missed");
  });
  test("G08 clean start gesture and bounded stop through debounce", []{
    CoffeeMachine m; init(m); m.state=HEATING_IDLE;
    pins[PIN_SETB]=LOW; tickAt(m,1100); tickAt(m,1125);
    pins[PIN_SETB]=HIGH; tickAt(m,1250); tickAt(m,1275);
    check(m.state==CLEAN_FLUSH && pins[PIN_VALVE]==HIGH,"short SET did not start clean");
    pins[PIN_SETB]=LOW; tickAt(m,1400); tickAt(m,1425);
    pins[PIN_SETB]=HIGH; tickAt(m,1550); tickAt(m,1575);
    check(m.state==HEATING_IDLE && pins[PIN_VALVE]==LOW,"valid SET stop failed");
  });
  test("G08 clean ignores too-short stop and expires at 60s", []{
    CoffeeMachine m; init(m); m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
    pins[PIN_SETB]=LOW; tickAt(m,1100); tickAt(m,1125);
    pins[PIN_SETB]=HIGH; tickAt(m,1150); tickAt(m,1175);
    check(m.state==CLEAN_FLUSH,"short stop accepted");
    tickAt(m,61000); check(m.state==HEATING_IDLE && pins[PIN_VALVE]==LOW,"clean timeout missed");
  });
  test("G08 setpoint edit timeout discards changes", []{
    CoffeeMachine m; init(m); m.enterSetpointEdit(); m.editSetpoint=110;
    tickAt(m,20999); check(m.state==SETPOINT_EDIT,"edit expired early");
    tickAt(m,21000);
    check(m.state==HEATING_IDLE && m.pers.setpointC()==97.5f && EEPROM.commits==0,"abandoned edit saved");
  });
  test("G08 both-button edit save commits once and survives reboot", []{
    CoffeeMachine m; init(m); m.enterSetpointEdit(); m.editSetpoint=100.3f;
    pins[PIN_SETB]=LOW; pins[PIN_RUNB]=HIGH;
    tickAt(m,1100); tickAt(m,1125); tickAt(m,1424);
    check(m.state==SETPOINT_EDIT,"both-button save early");
    tickAt(m,1425);
    check(m.state==HEATING_IDLE && m.pers.setpointC()==100.5f && EEPROM.commits==1,"edit not snapped/saved");
    CoffeeMachine reboot; reboot.begin(); check(reboot.pers.setpointC()==100.5f,"edit lost at restart");
  });
  test("G08 heating and READY frames match operation docs", []{
    CoffeeMachine m; init(m); m.state=HEATING_IDLE; tickAt(m,1000);
    for(uint8_t x:displayFrame) check(x==0x80,"heating not blinking 8888");
    sensor(m,96,96); tickAt(m,1100); tickAt(m,1101);
    for(uint8_t x:displayFrame) check(x==uint8_t(~SEGMENT_CODE[0]),"READY not 0000");
  });
  test("G08 NTC fault remains latched after sensor recovers", []{
    CoffeeMachine m; init(m); m.state=HEATING_IDLE; m.ntc.fault=FAULT_NTC_INVALID;
    tickAt(m,1100); sensor(m,96,96); writes.clear(); tickAt(m,1200);
    check(m.state==FAULT_LATCHED && m.fault==FAULT_NTC_INVALID && allOff() && noActuatorHighWrite(),
          "UI/sensor recovery cleared fault");
  });
#include "heat_recovery_cases.h"
#endif
  std::cout<<"RESULT tests="<<total<<" failed="<<failed<<"\n";
  return failed?1:0;
}
