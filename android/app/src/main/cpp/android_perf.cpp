/**
 * android_perf.cpp - Android performance glue (thermal-aware frame cap).
 *
 * A phone that runs hot throttles its GPU mid-level and the frame rate dives
 * unpredictably. The launcher (PerfManager.java) reports the OS thermal
 * status; this file turns it into a framerate_limit cvar drop (the cvar is
 * read every frame by the command processor, so the cap applies live), and
 * lifts it again once the device cools down. The cap never rises above the
 * limit the session started with, so sg_frame_rate = "original" (30) or a
 * user's settings.txt cap stay respected.
 *
 * @added  Sonic Generations Android port, 2026
 */

#include "android_perf.h"

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

namespace rexport::perf {

namespace {

std::mutex g_mutex;
int g_base_limit = 0;      // the session's own limit (0 = uncapped)
int g_thermal_limit = 0;   // cap from thermal status (0 = none)
bool g_known = false;

void ApplyLocked() {
  int limit = g_base_limit;
  if (g_thermal_limit > 0) {
    limit = (limit > 0) ? std::min(limit, g_thermal_limit) : g_thermal_limit;
  }
  rex::cvar::SetFlagByName("framerate_limit", std::to_string(limit));
  REXLOG_INFO("perf: thermal cap {} (base {}, thermal {})", limit, g_base_limit, g_thermal_limit);
}

}  // namespace

void SetThermalStatus(int status) {
  // PowerManager.THERMAL_STATUS_NONE/LIGHT: full speed. MODERATE: 45 FPS.
  // SEVERE and above: 30 FPS (the console's own rate).
  const int cap = (status >= 3) ? 30 : (status == 2) ? 45 : 0;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_known) {
    g_known = true;
    g_base_limit = std::atoi(rex::cvar::GetFlagByName("framerate_limit").c_str());
  }
  if (cap == g_thermal_limit) {
    return;
  }
  g_thermal_limit = cap;
  ApplyLocked();
}

}  // namespace rexport::perf

extern "C" {

JNIEXPORT void JNICALL
Java_com_devz_sonicgenerations_PerfManager_nativeSetThermalStatus(JNIEnv*, jclass, jint status) {
  rexport::perf::SetThermalStatus(status);
}

}  // extern "C"
