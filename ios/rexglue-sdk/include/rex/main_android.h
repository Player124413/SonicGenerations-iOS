/**
 * @file        rex/main_android.h
 * @brief       Android runtime environment queries and app-context plumbing
 *
 * @license     BSD 3-Clause License
 * @remarks     threading_posix.cpp and memory_posix.cpp carry Android blocks
 *              inherited from Xenia that gate dlsym lookups on the device API
 *              level (pthread_getname_np and ASharedMemory_create are both
 *              API 26+). Xenia sourced this from its own JNI bootstrap; the NDK
 *              exposes it directly from API 24 on, so no JNI is involved.
 *
 *              SetAndroidApplicationContext and the ContentResolver fd bridge
 *              come from the Android port support by deivid22srk, 2026
 *              (android_runtime.cpp); the app's JNI glue calls the former
 *              before any runtime subsystem starts.
 */

#pragma once

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

#include <android/api-level.h>

#include <filesystem>
#include <string_view>

namespace rex {

/// API level of the device the process is running on, or -1 if unavailable.
/// This is the *device* level, not the compile-time __ANDROID_API__ target.
inline int GetAndroidApiLevel() {
  return android_get_device_api_level();
}

// Provided by the app's JNI glue before runtime startup:
//  - java_vm:         JavaVM* (used to attach native threads for JNI calls)
//  - android_context: jobject of the foreground Activity (global ref taken)
//  - native_library_dir: ApplicationInfo.nativeLibraryDir, where the packaged
//    .so files live (used by GetExecutableFolder() and the GPU plugin loader)
void SetAndroidApplicationContext(void* java_vm, void* android_context,
                                  const char* native_library_dir);

// nativeLibraryDir backing GetExecutableFolder() on Android (may be empty
// before the app context is set).
const std::filesystem::path& GetAndroidNativeLibraryDir();

// Opens a content:// URI through the activity's ContentResolver (the Java
// helper is MainActivity.openContentFd). Returns a detached fd, or -1.
int OpenContentUriFdJavaBridge(std::string_view uri, const char* mode);

// Verifies SetAndroidApplicationContext() ran; for startup assertions.
bool InitializeAndroidSystemForApplicationContext();

// Releases the JNI global refs taken by SetAndroidApplicationContext().
void ShutdownAndroidSystem();

}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
