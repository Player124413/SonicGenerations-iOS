#pragma once
#include <filesystem>
#include <functional>

// The mod manager (src/mods/mod_database.h): import, enable, order. The UI is
// ModManagerActivity on the Java side, opened over the game window; done runs
// once the player closes it (with the game resumed). Mods load when the game
// starts, so in game a changed selection offers to quit and apply it on the
// next launch - the same flow as the iOS manager (mod_manager_ios.mm).
void SgRunModManager(std::filesystem::path root, std::function<void()> done, bool in_game);

// Registers the handler behind the launcher's MODS button (called from the
// app class; the Java side invokes it through GameModBridge.nativeModButton).
void SgSetModMenuHandler(std::function<void()> handler);
