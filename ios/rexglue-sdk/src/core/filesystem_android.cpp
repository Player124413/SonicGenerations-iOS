/**
 * @file        rex/core/filesystem_android.cpp
 * @brief       Android filesystem entry points declared in rex/filesystem.h
 *
 * @license     BSD 3-Clause License
 *
 * @remarks     Inherited from Xenia, whose Android build resolved content://
 *              URIs through JNI (ContentResolver.openFileDescriptor). The JNI
 *              bridge now lives in android_runtime.cpp: the app glue hands us
 *              its Activity via rex::SetAndroidApplicationContext() and
 *              content URIs open through its ContentResolver. Ordinary
 *              filesystem access is unaffected: it goes through
 *              filesystem_posix.cpp like every other POSIX target.
 */

#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/main_android.h>

namespace rex {
namespace filesystem {

void AndroidInitialize() {}

void AndroidShutdown() {}

bool IsAndroidContentUri(const std::string_view source) {
  // Scheme test only; recognising the form costs nothing and lets callers
  // report something better than "file not found".
  return source.starts_with("content://");
}

int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode) {
  // Wired to the app's ContentResolver bridge (android_runtime.cpp); the app
  // glue must have called rex::SetAndroidApplicationContext() first. Returns
  // -1 (and callers fall back to ordinary paths) when it is not available.
  const int fd = rex::OpenContentUriFdJavaBridge(uri, mode);
  if (fd < 0) {
    REXLOG_ERROR("content:// open failed (no app context, or the resolver refused it): {}", uri);
  }
  return fd;
}

}  // namespace filesystem
}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
