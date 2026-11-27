#ifdef TAMAPOKE_LOCAL_TEST

#include "pvp_diag.h"

#include "pvp_diag_nvs.h"

bool pvpDiagCommand(const String &line) {
  if (line == "NVSSTAT") {
    pvpDiagNvsPrint("now");
    Serial.println("DONE");
    return true;
  }
  if (line == "PVPPING" || line == "PVPPING audio") {
    pvpDiagPing(line == "PVPPING audio");
    return true;
  }
  if (line == "RADIOCYC") {
    pvpDiagCycles();
    return true;
  }
  if (line == "GEOM") {
    pvpDiagGeom();
    Serial.println("DONE");
    return true;
  }
  return false;
}

#endif
