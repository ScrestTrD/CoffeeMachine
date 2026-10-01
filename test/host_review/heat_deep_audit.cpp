// Deep audit: production controller properties and real ADC filter response.
// Reuses the established mock fixture; no physical thermal model is asserted.
#define main existing_gate_main
#include "review.cpp"
#undef main
int main() {
 total=failed=0;
 test("A01 brew demand independent of pump across safe temperature grid",[]{
   for(auto state:{RUN_PUMP_PREDELAY,RUN_ACTIVE,PRESET_RECORD_ACTIVE,CLEAN_FLUSH})
    for(float t:{20.f,96.9f,97.f,97.25f,97.5f,105.f,145.f})
     for(bool pump:{false,true}) {
      CoffeeMachine m; init(m); m.state=state; sensor(m,t,t);
      m.setActuatorPump(pump); m.applyHeating();
      check(pins[PIN_SSR]==HIGH,"safe brew lost heat request");
     }
 });
 test("A02 all brew exits recover below set regardless of old thermostat latch",[]{
   for(auto state:{RUN_PUMP_PREDELAY,RUN_ACTIVE,PRESET_RECORD_ACTIVE,CLEAN_FLUSH})
    for(float t:{20.f,96.9f,97.f,97.25f,97.49f,97.5f,105.f})
     for(bool old:{false,true}) {
      CoffeeMachine m; init(m); m.state=state; sensor(m,t,t);
      m.thermostat.heaterOn_=old; m.setActuatorPump(false);
      m.enterIdleAfterDose();
      check((pins[PIN_SSR]==HIGH)==(t<97.5f),"wrong end demand");
     }
 });
 test("A03 hot filtered end retains monitoring even with cold raw",[]{
   CoffeeMachine m; init(m); m.state=RUN_ACTIVE; sensor(m,103,90);
   m.enterIdleAfterDose(); check(pins[PIN_SSR]==LOW&&m.recoveringHeat_,"expected approved hot end");
   for(float t:{99.f,97.5f,97.25f,97.f}) {
    sensor(m,t,85); m.applyHeating(); check(pins[PIN_SSR]==(t<97.5f?HIGH:LOW),"post-brew threshold incorrect");
   }
   sensor(m,96.99f,85); m.applyHeating(); check(pins[PIN_SSR]==HIGH,"idle failed to restart");
 });
 test("A04 one target crossing retains monitoring; subsequent fall reheats",[]{
   CoffeeMachine m; init(m); m.state=RUN_ACTIVE; sensor(m,96,90);
   m.enterIdleAfterDose(); check(pins[PIN_SSR]==HIGH,"no recovery");
   sensor(m,97.5f,95); m.applyHeating(); check(m.recoveringHeat_&&pins[PIN_SSR]==LOW,"target not cleared");
   sensor(m,97.2f,90); m.applyHeating(); check(pins[PIN_SSR]==HIGH,"monitor failed to reheat");
 });
 test("A05 overtemperature blocks heat while brew pump remains on",[]{
   for(bool rawHigh:{false,true}) {
    CoffeeMachine m; init(m); m.state=RUN_ACTIVE; m.setActuatorPump(true);
    sensor(m,rawHigh?90.f:145.1f,rawHigh?145.1f:90.f); m.applyHeating();
    check(pins[PIN_SSR]==LOW&&pins[PIN_PUMP]==HIGH,"cutoff behavior changed");
    sensor(m,90,90); m.applyHeating(); check(pins[PIN_SSR]==HIGH,"cutoff did not release");
   }
 });
 test("A06 real ADC cooling step: measured software delay at several loop cadences",[]{
   for(unsigned cadence:{6u,20u,100u}) {
    CoffeeMachine m; init(m); m.state=RUN_ACTIVE; sensor(m,103,103);
    int best=0; float error=1000;
    for(int adc=7;adc<1017;++adc) {
     float t=m.ntc.resistanceToTempC(m.ntc.adcToResistance(adc))+NTC_CAL_OFFSET_C;
     if(std::isfinite(t)&&fabs(t-96)<error){error=fabs(t-96);best=adc;}
    }
    mockAdc=best; m.enterIdleAfterDose(); m.ntc.sampleIdx=0; m.ntc.lastSampleMs=fakeMs;
    auto start=fakeMs; unsigned ticks=0;
    while(pins[PIN_SSR]==LOW && ticks++<200) {fakeMs+=cadence;m.tick();}
    check(pins[PIN_SSR]==HIGH&&m.fault==FAULT_NONE,"real sample failed to restart");
    std::cout<<"TRACE cadence_ms="<<cadence<<" restart_ms="<<fakeMs-start
     <<" raw="<<m.ntc.rawTemp()<<" filtered="<<m.ntc.controlTemp()<<"\n";
   }
 });
 std::cout<<"TOTAL "<<total<<" FAILED "<<failed<<"\n";
 return failed?1:0;
}
