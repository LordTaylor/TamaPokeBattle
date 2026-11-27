#pragma once

#ifdef TAMAPOKE_LOCAL_TEST

#include <Arduino.h>

// T4.0 measurement commands, local test build only. Returns true when `line` was one of
// them (NVSSTAT, PVPPING [audio], RADIOCYC, GEOM). Commands block loop() while they run.
bool pvpDiagCommand(const String &line);

void pvpDiagCycles();                 // pvp_diag_cycles.cpp
void pvpDiagPing(bool withAudio);     // pvp_diag_ping.cpp
void pvpDiagGeom();                   // pvp_diag_geom.cpp

#endif
