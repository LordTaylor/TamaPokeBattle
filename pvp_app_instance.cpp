#include "pvp_app.h"
#include "pvp_espnow_link.h"
#include "pvp_host_pet.h"
#include "pvp_ui.h"

// Global PvP objects. TamaPoke.ino knows none of these classes; it only calls the
// hooks declared in pvp_ui.h.
namespace {
PvpEspNowLink gLink;
PvpHostPet gHost;
PvpApp gApp(gLink, gHost, gHost);
}  // namespace

PvpApp &pvpAppInstance() { return gApp; }

const PvpProfile &pvpProfileInstance() { return gHost; }

void pvpAppUpdate(uint32_t nowMs) {
  gApp.update(nowMs);
  pvpUiTrack(nowMs);
}

bool pvpAppActive() { return gApp.active(); }

bool pvpAppKeepAwake() { return gApp.keepAwake(); }

bool pvpAppRadioOn() { return gApp.radioOn(); }

void pvpAppForceStop(uint32_t nowMs) { gApp.forceStop(nowMs); }
