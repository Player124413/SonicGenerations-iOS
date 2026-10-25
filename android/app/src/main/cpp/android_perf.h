/**
 * android_perf.h - Android performance glue (thermal-aware frame cap).
 *
 * @added  Sonic Generations Android port, 2026
 */

#pragma once

namespace rexport::perf {

/**
 * Applies a frame-rate cap from the Java thermal listener (degrees of
 * throttling), never raising above the limit the runtime already runs with.
 * status: PowerManager.THERMAL_STATUS_* (0 = none ... 7 = shutdown); the
 * Java side only forwards changes.
 */
void SetThermalStatus(int status);

}  // namespace rexport::perf
