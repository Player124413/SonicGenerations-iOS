# Sonic Generations for Android - native build.
#
# Handed over from Gradle (-D... in app/build.gradle.kts), with fallbacks for a
# direct CMake invocation. Built from this one project:
#   * the ReXGlue SDK (../../ios/rexglue-sdk - the same SDK tree the iOS build
#     uses, whose Android target adds the android_runtime glue, the jthread
#     shim and the mobile performance fixes),
#   * libadrenotools (optional) - custom Turnip/Adreno Vulkan driver loader,
#   * the recompiled guest code (generated from your default.xex),
#   * the app: the shared sources of ios/sonicgenerations/src (frame rate hook,
#     spin waits, the HedgeModManager mod loader) plus the Android pieces.
cmake_minimum_required(VERSION 3.25)
project(sonicgenerations_android LANGUAGES C CXX ASM)

# android/app/src/main/cpp -> repository root is five levels up.
get_filename_component(SG_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(REXSDK_DIR "${SG_REPO_ROOT}/ios/rexglue-sdk" CACHE PATH "ReXGlue SDK tree (shared with the iOS build)")
set(REX_PORT_DIR "${SG_REPO_ROOT}/android/sonicgenerations" CACHE PATH "the recompiled game project (manifest, src, generated)")
set(REX_APP_NAME "sonicgenerations" CACHE STRING "rexglue project name")
# The portable app sources shared with the iOS build.
set(SG_SHARED_APP_DIR "${SG_REPO_ROOT}/ios/sonicgenerations/src")

if(NOT EXISTS "${REXSDK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "ReXGlue SDK not found at ${REXSDK_DIR}")
endif()
if(NOT EXISTS "${REX_PORT_DIR}/generated/default/sources.cmake")
    message(FATAL_ERROR "no generated code at ${REX_PORT_DIR}/generated/default - run android/build.sh (it runs codegen first)")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_USE_D3D12 OFF CACHE BOOL "" FORCE)

# --- mobile performance flags --------------------------------------------------
# Applied to everything below (SDK runtime, GPU plugin, recompiled code):
#   * -O3 + thin LTO across the runtime.
#   * ARMv8.2-A + fp16/dotprod: every 64-bit Android SoC since ~2018 (Snapdragon
#     660+, Exynos 9810+, Kirin 970+, Dimensity/Helio G) has it; the hot loops
#     (byte-swapping guest memory, vector ops emitted for VMX128) benefit.
#     Gated by REX_ANDROID_BASELINE_ARM=1 to fall back to plain armv8-a.
#   * -fno-plt / -fomit-frame-pointer: fewer indirections per call. The
#     recompiled code is millions of small functions calling through the
#     dispatcher; per-call overhead is the CPU bottleneck on phones.
#   * -fno-semantic-interposition / hidden visibility: lets LTO inline across
#     the runtime library instead of going through the GOT.
#
# (@added deivid22srk, 2026 - kept from the upstream Android build.)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  add_compile_options(-O3 -DNDEBUG -fomit-frame-pointer -fno-plt
                      -fno-semantic-interposition -fmerge-all-constants
                      -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables)
  if(NOT DEFINED ENV{REX_ANDROID_BASELINE_ARM})
    add_compile_options(-march=armv8.2-a+fp16+dotprod -mtune=cortex-a76)
  endif()
  add_link_options(-Wl,-O2 -Wl,--gc-sections -Wl,--icf=all -Wl,--as-needed -Wl,-z,now)
endif()

add_subdirectory("${REXSDK_DIR}" rexglue-sdk)

# --- NativeXRenderer: the native renderer (DiligentCore/Vulkan) ------------- #
# Over the game's XDK Direct3D layer instead of emulating the Xenos; added
# after the SDK it reuses (its CMakeLists says so). The submodule's own
# thirdparty/diligent-core is fetched by "git submodule update --init
# --recursive" (android/build.sh does it). The game's hooks and profile go
# into main (below): hooks override the generated weak functions and must not
# sit in an archive.
#
# The SuperResolution module is built but its DLSS path stays off: upscaler.cpp
# needs the module's interface types to instantiate its RefCntAutoPtr members
# (their destructor requires complete types), and Initialize returns false
# without NGX on the device anyway. NativeXRenderer links the module itself
# only when DILIGENT_DLSS_SUPPORTED (a Windows SDK), so the pieces are added
# here.
set(NATIVE_X_RENDERER_DIR "${SG_REPO_ROOT}/native_x_renderer" CACHE PATH
    "NativeXRenderer source tree (a submodule of this repository)")
add_subdirectory("${NATIVE_X_RENDERER_DIR}" native_x_renderer)
target_compile_definitions(native_x_renderer PRIVATE NATIVE_X_RENDERER_DLSS=1)
target_link_libraries(native_x_renderer PRIVATE Diligent-SuperResolution-static)

# --- libadrenotools: custom Turnip/Adreno Vulkan driver loader ---------------
# Ships as libadrenotools.so + its hook libs; android_main.cpp dlopen()s the
# driver through it when the launcher has one configured. Cloned by build.sh
# into android/sdk/libadrenotools (kept out of git).
set(ADRENOTOOLS_DIR "${SG_REPO_ROOT}/android/sdk/libadrenotools" CACHE PATH "libadrenotools checkout")
if(EXISTS "${ADRENOTOOLS_DIR}/CMakeLists.txt")
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory("${ADRENOTOOLS_DIR}" adrenotools)
    set(REX_HAVE_ADRENOTOOLS ON)
else()
    message(WARNING "libadrenotools not found at ${ADRENOTOOLS_DIR} - custom GPU drivers disabled")
endif()

# --- recompiled guest code (codegen output) ---------------------------------
set(REX_GEN_DIR "${REX_PORT_DIR}/generated/default")
include("${REX_GEN_DIR}/sources.cmake")
set(REX_GEN_SOURCES "")
foreach(src ${GENERATED_SOURCES})
    if(IS_ABSOLUTE "${src}")
        list(APPEND REX_GEN_SOURCES "${src}")
    else()
        list(APPEND REX_GEN_SOURCES "${REX_GEN_DIR}/${src}")
    endif()
endforeach()

add_library(recomp STATIC ${REX_GEN_SOURCES})
target_include_directories(recomp PRIVATE "${REX_PORT_DIR}" "${REX_PORT_DIR}/src" "${REX_GEN_DIR}")
target_precompile_headers(recomp PRIVATE "${REX_GEN_DIR}/${REX_APP_NAME}_pch.h")
# The generated PPC->C++ is enormous (hundreds of MB of source): strip debug
# info entirely, keep -O3. (No -fno-exceptions: REX_UNIMPLEMENTED in the
# generated PCH throws.) No RTTI: nothing in it uses dynamic_cast/typeid.
target_compile_options(recomp PRIVATE
    -g0 -O3
    -fno-rtti
    -fno-math-errno -fno-trapping-math
    -Wno-unused-variable -Wno-unused-label -Wno-unused-but-set-variable
    -Wno-unused-value -Wno-parentheses-equality)
# Data-section relocation pressure: the register/init tables are huge.
set_target_properties(recomp PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
target_link_libraries(recomp PUBLIC rex::runtime rexui)

# --- app library loaded by SDLActivity ----------------------------------------
add_library(main SHARED
    android_gamepad.cpp
    android_main.cpp
    android_perf.cpp
    "${REX_PORT_DIR}/src/main.cpp"
    "${REX_PORT_DIR}/src/mod_manager_android.cpp"
    # The portable app sources, shared with the iOS build (unchanged):
    "${SG_SHARED_APP_DIR}/frame_rate.cpp"
    "${SG_SHARED_APP_DIR}/spin_waits.cpp"
    "${SG_SHARED_APP_DIR}/native_x_renderer_profile.cpp"
    "${SG_SHARED_APP_DIR}/native_x_renderer_sonicgenerations.cpp"
    "${SG_SHARED_APP_DIR}/mods/cri_utf.cpp"
    "${SG_SHARED_APP_DIR}/mods/crilayla.cpp"
    "${SG_SHARED_APP_DIR}/mods/cpk_overlay.cpp"
    "${SG_SHARED_APP_DIR}/mods/hedge_archive.cpp"
    "${SG_SHARED_APP_DIR}/mods/ini.cpp"
    "${SG_SHARED_APP_DIR}/mods/mod_database.cpp"
    "${SG_SHARED_APP_DIR}/mods/mod_loader.cpp"
    # ReXApp base class (the SDL entry point is android_main.cpp instead of
    # the SDK's windowed_app_main_sdl.cpp).
    "${REXSDK_DIR}/src/ui/rex_app.cpp"
)
target_include_directories(main PRIVATE
    "${REX_PORT_DIR}" "${REX_PORT_DIR}/src" "${REX_GEN_DIR}"
    "${SG_SHARED_APP_DIR}"
    "${REXSDK_DIR}/include" "${REXSDK_DIR}/src/ui")
target_compile_options(main PRIVATE -Wno-unused-variable)
target_compile_definitions(main PRIVATE
    REXGLUE_BUILD_CONFIG="$<CONFIG>"
    REX_APP_NAME="${REX_APP_NAME}")
target_link_libraries(main PRIVATE recomp rex::runtime rexui native_x_renderer::native_x_renderer)
if(REX_HAVE_ADRENOTOOLS)
    target_compile_definitions(main PRIVATE REX_HAVE_ADRENOTOOLS=1)
    target_link_libraries(main PRIVATE adrenotools)
    add_dependencies(main main_hook file_redirect_hook gsl_alloc_hook hook_impl)
endif()
target_link_options(main PRIVATE -Wl,--export-dynamic)
add_dependencies(main rexgpu-xenos)
set_target_properties(main PROPERTIES CXX_VISIBILITY_PRESET default VISIBILITY_INLINES_HIDDEN OFF)
