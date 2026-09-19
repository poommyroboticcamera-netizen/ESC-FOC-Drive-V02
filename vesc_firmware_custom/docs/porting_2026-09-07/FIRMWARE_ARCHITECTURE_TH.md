# VESC dependency analysis และ minimal firmware architecture

Analysis snapshot: local `vedderb/bldc` HEAD4c111e2c5dcd2108a2c1029181940aa7f6d04dbd รวมworking-treechangesที่มีอยู่ก่อนเริ่มงาน. Existing `motor/mc_interface.c`, `motor/mcpwm.c`, `motor/mcpwm.h`, `motor/mcpwm_foc.c`, `terminal.c` ถูกแก้มาก่อนแล้ว;งานนี้ไม่แก้ไฟล์เหล่านั้นและไม่ใช้binaryเก่าเป็นผลtargetใหม่

## PHASE 4 — Dependency map ก่อน copy source

| VESC MODULE | PURPOSE | KEEP | REWRITE | REMOVE | DEPENDENCIES / boundary |
|---|---|---|---|---|---|
| `motor/mcpwm_foc.c/.h` | acquisition,timing,currentPI,mode dispatch | selected algorithms afterbring-up | splitHAL/loop/state | HFI,dual-motor,encoderbranches initially | mc_interface,ch,hal,STM32,digital_filter,utils_math/sys,ledpwm,terminal,encoder,commands,timeout,timer,virtual_motor,foc_math; not standalone |
| `motor/foc_math.c/.h` | observer,PLL,SVM,speedPID | candidateobserver/PLL/SVM | scalarinput/stateinterfaces | position,HFI,FW,Hall/encoderinitially | hw.h,utils_math,datatypes and motor_all_state_t; evenmathfileisnotpure |
| `motor/mc_interface.c/.h` | commandlimits,faults,telemetry | protectionconcepts | smallstatecontroller | six-stepdispatch,multimotor,BMS/appcoupling | mcpwm,mcpwm_foc,hw,ch/hal,commands,encoder,buffer,comm_can,shutdown,app,mempools,crc,bms,events,timer,confgenerator |
| `hwconf/hw.c/.h` | macro-selectedboardsanddriverdefaults | referencepin/equationevidence | typedboardHAL/capabilities | unrelatedboards/DRV83xx | conf_general,utils_math,HW_SOURCE,HW_HEADER; defaultgate/faultmacroscanhideabsenthardware |
| `hwconf/makerbase/75_100/*old*` | V1selection | referenceonly | customboardidentity/constants | no_limits target | sharedcoreheaderwithnewervariant;donotaccidentallyenablephasefilters |
| `conf_general.c/.h` | configstorage,defaults,parameterdetect | deadtimeencodingconcept | versionedconfig+CRC+rangevalidation | auto-detectionbeforebasicFOC | EEPROM,motorinterfaces,app,worker,confgenerator,mempools,commands,terminal,hw |
| `datatypes.h` | globalconfig/protocol/state | selecteddefinitionsifneeded | smallmotor_config/state/faulttypes | broadapp/loggingtypes | directlyincludesch.h;do notcopywholeheader |
| `util/utils_math.*` | transforms,anglewrap,saturation | selectedpurefunctions | removehardware/globalstate | unusedmathutilities | auditfunction-leveltransitivecalls |
| `terminal.c` | debuganddestructivetestcommands | neededobservability | boundeddiagnosticprotocol | legacycommands/detectioncommandsinitially | motorinterfaces,commands,comm_can,app,USB,encoder,mempools,metadata |
| `comm/packet.*`,buffer,crc | VESCframing/serialization | onlyifOptionAselected | commandwhitelist | firmwareupdate/configwriteinitially | commandsdispatchislarge;packetparseraloneisnotToolcompatibility |
| `comm/comm_can.*` | CANtransport+VESCnetwork | transportconcept | smallCANdriver+fixedprotocol | UAVCAN/BMS/remoteupdate/apphooks | channelconfiguration,buffer/commands,timeout |
| ChibiOS3.0.5 +STM32StdPeriph | RTOS,startup,registerHAL | viablelaterplatform | narrowexplicitbuildselection | unrelatedOSports/drivers | licensingpercomponent;preserveIRQpriority/DMA/CCMconstraints |
| `main.c`,Makefile,`make/fw.mk` | fullapplicationstartup/build | toolchain/linkerlessons | explicitminimaltranslationunitlist | repository-wideautoselection | fullVESCstartupbringsmanyunrelatedsubsystems |
| LispBM | scripting | no | no | yes fromnewtarget | VM/extensions/runtime |
| NRF/LoRa/IMU/GPS | externaldevices | no | no | yes fromnewtarget | notpresentcustomhardware |
| Appframework | throttle/controlapps | noneinitially | onecommandarbiter | yesfullframework | protocol→validatedcommands→state |
| logging/QML/virtualmotor | UI/storage/simulation | hosttestconceptonly | smalltelemetryringifneeded | yesfirmwareinitially | nofastloopprintf/filesystem |
| watchdog/timeout | fail-stop/deadman | requiredconcept | localboundedimplementation | no | resetcause,pwmkill,monotonicclock |
| bootloader | upgrade | optionalfuture | explicitflashlayout | omitinitially | SWDflashstart0x08000000;notVESCbootloaderimage |
| motorparameterdetection | R/L/flux/Hall | futureonly | lowenergyteststates | excludeduntilbasicFOCpasses | energizedtests,accurateADC/timing,knownlimits |

ExistingFOCsourcefacts: `timer_reinit`~line169, tripleADC/DMAsetup~405, `mcpwm_foc_adc_int_handler`2879, currentoffset/scaling/reconstruction~3056, observercalls~3464/3710, PLL~3837, `stop_pwm_hw`~5220. `foc_math.c`: observer26,PLL225,SVM246,speedPID492,fieldweakening708. Line numbers belong to auditedworkingtreeandcanmove

## Implementation decision

Deliver **boot_debug** as an independent smalltargetunder `minimal_port/boot_debug`, with an explicitlylistedbuild. ThisfirsttargetusesfreestandingC/registeraccess andSWDmailbox, noRTOS/USB/UART/CANstack becauseitsjobisphase5–6only. ItdoesnotcopyVESCsource. ThisdoesnotdecidethatfutureFOCmustbebare-metal: afterhardwarevalidation, narrowChibiOS+Cortex-M4F+STM32peripheralHAL remainsavalidwaytoreuseVESCtimingwithlessrisk. Do notmixCubeHALandChibiOSinterruptownershipcasually

Boot target deliberately has noADC/PWMalgorithm/observerfakeimplementations. Its gateenable returns `UNSUPPORTED_HW`, faulttelemetryreturns `UNAVAILABLE`, and neitherPONGnorarmrequestcanstartmotor. BenchbootitselfneedsHW01resolved. SuccessfulcompileisnotMCUbootvalidation

## Target architecture for later phases (specification, not implemented motor firmware)

```
board/pins + platform(clock,GPIO,IRQ,DMA)
    → adc / pwm / current_sense / gate_driver / temperature
    → foc_math + current_controller + svpwm + observer
    → motor_control state machine + speed_controller
    → protection + command arbiter + telemetry
```

Planned folders follow userrequest: `/board`, `/drivers`, `/motor`, `/control`, `/protection`, `/communication`, `/config`, `main.c`. HALprovides timestampedrawtriplets,validitymask,physicalVBUS/temperaturevalidflags,atomicPWMupdate,andforceoff. FOCreceivescalibratedcurrents/voltage/angle/dtandreturnsvectors; itdoesnotknowEG3112orGPIOaddresses

`motor_config_t`retainstheuser'sfields: motor_current_max/min,battery_current_max/min,voltage_min/max,rpm_max,duty_max,switching_frequency,motor_R,motor_L,motor_flux,motor_pole_pairs,current_kp/ki,speed_kp/ki. Addschema/version/CRC,separateregenlimits,ADCcalibration,hardwarecapabilitiesandexplicitvalidflags. RejectNaN/Inf,nonpositiveR/L/fluxwhenrequired,invalidpolepairs,gains/frequency/dutyoutsideverifiedranges. SeparateelectricalRPMfrommechanicalRPM: erpm=mechanicalRPM×polepairs

### State machine

| State | Entry/action | Allowed exit |
|---|---|---|
| MOTOR_DISABLED | allgatesoff,clearcommands | CALIBRATINGonlywithverifiedhardwareandsensors |
| MOTOR_CALIBRATING | zero-currentPWMoffsamplemean/stddev;checkoffset/noise | READYifvalid;otherwiseFAULT_CURRENT_SENSOR |
| MOTOR_READY | driverstilloff,commandszero | ALIGNMENTonlyonfresharmandvalidbus/temp/config |
| MOTOR_ALIGNMENT | lowId,time/currentlimited | OPEN_LOOPorFAULT |
| MOTOR_OPEN_LOOP | rampangle/frequency,currentlimited | TRANSITIONonlywithobserverconfidence |
| MOTOR_TRANSITION | blendwrappedanglewithoutjump,hysteresis/dwell | CLOSED_LOOPiflockholds;controlledfallbackorFAULT |
| MOTOR_CLOSED_LOOP_FOC | currentloop;speedloopoptional | DISABLED/FAULT;neverautoresumeafterreset |
| MOTOR_FAULT | immediateforceoff,latchedcause,snapshot | explicitclearwhilecommandzeroandcausegone;thenDISABLED |

Anyfault/disarm/timeoutpreemptsanystate. Detectionstatesareaddedonlyafterbasicclosedloopworks. Alignment/openloopareenergizedtests, notimplementedbeforehardwarebring-up. Observerconfidenceusesfinitefluxstate,fluxmagnitude,PLLresidual,speedconsistencyandminimumbackEMFoveradwelltime; PLLfinitevaluealoneisnotlock. Fluxobserverisnotreliablyobservableatstandstill. StartuprequiresmotorR/L/fluxandload-awarecurrentramps,notarbitraryconstants

### FOC loops and limits

FastloopatavalidPWMwindow: acquirecoherentADC→offset/gain/sign→validity/reconstruct→Clarke→Park→Id/Iqerrors→PIwithantiwindup→vectorvoltagelimitusingmeasuredVBUS→inversePark→SVPWM→boundedshadowupdate. Observermustusethevoltageactuallyappliedintheprecedinginterval,withconsistentdelay/deadtimecompensation; estimatesinwrongtimestamporderbiasflux

Chooseonescalingconvention: amplitude-invariantClarke `iα=iu`, `iβ=(iu+2iv)/sqrt(3)` when balanced. Parkusesconsistentpositiveelectricalrotation. InlinearSVPWMvoltagemagnitude≤VBUS/sqrt(3), thenreserveheadroomforbootstrapandsampling. Anti-windupmustaccountforvectorclampingaswellascurrentcommandlimits. Idinitially0outsidealignment;fieldweakeningoff

CURRENTmode→Iq/Idreferences. DUTYmode→boundedvoltage/modulationwithcurrentoverride,notunprotectedrawtimerwrites. SPEEDmode→slowPI(e.g.1kHzcandidate)→Iqreference,withantiwinduptrackingavailablephase/battery/regenlimits. OPEN_LOOPmode→boundedangle-rampwithlowcurrentonly. Numericalspeeds/gainsremainunsetuntilmotorandtimingmeasurements

Batterycurrentestimateforcontrolmayuseaveragepower: `(3/2)(vd×id+vq×iq)/VBUS` underthesametransformconvention; accountforlosses/dynamicenergy. ItisnotadedicatedBUS_OVER_CURRENTmeasurement. Keep phase_current_max, battery_current_max, regen_phase_current_max and regen_battery_current_max separate

Regen: softOV→progressivelyreduceallowednegativeIq/batterycurrentbeforehardOV; considerbattery/BMSacceptanceandregenerativeenergy. Turningallgatesoffdoesnotguaranteebusenergystopsbecausebodydiodescanrectifyaspinningmotor. Hardwareenergysink/contactorstrategyisaseparatedesignitem; neverclaimfirmware-onlyOVcontrolwithoutphysicalVBUSmeasurement

### Protection ownership

Fastpath: comparator/BKINifadded,ADCstale/overrun,railing,instantphaseOC,nonfiniteFOC,deadlineoverrun→forceoff. Slowpath: thermalderating/OT,UV/OVsupervision,observerconfidence/stall,speedlimit,commandtimeout. Faultsnapshotincludesrawcurrents,validity,VBUSvalid,timestamp,state,appliedduty,resetcause; noheap/printfinISR. Watchdogfedonlywhenfast/slowloopheartbeatsarebothhealthy. HardFault,NMIandunexpectedISRkillPWMfirst. BusOC/motorOT/driverfaultmustreportUNAVAILABLEwhenhardwarecannotmeasurethem,notinventnormalreadings

## Communication options

| | Option A: minimum VESC Tool protocol | Option B: small UART/CAN protocol |
|---|---|---|
| Reuse | packetframing,CRC,serialization;selectedGET_VALUES | tinyboundeddecoder,fixedversion/schema |
| Complexity | medium/high:Toolversionhandshake,commandIDs,scales,selectedvalueslayout,transportandtimeout | lowerinitially;owndebugviewerrequired |
| Risks | claimingcompatibilitywithouttestingrealTool;accidentalconfig/update/detectioncommands | unit/versionmismatch,stalecommands;solvewithlength/range/sequence/timeout |
| Hardware | USBrouteexistsbutneedsQA;CANalsoexists | CAN1routed;UARTrequiresconfirmedtestwires |
| Decision | deferuntilmotorcorestable | recommendedinitialmotortransport;SWDmailboxforbootstage |

ProposedCANmessagesseparatecommandsandtelemetry, carrysequence/status/measurementvalidbits; whitelistsupportedcommandsanddenyenergizingcommandswhileunsupportedhardwareflagsset. CANterminationR84fixed120Ωmustmatchnetworkends. FrameCRCprovidedbyCANdoesnotreplaceapplicationsequencing/watchdog. NoexternalCANcommunicationisimplementedintheboot-onlymilestone

## Code reuse / provenance

NoVESCmotoralgorithmhasbeencopiedintonewsourceinthismilestone. Futureextractionmustrecordsourceproject,commit,file,function,modificationsandlicenseforeachunit. VESCheadersspecifyGPLv3-or-later;retaincopyright/licenseandprovidecorrespondingsourceunderapplicabletermswhenredistributingderivedfirmware. ChibiOS/ST/CMSIScomponentsneedtheirownnotices;do notassumeeverythirdpartylibraryhasthesamelicense. Thisisaprovenanceplan,nolegalstatusisclaimedforanuncreatedmotorbinary

| SOURCE FILE | PROJECT / ORIGINAL FUNCTION | MODIFICATION planned | REUSE NOW |
|---|---|---|---|
| motor/foc_math.c | vedderb/bldc:foc_svm | explicitnormalizationandPWMHALreturn | none |
| motor/foc_math.c | foc_observer_update,foc_pll_run | reducedstate/config,hosttests,finite/confidencechecks | none |
| motor/mcpwm_foc.c | control_current,stop_pwm_hw,ADCISR | splitpurecontrolfromhardware,explicitvalidity | none |
| conf_general.c | deadtimecalculation/parameterdetect | retainencodingidea;deferenergizeddetection | none |
| hwconf/makerbase/75_100 | hw_init_gpio,hw_setup_adc_channels | remappedcapabilitiesandscalesfromcustomevidence | referenceonly |
