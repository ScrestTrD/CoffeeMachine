// Included inside review.cpp main(): assert the approved brew/recovery contract.
// H16+ verify monitor state; use the pre-monitor v14 snapshot for reproduction.
  auto prepareDose = [](CoffeeMachine& m, float temp) {
    const uint32_t start=1000u;
    init(m); m.state=RUN_ACTIVE; m.currentPreset=1;
    m.pers.setPreset(1,2); m.pers.setLastPreset(1);
    m.doseValveOpenMs=start; m.doseStartMs=start-6000u;
    sensor(m,temp,temp); m.setActuatorPump(true);
  };
  auto onlySsrWrites = [](int level) {
    for(const auto& w:writes) if(w.pin==PIN_SSR)
      check(w.value==level,"unexpected transient SSR write");
  };
  test("H01 preset keeps heat throughout wet soak press extraction", [&]{
    CoffeeMachine m; init(m); sensor(m,105,105);
    m.state=RUN_PUMP_PREDELAY; m.doseStartMs=1000;
    m.currentPreset=1; m.pers.setPreset(1,25);
    for(uint32_t t:{1000u,2999u,3000u,3001u,4999u,5000u,7000u,7100u}) {
      writes.clear(); tickAt(m,t);
      check(pins[PIN_SSR]==HIGH,"brew heat followed stopped soak pump");
      if(t!=1000) onlySsrWrites(HIGH);
      if(t>=3000 && t<5000) check(pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW,"soak hydraulics changed");
    }
  });
  test("H02 recording keeps heat throughout its pump-off soak", [&]{
    CoffeeMachine m; init(m); sensor(m,105,105);
    m.state=PRESET_RECORD_ACTIVE; m.recordStartMs=1000; m.selectRecordIdx=1;
    for(uint32_t t:{1000u,3000u,3001u,4999u,5000u,7000u}) {
      writes.clear(); tickAt(m,t);
      check(pins[PIN_SSR]==HIGH,"recording soak lost heat"); if(t!=1000) onlySsrWrites(HIGH);
    }
  });
  test("H03 cold completion heats even in old thermostat deadband then stops at set", [&]{
    CoffeeMachine m; prepareDose(m,97.25f); writes.clear(); tickAt(m,3000);
    check(pins[PIN_PUMP]==LOW && pins[PIN_VALVE]==LOW && pins[PIN_SSR]==HIGH,"completion lost recovery heat");
    onlySsrWrites(HIGH);
    sensor(m,97.49f,100); tickAt(m,3001); check(pins[PIN_SSR]==HIGH,"recovery stopped below filtered setpoint");
    sensor(m,97.5f,94); writes.clear(); tickAt(m,3002);
    check(pins[PIN_SSR]==LOW,"recovery waited beyond setpoint"); onlySsrWrites(LOW);
    tickAt(m,3003); check(pins[PIN_SSR]==LOW,"old thermostat demand restarted recovery");
    sensor(m,97.0f,94); tickAt(m,3004); check(pins[PIN_SSR]==HIGH,"post-brew monitor did not restart below set");
    sensor(m,96.99f,94); tickAt(m,3005); check(pins[PIN_SSR]==HIGH,"normal thermostat did not resume");
  });
  for(float temp:{97.5f,97.75f,105.0f}) {
    const std::string label="H04 hot completion turns OFF immediately despite old thermostat ON "+std::to_string(temp);
    test(label.c_str(),[&,temp]{
      CoffeeMachine m; prepareDose(m,temp); m.thermostat.update(80);
      writes.clear(); tickAt(m,3000);
      check(pins[PIN_PUMP]==LOW && pins[PIN_SSR]==LOW,"hot end retained thermostat heat");
      onlySsrWrites(LOW); tickAt(m,3001); check(pins[PIN_SSR]==LOW,"hot handoff re-enabled heat");
    });
  }
  test("H05 warm filter with cold raw follows approved filtered cutoff", [&]{
    CoffeeMachine m; prepareDose(m,103); sensor(m,103,96);
    tickAt(m,3000); check(pins[PIN_SSR]==LOW,"hot filtered end forced extra heat");
    sensor(m,97.2f,94); tickAt(m,3100); check(pins[PIN_SSR]==HIGH,"warm end discarded post-brew monitor");
    sensor(m,96.9f,93); tickAt(m,3200); check(pins[PIN_SSR]==HIGH,"normal thermostat failed to restart");
  });
  struct HeatExit { const char* name; int route; };
  const HeatExit exits[]={
    {"preset SET",0},{"preset RUN hold",1},{"preamble SET",2},{"preamble RUN hold",3},
    {"record SET",4},{"record RUN save",5},{"record timeout",6},{"clean SET release",7},
    {"clean timeout",8},{"preset timeout rollover",9}
  };
  for(const auto& item:exits) {
    const std::string label=std::string("H06 cold recovery after ")+item.name;
    test(label.c_str(),[&,item]{
      CoffeeMachine m; prepareDose(m,97.25f); uint32_t stop=1200;
      m.pers.setPreset(1,25);
      if(item.route==2 || item.route==3) { m.state=RUN_PUMP_PREDELAY; m.doseStartMs=uint32_t(1200)-2500u; }
      if(item.route>=4 && item.route<=6) { setupRecord(m); stop=item.route==6?70000:19000; }
      if(item.route==7 || item.route==8) {
        m.state=CLEAN_FLUSH; m.cleanStartMs=1000;
        if(item.route==7) { m.flushPressStartMs=1000; m.setBtn.enqueue(Button::EV_RELEASED); }
        else stop=61000;
      } else if(item.route==9) {
        const uint32_t start=UINT32_MAX-30000u;
        m.doseValveOpenMs=start; m.doseStartMs=start-6000u; m.pers.setPreset(1,60); stop=start+60000u;
      } else if(item.route==1 || item.route==3) {
        pins[PIN_RUNB]=HIGH; m.runBtn.stable=m.runBtn.lastStable=HIGH;
        m.runBtn.heldFired=true; m.runBtn.lastChangeMs=1000;
      } else if(item.route!=6) {
        fakeMs=stop;
        if(item.route==5) pressEvent(m.runBtn,PIN_RUNB,false);
        else pressEvent(m.setBtn,PIN_SETB,true);
      }
      writes.clear(); tickAt(m,stop);
      check((m.state==READY_IDLE || m.state==HEATING_IDLE) && pins[PIN_PUMP]==LOW &&
            pins[PIN_VALVE]==LOW && pins[PIN_SSR]==HIGH,"exit did not transfer to recovery");
      onlySsrWrites(HIGH);
      sensor(m,97.5,97.5); writes.clear(); tickAt(m,stop+1);
      check(pins[PIN_SSR]==LOW,"exit recovery failed to stop at set"); onlySsrWrites(LOW);
    });
  }
  for(FsmState idleExit:{PRESET_RECORD_READY,PRESET_SELECT_RUN}) {
    const std::string label="H07 selection cancellation does not arm recovery "+std::to_string(int(idleExit));
    test(label.c_str(),[&,idleExit]{
      CoffeeMachine m; init(m); sensor(m,97.25,97.25); m.state=idleExit; m.selectRunIdx=0;
      pressEvent(m.setBtn,PIN_SETB,true); tickAt(m,1100);
      check(pins[PIN_SSR]==LOW,"no-brew cancel armed recovery");
    });
  }
  test("H08 recovery heat stops on first actual filtered ADC publish reaching set", [&]{
    CoffeeMachine m; prepareDose(m,97.25f); tickAt(m,3000);
    check(pins[PIN_SSR]==HIGH,"recovery not entered");
    const float r=NTC_DEFAULT_R0*expf(NTC_DEFAULT_BETA*(1/(100-NTC_CAL_OFFSET_C+273.15f)-1/NTC_T0_K));
    mockAdc=lroundf(1023*r/(NTC_R_SERIES+r));
    for(uint32_t t=3006;t<=3042;t+=6) {
      fakeMs=t; writes.clear(); m.tick();
      const int expected=m.ntc.controlTemp()>=m.thermostat.getSetpoint()?LOW:HIGH;
      check(pins[PIN_SSR]==expected,"heater response lagged filtered publication");
      onlySsrWrites(expected);
    }
    check(m.ntc.controlTemp()>=97.5f && pins[PIN_SSR]==LOW,"ADC fixture did not reach setpoint");
  });
  test("H09 cutoff still wins during soak and recovery", [&]{
    CoffeeMachine m; prepareDose(m,97.25f); m.state=RUN_PUMP_PREDELAY; m.doseStartMs=1000;
    sensor(m,100,146); writes.clear(); tickAt(m,3500);
    check(pins[PIN_PUMP]==LOW && pins[PIN_SSR]==LOW && m.fault==FAULT_NONE,"soak bypassed raw cutoff");
    onlySsrWrites(LOW);
    sensor(m,97.25,145); pressEvent(m.setBtn,PIN_SETB,true); tickAt(m,3600);
    check(pins[PIN_SSR]==HIGH,"cold stop did not recover");
    sensor(m,97.25,146); writes.clear(); tickAt(m,3601);
    check(pins[PIN_SSR]==LOW,"recovery bypassed raw cutoff"); onlySsrWrites(LOW);
    sensor(m,97.25,140); tickAt(m,3602); check(pins[PIN_SSR]==HIGH,"recovery lost after temporary cutoff");
    sensor(m,146,140); writes.clear(); tickAt(m,3603); onlySsrWrites(LOW);
  });
  for(int failure:{1,3,6,8}) {
    const std::string label="H10 recovery remains OFF after fault E"+std::to_string(failure);
    test(label.c_str(),[&,failure]{
      CoffeeMachine m; prepareDose(m,97.25f); tickAt(m,3000);
      check(pins[PIN_SSR]==HIGH,"recovery not entered");
      if(failure==1) m.ntc.fault=FAULT_NTC_INVALID;
      if(failure==6) m.latchFault(FAULT_CRC);
      if(failure==8) m.state=static_cast<FsmState>(255);
      writes.clear(); tickAt(m,failure==3?5000:3001,failure!=3);
      check(allOff() && int(m.fault)==failure && noActuatorHighWrite(),"fault did not lock recovery outputs");
      sensor(m,80,80); writes.clear(); tickAt(m,5100);
      check(allOff() && noActuatorHighWrite(),"sensor recovery cleared fault");
    });
  }
  test("H11 new brew overrides recovery and its hot end remains OFF", [&]{
    CoffeeMachine m; prepareDose(m,97.25f); tickAt(m,3000);
    m.enterState(PRESET_RECORD_ACTIVE); m.recordStartMs=4000; m.selectRecordIdx=1;
    sensor(m,105,105); writes.clear(); tickAt(m,6500);
    check(pins[PIN_PUMP]==LOW && pins[PIN_SSR]==HIGH,"new recording soak inherited idle heat mode");
    pressEvent(m.setBtn,PIN_SETB,true); writes.clear(); tickAt(m,6501);
    check(pins[PIN_SSR]==LOW,"old recovery survived hot new-cycle end"); onlySsrWrites(LOW);
  });
  test("H12 changed setpoint during recovery is applied in the same tick", [&]{
    CoffeeMachine m; prepareDose(m,97.25f); tickAt(m,3000);
    m.enterSetpointEdit(); m.editSetpoint=95.5;
    pins[PIN_SETB]=LOW; pins[PIN_RUNB]=HIGH;
    tickAt(m,3100); tickAt(m,3125); writes.clear(); tickAt(m,3425);
    check(m.pers.setpointC()==95.5 && pins[PIN_SSR]==LOW,"new setpoint did not end recovery");
    onlySsrWrites(LOW);
  });
  test("H13 recovery cutoff works at both setpoint limits", [&]{
    for(float sp:{90.0f,140.0f}) {
      CoffeeMachine m; prepareDose(m,sp-0.25f); m.thermostat.init(sp);
      tickAt(m,3000); check(pins[PIN_SSR]==HIGH,"boundary recovery lost heat");
      sensor(m,sp,sp); writes.clear(); tickAt(m,3001);
      check(pins[PIN_SSR]==LOW,"boundary recovery exceeded setpoint"); onlySsrWrites(LOW);
    }
  });
  test("H14 boot prime still uses normal thermostat after its pump stops", [&]{
    CoffeeMachine m; init(m); sensor(m,97.25,97.25);
    m.state=STARTUP_PRIME; m.stateEnteredMs=1000; tickAt(m,5999);
    check(pins[PIN_SSR]==HIGH,"prime pump no longer forces heat");
    tickAt(m,6000); check(pins[PIN_SSR]==LOW,"prime incorrectly armed brew recovery");
  });
  for(bool recording:{false,true}) {
    test(recording?"H15 record save sees SSR OFF before EEPROM commit":
                   "H15 preset completion sees SSR OFF before EEPROM commit",[&,recording]{
      CoffeeMachine m; prepareDose(m,97.5f); m.thermostat.update(80);
      uint32_t stop=3000;
      if(recording) {
        setupRecord(m); stop=19000; fakeMs=stop; pressEvent(m.runBtn,PIN_RUNB,false);
      } else m.pers.setLastPreset(0);
      EEPROM.onCommit=[]{
        check(allOff(),"flash commit delayed hot-end actuator shutdown");
      };
      writes.clear(); tickAt(m,stop);
      check(EEPROM.commits==1 && allOff(),"save did not complete with SSR OFF");
      onlySsrWrites(LOW);
    });
  }

  test("H16 fresh stable band hands off only after three seconds", [&]{
    CoffeeMachine m; prepareDose(m,103); tickAt(m,3000);
    check(m.recoveringHeat_ && pins[PIN_SSR]==LOW,"hot end discarded monitor");
    sensor(m,97.7f,97.7f);
    for(uint32_t t=3100;t<=6000;t+=100) {
      tickAt(m,t); check(m.recoveringHeat_ && pins[PIN_SSR]==LOW,"early handoff");
    }
    tickAt(m,6100); check(!m.recoveringHeat_ && pins[PIN_SSR]==LOW,"stable handoff missing");
    sensor(m,97.2f,97.2f); tickAt(m,6200);
    check(pins[PIN_SSR]==LOW,"normal thermostat not restored");
  });
  test("H17 dip and overshoot each restart stability confirmation", [&]{
    for(float excursion:{97.2f,98.1f}) {
      CoffeeMachine m; prepareDose(m,97.7f); tickAt(m,3000);
      for(uint32_t t=3100;t<=5900;t+=100) tickAt(m,t);
      sensor(m,excursion,excursion); tickAt(m,6000);
      check(pins[PIN_SSR]==(excursion<97.5f?HIGH:LOW),"wrong excursion demand");
      sensor(m,97.7f,97.7f);
      for(uint32_t t=6100;t<=9000;t+=100) {
        tickAt(m,t); check(m.recoveringHeat_,"old stability time survived");
      }
      tickAt(m,9100); check(!m.recoveringHeat_,"new stability window failed");
    }
  });
  test("H18 reused sample and publication gap cannot certify stability", [&]{
    CoffeeMachine m; prepareDose(m,97.7f); tickAt(m,3000); tickAt(m,3100);
    fakeMs=4500; m.applyHeating(); check(m.recoveringHeat_,"reused sample certified stability");
    sensor(m,97.7f,97.7f); tickAt(m,6100);
    check(m.recoveringHeat_,"publication gap counted as stability");
    for(uint32_t t=6200;t<=9000;t+=100) {tickAt(m,t);check(m.recoveringHeat_,"gap window ended early");}
    tickAt(m,9100); check(!m.recoveringHeat_,"gap window never completed");
  });
  test("H19 stability timer survives millis rollover", [&]{
    CoffeeMachine m; prepareDose(m,97.7f);
    fakeMs=UINT32_MAX-1500u; sensor(m,97.7f,97.7f); m.enterIdleAfterDose();
    const uint32_t start=fakeMs+100;
    for(uint32_t dt=0;dt<3000;dt+=100){tickAt(m,start+dt);check(m.recoveringHeat_,"rollover early handoff");}
    tickAt(m,start+3000); check(!m.recoveringHeat_,"rollover blocked handoff");
  });

  test("H20 falling in-band temperatures cannot certify recovery", [&]{
    for(float drop:{0.49f,0.09f}) {
      CoffeeMachine m; prepareDose(m,100); tickAt(m,3000);
      for(uint32_t dt=0;dt<=3000;dt+=100) {
        const float t=97.99f-drop*float(dt)/3000;
        sensor(m,t,t); tickAt(m,3100+dt);
      }
      check(m.recoveringHeat_,"falling temperature was certified stable");
      sensor(m,97.2f,97.2f); tickAt(m,6200);
      check(pins[PIN_SSR]==HIGH,"falling-window handoff delayed reheat");
    }
  });
  test("H21 quiet noisy plateau can return to thermostat", [&]{
    CoffeeMachine m; prepareDose(m,100); tickAt(m,3000);
    for(uint32_t dt=0;dt<=3000;dt+=100) {
      const float t=dt%200?97.68f:97.72f;
      sensor(m,t,t); tickAt(m,3100+dt);
    }
    check(!m.recoveringHeat_&&pins[PIN_SSR]==LOW,"small noise prevented stable handoff");
  });
  test("H22 rapidly rising band is not a quiet plateau", [&]{
    CoffeeMachine m; prepareDose(m,100); tickAt(m,3000);
    for(uint32_t dt=0;dt<=3000;dt+=100) {
      const float t=97.55f+0.3f*float(dt)/3000;
      sensor(m,t,t);tickAt(m,3100+dt);
    }
    check(m.recoveringHeat_,"large in-band swing certified stability");
  });
  test("H23 lowered setpoint stops heat before flash in both heat modes", [&]{
    for(bool recovery:{false,true}) {
      CoffeeMachine m; init(m);sensor(m,97.25f,97.25f);
      m.thermostat.update(80);m.setActuatorSSR(true);m.state=READY_IDLE;
      if(recovery){m.state=RUN_ACTIVE;m.enterIdleAfterDose();}
      m.enterSetpointEdit();m.editSetpoint=95.5f;
      EEPROM.onCommit=[]{check(pins[PIN_SSR]==LOW,"new setpoint was applied after flash");};
      m.leaveSetpointEdit(true);m.applyHeating();
      check(EEPROM.commits==1&&pins[PIN_SSR]==LOW,"lowered setpoint not saved cold");
    }
  });
  test("H24 failure saving changed setpoint still latches E6", [&]{
    CoffeeMachine m;prepareDose(m,97.25f);tickAt(m,3000);
    m.enterSetpointEdit();m.editSetpoint=95.5f;EEPROM.commitOk=false;
    m.leaveSetpointEdit(true);
    check(m.fault==FAULT_CRC&&allOff(),"setpoint save failure enabled output");
  });
  test("H25 save started near edit timeout uses real debounce and wins", [&]{
    CoffeeMachine m;init(m);sensor(m,100,100);m.state=READY_IDLE;
    m.enterSetpointEdit();m.editSetpoint=95.5f;
    pins[PIN_SETB]=LOW;pins[PIN_RUNB]=HIGH;
    tickAt(m,20850);tickAt(m,20875);tickAt(m,21000);tickAt(m,21175);
    check(m.pers.setpointC()==95.5f&&EEPROM.commits==1,"timeout discarded active save");
  });
  test("H26 SET recording gesture survives both readiness transitions", [&]{
    for(bool warming:{false,true}) {
      CoffeeMachine m;init(m);m.state=warming?HEATING_IDLE:READY_IDLE;
      sensor(m,warming?94.f:96.f,warming?94.f:96.f);pins[PIN_SETB]=LOW;
      tickAt(m,1100);tickAt(m,1125);tickAt(m,2000);
      sensor(m,warming?96.f:94.f,warming?96.f:94.f);tickAt(m,2100);
      tickAt(m,4200);
      check(m.state==PRESET_SELECT_RECORD,"readiness change discarded record gesture");
    }
  });
  test("H27 SET+RUN edit gesture survives thermal readiness change", [&]{
    CoffeeMachine m;init(m);m.state=HEATING_IDLE;sensor(m,94,94);
    pins[PIN_SETB]=LOW;pins[PIN_RUNB]=HIGH;
    tickAt(m,1100);tickAt(m,1125);
    sensor(m,96,96);tickAt(m,2100);tickAt(m,6200);
    pins[PIN_SETB]=HIGH;pins[PIN_RUNB]=LOW;
    tickAt(m,6300);tickAt(m,6325);
    check(m.state==SETPOINT_EDIT,"readiness change discarded edit gesture");
  });
  test("H28 short clean gesture survives thermal readiness change", [&]{
    CoffeeMachine m;init(m);m.state=HEATING_IDLE;sensor(m,94,94);
    pins[PIN_SETB]=LOW;tickAt(m,1100);tickAt(m,1125);
    sensor(m,96,96);tickAt(m,1200);
    pins[PIN_SETB]=HIGH;tickAt(m,1300);tickAt(m,1325);
    check(m.state==CLEAN_FLUSH&&pins[PIN_PUMP]==HIGH,"readiness discarded short SET release");
  });
