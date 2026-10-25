#include <rex/platform.h>
#include <rex/platform/dynlib.h>

static_assert(REX_PLATFORM_LINUX || REX_PLATFORM_MAC, "This file is POSIX-only");

#include <dlfcn.h>

#include <cstdint>
#include <cstdlib>

namespace rex::platform {

DynamicLibrary::~DynamicLibrary() {
  Close();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

#if REX_PLATFORM_ANDROID
// A Vulkan handle opened by the host app through libadrenotools (custom
// Turnip/Adreno driver from the launcher) is published in REX_VULKAN_HANDLE as
// a decimal pointer. Every "libvulkan.so" load in the process then reuses it,
// so the GPU plugin talks to the replacement driver without knowing about it.
// The handle is owned by the app: never dlclose'd here.
static void* BorrowedVulkanHandle() {
  const char* v = std::getenv("REX_VULKAN_HANDLE");
  if (!v || !*v) return nullptr;
  return reinterpret_cast<void*>(static_cast<uintptr_t>(std::strtoull(v, nullptr, 10)));
}
static bool IsBorrowed(void* h) { return h && h == BorrowedVulkanHandle(); }
#endif

bool DynamicLibrary::Load(const std::filesystem::path& path, SymbolResolution mode) {
  Close();
  int flags = (mode == SymbolResolution::kImmediate) ? RTLD_NOW : RTLD_LAZY;
#if REX_PLATFORM_ANDROID
  if (path.filename() == "libvulkan.so") {
    if (void* borrowed = BorrowedVulkanHandle()) {
      handle_ = borrowed;
      return true;
    }
  }
#endif
  handle_ = dlopen(path.c_str(), flags);
  return handle_ != nullptr;
}

void DynamicLibrary::Close() {
  if (handle_) {
#if REX_PLATFORM_ANDROID
    if (!IsBorrowed(handle_))
#endif
      dlclose(handle_);
    handle_ = nullptr;
  }
}

void* DynamicLibrary::GetRawSymbol(const char* name) const {
  if (!handle_)
    return nullptr;
  return dlsym(handle_, name);
}

}  // namespace rex::platform
