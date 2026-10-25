// sonicgenerations - HedgeModManager mod manager on Android.
//
// The list UI is ModManagerActivity (Java); this file owns the mod database
// (src/mods/mod_database.*) behind it and the pause/resume plumbing around it,
// mirroring mod_manager_ios.mm: the game is paused while the manager is open,
// a changed selection offers "Quit Game" (applies on the next launch) or
// "Keep Playing".

#include "mod_manager_android.h"

#include "mods/mod_database.h"

#include <SDL3/SDL.h>
#include <jni.h>

#include <cstdio>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

std::mutex g_mutex;
std::filesystem::path g_root;
std::function<void()> g_done;        // resume callback, set while open
std::function<void()> g_menu_handler;  // the app's MODS button body
std::vector<sg::mods::ModEntry> g_mods;  // the working selection
bool g_in_game = false;

JNIEnv* Env() {
  return static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
}

jobject Activity() {
  return static_cast<jobject>(SDL_GetAndroidActivity());
}

std::string JsonEscape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// {"mods":[{...}], "issue":"..."} for ModManagerActivity.
std::string ScanToJson() {
  g_mods = sg::mods::Scan(g_root);
  std::string json = "{\"mods\":[";
  for (size_t i = 0; i < g_mods.size(); ++i) {
    const auto& mod = g_mods[i];
    if (i) json += ',';
    json += "{\"title\":\"" + JsonEscape(mod.title) + "\"";
    json += ",\"author\":\"" + JsonEscape(mod.author) + "\"";
    json += ",\"version\":\"" + JsonEscape(mod.version) + "\"";
    json += ",\"issue\":\"" + JsonEscape(mod.issue) + "\"";
    json += ",\"note\":\"" + JsonEscape(mod.note) + "\"";
    json += ",\"enabled\":" + std::string(mod.enabled ? "true" : "false") + "}";
  }
  json += "],\"issue\":\"" + JsonEscape(sg::mods::SelectionIssue(g_mods)) + "\"}";
  return json;
}

}  // namespace

void SgSetModMenuHandler(std::function<void()> handler) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_menu_handler = std::move(handler);
}

void SgRunModManager(std::filesystem::path root, std::function<void()> done, bool in_game) {
  JNIEnv* env = Env();
  jobject activity = Activity();
  if (!env || !activity) {
    // No UI to open: just resume.
    if (done) done();
    return;
  }
  jclass cls = env->GetObjectClass(activity);
  jmethodID open = env->GetMethodID(cls, "openModManager", "(Ljava/lang/String;Z)V");
  env->DeleteLocalRef(cls);
  if (!open) {
    // No UI to open: just resume.
    if (done) done();
    return;
  }
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_root = std::move(root);
    g_done = std::move(done);
    g_in_game = in_game;
    g_mods.clear();
  }
  jstring path = env->NewStringUTF(g_root.string().c_str());
  env->CallVoidMethod(activity, open, path, in_game ? JNI_TRUE : JNI_FALSE);
  env->DeleteLocalRef(path);
}

// ---------------------------------------------------------------------------
// JNI entry points (GameModBridge + ModManagerActivity)
// ---------------------------------------------------------------------------

extern "C" {

// The MODS button on the launcher overlay: run the app's handler (pauses the
// game and opens the manager).
JNIEXPORT void JNICALL
Java_com_devz_sonicgenerations_GameModBridge_nativeModButton(JNIEnv*, jclass) {
  std::function<void()> handler;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    handler = g_menu_handler;
  }
  if (handler) handler();
}

// The manager closed: resume the game (done), and quit when the player chose
// "Quit Game" - mods load at startup, so a changed selection applies on the
// next launch.
JNIEXPORT void JNICALL
Java_com_devz_sonicgenerations_GameModBridge_nativeModManagerClosed(JNIEnv*, jclass,
                                                                   jboolean quit) {
  std::function<void()> done;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    done = std::move(g_done);
    g_done = nullptr;
    g_mods.clear();
  }
  if (done) done();
  if (quit) {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
  }
}

JNIEXPORT jstring JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeScan(JNIEnv* env, jclass) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return env->NewStringUTF(ScanToJson().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeImport(JNIEnv* env, jclass,
                                                              jstring jpath) {
  const char* chars = env->GetStringUTFChars(jpath, nullptr);
  std::filesystem::path folder(chars ? chars : "");
  if (chars) env->ReleaseStringUTFChars(jpath, chars);
  std::string error;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!sg::mods::Import(g_root, folder, error)) {
      if (error.empty()) error = "Could not import that folder.";
    }
  }
  return env->NewStringUTF(error.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeSetEnabled(JNIEnv*, jclass,
                                                                  jint index, jboolean on) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (index < 0 || static_cast<size_t>(index) >= g_mods.size()) return JNI_FALSE;
  auto& entry = g_mods[static_cast<size_t>(index)];
  if (!entry.issue.empty() && on) return JNI_FALSE;  // cannot be enabled
  entry.enabled = on == JNI_TRUE;
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeMove(JNIEnv*, jclass, jint from,
                                                             jint to) {
  std::lock_guard<std::mutex> lock(g_mutex);
  const size_t count = g_mods.size();
  if (from < 0 || to < 0 || static_cast<size_t>(from) >= count ||
      static_cast<size_t>(to) >= count) {
    return JNI_FALSE;
  }
  auto entry = g_mods[static_cast<size_t>(from)];
  g_mods.erase(g_mods.begin() + from);
  g_mods.insert(g_mods.begin() + to, std::move(entry));
  return JNI_TRUE;
}

// Saves the selection; returns the selection issue (missing dependency), or "".
JNIEXPORT jstring JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeSave(JNIEnv* env, jclass) {
  std::lock_guard<std::mutex> lock(g_mutex);
  std::string issue = sg::mods::SelectionIssue(g_mods);
  std::string error;
  if (!sg::mods::Save(g_root, g_mods, error) && issue.empty()) {
    issue = error.empty() ? "Could not save the mod selection." : error;
  }
  return env->NewStringUTF(issue.c_str());
}

// True while the manager was opened from the in-game MODS button.
JNIEXPORT jboolean JNICALL
Java_com_devz_sonicgenerations_ModManagerActivity_nativeInGame(JNIEnv*, jclass) {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_in_game ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
