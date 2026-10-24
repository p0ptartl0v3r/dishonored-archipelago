#include "game_hooks.hpp"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <map>
#include <unordered_set>
#include <vector>

#include "log.hpp"
#include "memory.hpp"
#include "ue3.hpp"

namespace ap {
namespace {

// UObject::ProcessEvent(UFunction*, void* params, void* result) is __thiscall.
// A __fastcall function with a dummy EDX argument has the same calling convention
// and, unlike __thiscall, can be written as a free function.
using ProcessEventFn = void(__fastcall*)(ue3::UObject* self, void* edx, ue3::UFunction* function, void* params,
                                         void* result);

// Byte pattern for ProcessEvent's prologue (credit: ectrc/dismod).
constexpr const char* kProcessEventPattern =
    "55 8B EC 6A ?? 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 83 EC ?? A1 ?? ?? ?? ?? 33 C5 89 45 ?? 53 56 57 50 8D 45 ?? 64 "
    "A3 ?? ?? ?? ?? 8B F1";

struct ConsoleCommandParams {
  ue3::FString command;
  uint32_t write_to_log;
  ue3::FString return_value;
};
static_assert(offsetof(ConsoleCommandParams, return_value) == 0x10);

Config g_config;
ProcessEventFn g_original = nullptr;

std::mutex g_queue_mutex;
std::deque<std::string> g_queue;
std::atomic<bool> g_queue_pending = false;

std::atomic<bool> g_engine_ready = false;
// Looked up on the game thread (the object table can change under another thread).
std::atomic<ue3::UFunction*> g_console_command = nullptr;
ue3::UObject* g_engine = nullptr;  // lives as long as the process
ue3::UObject* g_cheats_tried_for = nullptr;  // controller we last tried EnableCheats on
ue3::UObject* g_cheat_manager_class = nullptr;  // Class DishonoredGame.DishonoredCheatManager
DWORD g_last_cheats_try = 0;
bool g_cheats_reported = false;
std::atomic<bool> g_cheats_ready = false;
DWORD g_last_resolve_tick = 0;
int g_resolve_attempts = 0;
DWORD g_last_check_tick = 0;
std::atomic<bool> g_in_gameplay = false;
// Set when the game starts quitting: from then on the hook only passes calls
// through, so nothing reads objects the engine is tearing down (a crash on exit
// stops Steam syncing saves to the cloud).
std::atomic<bool> g_shutting_down = false;
// The main menu's background scene has a player pawn too, so "has a pawn" isn't
// enough. The game starts in the menu; mapchange Dishonored_MainMenu returns to it;
// gameplay that resumes after a loading screen (pawn lost, then regained) is a real level.
std::atomic<bool> g_in_menu = true;
bool g_ever_lost_gameplay = false;
// Starting a new game from the menu loads the level without the player ever disappearing,
// so a mapchange to a real level while in the menu arms this; gameplay starts once the
// menu's controller/pawn has been replaced (or after a timeout).
bool g_menu_exit_pending = false;
DWORD g_menu_exit_tick = 0;
ue3::UObject* g_menu_controller = nullptr;
ue3::UObject* g_menu_pawn = nullptr;
// A loaded save restores its own state for a moment after the pawn appears and would
// overwrite anything granted earlier, so commands wait this long after gameplay starts.
constexpr DWORD kSettleMs = 3000;
thread_local bool g_inside_hook_work = false;

// Names (FName indices) of the functions that signal check-relevant events.
struct EventNames {
  int32_t player_pawn = -1;                // class DishonoredPlayerPawn
  int32_t upgrade_tweaks = -1;             // class DisTweaks_Upgrade
  int32_t destroyed = -1;                  // Actor.Destroyed: a pickup was taken (or removed on load)
  int32_t client_prepare_map_change = -1;  // PlayerController.ClientPrepareMapChange(LevelName, ...)
  int32_t on_add_abstract_item = -1;       // DishonoredPawn.OnAddAbstractItem: scripted item reward
  int32_t on_objective_action = -1;        // DishonoredPlayerController.OnObjectiveAction
  int32_t pre_exit = -1;                   // GameInfo.PreExit: the game is quitting
  // Player pawn handlers that level scripts (Kismet) use to give things.
  int32_t on_add_power = -1;
  int32_t on_give_upgrade = -1;
  int32_t on_give_pickup = -1;
  int32_t on_add_inventory_item = -1;
  int32_t pickup_base = -1;                // class DisPickup_Base
  int32_t abstract_item_pickup = -1;       // class DisAbstractItemPickup
  int32_t whale_bone_charm = -1;           // class DisWhaleBoneCharm
};
EventNames g_names;
DWORD g_gameplay_started_tick = 0;

std::mutex g_events_mutex;
std::vector<std::string> g_events;

std::atomic<bool> g_call_logging = true;
std::mutex g_seen_mutex;
std::unordered_set<ue3::UFunction*> g_seen_functions;

bool contains_any(const std::string& text, const std::vector<std::string>& words) {
  for (const auto& word : words) {
    if (!word.empty() && text.find(word) != std::string::npos) return true;
  }
  return false;
}

void log_call(ue3::UObject* self, ue3::UFunction* function) {
  if (g_config.call_log == CallLogMode::Unique) {
    {
      std::lock_guard lock(g_seen_mutex);
      if (!g_seen_functions.insert(function).second) return;
    }
    log("call {} on {}", ue3::full_name(function), ue3::class_name(self));
    return;
  }
  // Filter mode: building names for every call is slow, which is fine for research.
  const std::string function_name = ue3::full_name(function);
  const std::string object_name = ue3::full_name(self);
  if (contains_any(function_name, g_config.log_filter) || contains_any(object_name, g_config.log_filter)) {
    log("call {} on {}", function_name, object_name);
  }
}

std::wstring widen(const std::string& text) {
  // Console commands are ASCII; MultiByteToWideChar keeps any UTF-8 intact anyway.
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring out(size > 0 ? size - 1 : 0, L'\0');
  if (size > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
  return out;
}

void run_console_command(ue3::UObject* player_controller, const std::string& command) {
  const std::wstring wide = widen(command);
  ConsoleCommandParams params{};
  params.command = {wide.c_str(), static_cast<int32_t>(wide.size() + 1), static_cast<int32_t>(wide.size() + 1)};
  params.write_to_log = 1;
  g_original(player_controller, nullptr, g_console_command.load(), &params, nullptr);
  // The engine allocated return_value; we leave it rather than free it with the wrong allocator.
  const std::string result = ue3::to_string(params.return_value);
  log("console: {}{}", command, result.empty() ? "" : " -> " + result);
}

// Cheat commands (GiveRunes, AddPower, ...) live on the CheatManager, which the
// game only creates once cheats are enabled. Each map load brings a new controller.
// Runs "EnableCheats" as a console command, the same thing the console-unlock key
// (F1) does, retrying every few seconds. Returns true once the CheatManager exists.
bool ensure_cheat_manager(ue3::UObject* player_controller) {
  if (ue3::field<ue3::UObject*>(player_controller, ue3::kPlayerControllerCheatManager) != nullptr) {
    if (g_cheats_tried_for == player_controller && !g_cheats_reported) {
      log("cheats enabled (CheatManager created)");
      g_cheats_reported = true;
    }
    return true;
  }
  const DWORD now = GetTickCount();
  if (g_cheats_tried_for == player_controller && now - g_last_cheats_try < 5000) return false;
  const bool first_try = g_cheats_tried_for != player_controller;
  g_cheats_tried_for = player_controller;
  g_cheats_reported = false;
  g_last_cheats_try = now;
  // EnableCheats creates a CheatManager of type CheatClass, which the shipped game
  // leaves empty (None) - so set it first, as console-unlock tools do.
  auto** cheat_class = reinterpret_cast<ue3::UObject**>(reinterpret_cast<uint8_t*>(player_controller) +
                                                        ue3::kPlayerControllerCheatClass);
  if (*cheat_class == nullptr) {
    if (g_cheat_manager_class == nullptr) {
      g_cheat_manager_class = ue3::find_object("Class DishonoredGame.DishonoredCheatManager");
    }
    if (g_cheat_manager_class != nullptr) {
      *cheat_class = g_cheat_manager_class;
      if (first_try) log("set CheatClass to DishonoredCheatManager");
    } else if (first_try) {
      log("WARNING: Class DishonoredGame.DishonoredCheatManager not found");
    }
  }
  run_console_command(player_controller, "EnableCheats");
  const bool created = ue3::field<ue3::UObject*>(player_controller, ue3::kPlayerControllerCheatManager) != nullptr;
  if (created) {
    log("cheats enabled (CheatManager created)");
    g_cheats_reported = true;
  } else if (first_try) {
    log("WARNING: EnableCheats did not create a CheatManager (CheatClass = {}). Retrying every 5s; "
        "items wait until cheats are on. Enabling the console in-game (F1) also works.",
        ue3::object_name(ue3::field<ue3::UObject*>(player_controller, ue3::kPlayerControllerCheatClass)));
  }
  return created;
}

// The controller's pawn if it is Corvo himself (not a possessed creature), else null.
ue3::UObject* player_pawn(ue3::UObject* player_controller) {
  auto* pawn = ue3::field<ue3::UObject*>(player_controller, ue3::kControllerPawn);
  return ue3::is_a(pawn, g_names.player_pawn) ? pawn : nullptr;
}

bool has_upgrade(ue3::UObject* pawn, ue3::UObject* upgrade) {
  const auto owned = ue3::field<ue3::TArray<ue3::UObject*>>(pawn, ue3::kPlayerPawnUpgrades);
  for (int32_t i = 0; i < owned.count && owned.data != nullptr; ++i) {
    if (owned.data[i] == upgrade) return true;
  }
  return false;
}

// Gives an upgrade (DisTweaks_Upgrade object name) the way level scripts do: the pawn's
// native OnGiveUpgrade handler with a GiveUpgrade Kismet action. Instead of constructing
// an action, it borrows the class default object, sets m_pUpgrade for the call and
// restores it. Skips upgrades already owned, so replaying after a load is harmless.
// Returns false to retry later (Corvo is possessing something).
bool grant_upgrade(ue3::UObject* player_controller, const std::string& name) {
  ue3::UObject* pawn = player_pawn(player_controller);
  if (pawn == nullptr) return false;
  static ue3::UFunction* on_give_upgrade = nullptr;
  static ue3::UObject* action = nullptr;
  if (on_give_upgrade == nullptr) {
    on_give_upgrade = ue3::find_object("Function DishonoredGame.DishonoredPlayerPawn.OnGiveUpgrade");
  }
  if (action == nullptr) {
    action = ue3::find_object("DisSeqAct_GiveUpgrade DishonoredGame.Default__DisSeqAct_GiveUpgrade");
  }
  if (on_give_upgrade == nullptr || action == nullptr) {
    log("WARNING: upgrade {}: OnGiveUpgrade or Default__DisSeqAct_GiveUpgrade not found", name);
    return true;
  }
  const int32_t wanted = ue3::find_name_index(name);
  ue3::UObject* upgrade = nullptr;
  for (auto* candidate : ue3::find_instances(g_names.upgrade_tweaks)) {
    if (candidate->name.index == wanted) {
      upgrade = candidate;
      break;
    }
  }
  if (upgrade == nullptr) {
    log("WARNING: upgrade {}: no DisTweaks_Upgrade with that name is loaded (see 'dump')", name);
    return true;
  }
  if (has_upgrade(pawn, upgrade)) {
    log("upgrade {}: already owned", name);
    return true;
  }
  auto** slot = reinterpret_cast<ue3::UObject**>(reinterpret_cast<uint8_t*>(action) + ue3::kGiveUpgradeActionUpgrade);
  ue3::UObject* saved = *slot;
  *slot = upgrade;
  ue3::UObject* params = action;  // OnGiveUpgrade(DisSeqAct_GiveUpgrade _pAction)
  g_original(pawn, nullptr, on_give_upgrade, &params, nullptr);
  *slot = saved;
  log("upgrade {}: {}", name,
      has_upgrade(pawn, upgrade) ? "granted" : "WARNING: called OnGiveUpgrade but the pawn doesn't list it");
  return true;
}

void emit_event(std::string text);

constexpr const char* kAmmoTypes[] = {"Bullet",       "Bullet_Explosive", "Arrow",      "Arrow_Sleep",
                                      "Arrow_Flare",  "SpringRazor",      "Grenade",    "StickyGrenade",
                                      "Arrow_Explosive", "ArcMine",       "FlashSmoke", "ArcMineStun"};

std::string location_of(ue3::UObject* actor) {
  if (actor == nullptr) return "at=?";
  const auto at = ue3::field<ue3::FVector>(actor, ue3::kActorLocation);
  return std::format("at={:.0f},{:.0f},{:.0f}", at.x, at.y, at.z);
}

// What the player has, compared every half second to report changes from any
// source (pickups, scripted rewards, shops, our own grants) as gain/lose events.
struct Holdings {
  std::map<std::string, int32_t> items;  // abstract items: runes, coins, blueprints, notes...
  std::map<std::string, int32_t> upgrades;
  std::map<std::string, int32_t> ammo;     // count per ammo type, and elixirs
};
std::optional<Holdings> g_holdings;  // reset on every load; the first read is the baseline
DWORD g_last_holdings_tick = 0;
std::atomic<bool> g_mark_requested = false;

Holdings read_holdings(ue3::UObject* pawn) {
  Holdings out;
  if (auto* inventory = ue3::field<ue3::UObject*>(pawn, ue3::kPawnInventory)) {
    const auto abstract = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventoryAbstractItems);
    for (int32_t i = 0; i < abstract.count && abstract.data != nullptr; ++i) {
      auto* item = *reinterpret_cast<ue3::UObject**>(abstract.data + i * 0xC);
      out.items[ue3::object_name(item)] += *reinterpret_cast<int32_t*>(abstract.data + i * 0xC + 4);
    }
    const auto ammo = ue3::field<ue3::TArray<int32_t>>(inventory, ue3::kInventoryAmmo);
    for (int32_t i = 0; i < ammo.count && i < 12 && ammo.data != nullptr; ++i) {
      if (ammo.data[2 * i] > 0) out.ammo[kAmmoTypes[i]] = ammo.data[2 * i];
    }
    for (int i = 0; i < 2; ++i) {
      const int32_t count = ue3::field<int32_t>(inventory, ue3::kInventoryElixirs + 4 * i);
      if (count > 0) out.ammo[i == 0 ? "HealthElixir" : "ManaElixir"] = count;
    }
  }
  const auto owned = ue3::field<ue3::TArray<ue3::UObject*>>(pawn, ue3::kPlayerPawnUpgrades);
  for (int32_t i = 0; i < owned.count && owned.data != nullptr; ++i) out.upgrades[ue3::object_name(owned.data[i])] = 1;
  return out;
}

void report_changes(const std::map<std::string, int32_t>& before, const std::map<std::string, int32_t>& after,
                    const char* kind, const std::string& where) {
  for (const auto& [name, count] : after) {
    const auto old = before.find(name);
    const int32_t was = old == before.end() ? 0 : old->second;
    if (count > was) emit_event(std::format("gain {} {} {}->{} {}", kind, name, was, count, where));
    if (count < was) emit_event(std::format("lose {} {} {}->{}", kind, name, was, count));
  }
  for (const auto& [name, was] : before) {
    if (!after.contains(name)) emit_event(std::format("lose {} {} {}->0", kind, name, was));
  }
}

// Coins going down while something else goes up in the same reading is a purchase
// (Piero's shop, black markets): report what was bought and the price.
void report_purchases(const Holdings& before, const Holdings& after, const std::string& where) {
  auto coins = [](const Holdings& h) {
    const auto it = h.items.find("Coins_AbsItm");
    return it == h.items.end() ? 0 : it->second;
  };
  const int32_t spent = coins(before) - coins(after);
  if (spent <= 0) return;
  auto bought = [&](const std::map<std::string, int32_t>& was, const std::map<std::string, int32_t>& now,
                    const char* kind) {
    for (const auto& [name, count] : now) {
      const auto old = was.find(name);
      if (name != "Coins_AbsItm" && count > (old == was.end() ? 0 : old->second)) {
        emit_event(std::format("buy {} {} for {} {}", kind, name, spent, where));
      }
    }
  };
  bought(before.items, after.items, "item");
  bought(before.upgrades, after.upgrades, "upgrade");
  bought(before.ammo, after.ammo, "ammo");
}

void track_holdings(ue3::UObject* player_controller, bool settled, DWORD now) {
  ue3::UObject* pawn = settled ? player_pawn(player_controller) : nullptr;
  if (!settled) g_holdings.reset();
  if (pawn == nullptr || now - g_last_holdings_tick < 500) return;  // possessing: keep the baseline
  g_last_holdings_tick = now;
  Holdings current = read_holdings(pawn);
  if (g_holdings) {
    const std::string where = location_of(pawn);
    report_changes(g_holdings->items, current.items, "item", where);
    report_changes(g_holdings->upgrades, current.upgrades, "upgrade", where);
    report_changes(g_holdings->ammo, current.ammo, "ammo", where);
    report_purchases(*g_holdings, current, where);
  }
  g_holdings = std::move(current);
}



// Research: writes what the player has, and every upgrade the game knows, to the log.
void dump_player_state(ue3::UObject* player_controller) {
  ue3::UObject* pawn = player_pawn(player_controller);
  log("==== dump ====");
  for (auto* upgrade : ue3::find_instances(g_names.upgrade_tweaks)) {
    log("upgrade {} \"{}\" item={} parent={}{}", ue3::object_name(upgrade),
        ue3::to_string(ue3::field<ue3::FString>(upgrade, ue3::kUpgradeDisplayName)),
        ue3::object_name(ue3::field<ue3::UObject*>(upgrade, ue3::kUpgradeHostItemType)),
        ue3::object_name(ue3::field<ue3::UObject*>(upgrade, ue3::kUpgradeParent)),
        pawn != nullptr && has_upgrade(pawn, upgrade) ? " OWNED" : "");
  }
  if (pawn == nullptr) {
    log("(not playing as Corvo: no player state)");
    return;
  }
  std::string slots;
  for (int i = 0; i < 10; ++i) {
    slots += std::format(" {}", ue3::field<int32_t>(pawn, ue3::kPlayerPawnBoneCharmSlots + 4 * i));
  }
  log("bone charm slots:{}", slots);
  auto* inventory = ue3::field<ue3::UObject*>(pawn, ue3::kPawnInventory);
  if (inventory == nullptr) return;
  const auto items = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventorySlots);
  for (int32_t i = 0; i < items.count && items.data != nullptr; ++i) {
    auto* item = *reinterpret_cast<ue3::UObject**>(items.data + i * 0xC);
    auto* required = *reinterpret_cast<ue3::UObject**>(items.data + i * 0xC + 4);
    log("inventory slot {}: {} (requires {})", i, ue3::full_name(item), ue3::object_name(required));
  }
  // One (count, capacity) pair per EeDisAmmoType.
  const auto ammo = ue3::field<ue3::TArray<int32_t>>(inventory, ue3::kInventoryAmmo);
  for (int32_t i = 0; i < ammo.count && ammo.data != nullptr; ++i) {
    log("ammo {}: {} / {}", i < 12 ? kAmmoTypes[i] : std::to_string(i), ammo.data[2 * i], ammo.data[2 * i + 1]);
  }
  const auto abstract = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventoryAbstractItems);
  for (int32_t i = 0; i < abstract.count && abstract.data != nullptr; ++i) {
    auto* item = *reinterpret_cast<ue3::UObject**>(abstract.data + i * 0xC);
    log("abstract item {} x{}", ue3::object_name(item), *reinterpret_cast<int32_t*>(abstract.data + i * 0xC + 4));
  }
  log("==== end of dump ====");
}

// Runs one queued request on the game thread. Returns false to keep it for later.
bool run_request(ue3::UObject* player_controller, const std::string& request) {
  if (request.rfind("console ", 0) == 0) {
    run_console_command(player_controller, request.substr(8));
    return true;
  }
  if (request.rfind("upgrade ", 0) == 0) return grant_upgrade(player_controller, request.substr(8));
  if (request == "dump") {
    dump_player_state(player_controller);
    return true;
  }
  log("WARNING: unknown game request: {}", request);
  return true;
}

void drain_queue(ue3::UObject* player_controller) {
  std::deque<std::string> requests;
  {
    std::lock_guard lock(g_queue_mutex);
    requests.swap(g_queue);
    g_queue_pending = false;
  }
  std::deque<std::string> later;
  for (const auto& request : requests) {
    if (!run_request(player_controller, request)) later.push_back(request);
  }
  if (!later.empty()) {
    std::lock_guard lock(g_queue_mutex);
    g_queue.insert(g_queue.begin(), later.begin(), later.end());
    g_queue_pending = true;
  }
}

// The local player's controller, read fresh from the engine each time (it is
// replaced on every map load, so it must never be cached).
ue3::UObject* local_player_controller() {
  if (g_engine == nullptr) return nullptr;
  const auto players = ue3::field<ue3::TArray<ue3::UObject*>>(g_engine, ue3::kEngineGamePlayers);
  if (players.count <= 0 || players.data == nullptr || players.data[0] == nullptr) return nullptr;
  return ue3::field<ue3::UObject*>(players.data[0], ue3::kPlayerActor);
}

void emit_event(std::string text) {
  log("event: {}", text);
  std::lock_guard lock(g_events_mutex);
  g_events.push_back(std::move(text));
}

std::string pickup_item_name(ue3::UObject* pickup) {
  auto* tweaks = ue3::field<ue3::UObject*>(pickup, ue3::kPickupTweaks);
  if (tweaks == nullptr) return "?";
  if (ue3::is_a(pickup, g_names.abstract_item_pickup)) {
    return ue3::object_name(ue3::field<ue3::UObject*>(tweaks, ue3::kAbstractPickupTweaksItem));
  }
  if (ue3::is_a(pickup, g_names.whale_bone_charm)) return ue3::object_name(tweaks);
  return "-";
}

// Called on the game thread for every ProcessEvent, before the function runs
// (so the object is still intact). Cheap integer compares unless something matches.
void detect_events(ue3::UObject* self, ue3::UFunction* function, void* params) {
  const int32_t name = function->name.index;
  if (name == g_names.destroyed) {
    if (!ue3::is_a(self, g_names.pickup_base)) return;
    // Pickups already collected in a save are destroyed while the level loads.
    const bool during_load = !g_in_gameplay || GetTickCount() - g_gameplay_started_tick < kSettleMs;
    // Pickups spawned at runtime (rewards, drops) live in the persistent level and get
    // numbered in spawn order, so their names aren't stable; the position is.
    const auto at = ue3::field<ue3::FVector>(self, ue3::kActorLocation);
    emit_event(std::format("pickup {} {} {} at={:.0f},{:.0f},{:.0f}{}", ue3::path_name(self), ue3::class_name(self),
                           pickup_item_name(self), at.x, at.y, at.z, during_load ? " (load)" : ""));
  } else if (name == g_names.client_prepare_map_change) {
    const std::string level = ue3::name_string(*static_cast<ue3::FName*>(params));
    emit_event("mapchange " + level);
    if (level.find("MainMenu") != std::string::npos) {
      g_menu_exit_pending = false;
      if (!g_in_menu) {
        g_in_menu = true;
        log("in main menu");
      }
    } else if (!g_in_menu && g_in_gameplay) {
      // A level transition. Some swap the player's inventory out and back (the Void keeps
      // a backup and restores it on return), so treat it like a load: wait to settle, then
      // the client re-applies every received item.
      g_gameplay_started_tick = GetTickCount();
      emit_event("gameplay");
    } else if (g_in_menu && !g_menu_exit_pending) {
      g_menu_exit_pending = true;
      g_menu_exit_tick = GetTickCount();
      g_menu_controller = local_player_controller();
      g_menu_pawn = g_menu_controller ? ue3::field<ue3::UObject*>(g_menu_controller, ue3::kControllerPawn) : nullptr;
    }
  } else if (name == g_names.on_add_abstract_item) {
    auto* action = *static_cast<ue3::UObject**>(params);
    if (action == nullptr) return;
    auto* item = ue3::field<ue3::UObject*>(action, ue3::kAddAbstractItemActionItem);
    emit_event(std::format("additem {} {}", ue3::path_name(action), ue3::object_name(item)));
  } else if (name == g_names.on_add_power || name == g_names.on_give_upgrade || name == g_names.on_give_pickup ||
             name == g_names.on_add_inventory_item) {
    // A level script gives the player something. Our own grants call the original
    // function directly and don't come through here.
    if (!ue3::is_a(self, g_names.player_pawn)) return;  // NPCs get equipment this way too
    auto* action = *static_cast<ue3::UObject**>(params);
    if (action == nullptr) return;
    std::string what;
    if (name == g_names.on_add_power) {
      what = std::format("power {} {}", ue3::to_string(ue3::field<ue3::FString>(action, ue3::kAddPowerActionName)),
                         ue3::field<int32_t>(action, ue3::kAddPowerActionLevel));
    } else if (name == g_names.on_give_upgrade) {
      what = "upgrade " + ue3::object_name(ue3::field<ue3::UObject*>(action, ue3::kGiveUpgradeActionUpgrade));
    } else if (name == g_names.on_give_pickup) {
      what = "pickup " + ue3::object_name(ue3::field<ue3::UObject*>(action, ue3::kGivePickupActionPickup));
    } else {
      what = "inventory " + ue3::object_name(ue3::field<ue3::UObject*>(action, ue3::kAddInventoryActionItem));
    }
    emit_event(std::format("grant {} by {}", what, ue3::path_name(action)));
  } else if (name == g_names.on_objective_action) {
    auto* action = *static_cast<ue3::UObject**>(params);
    if (action == nullptr) return;
    const auto objectives =
        ue3::field<ue3::TArray<ue3::UObject*>>(action, ue3::kObjectiveActionObjectives);
    std::string names;
    for (int32_t i = 0; i < objectives.count && objectives.data != nullptr; ++i) {
      names += (names.empty() ? "" : ",") + ue3::object_name(objectives.data[i]);
    }
    emit_event(std::format("objective {} {} {}", ue3::path_name(action), ue3::class_name(action),
                           names.empty() ? "-" : names));
  }
}

// Finds the engine object and ConsoleCommand. Retried until they are loaded.
void resolve_engine_objects() {
  const DWORD now = GetTickCount();
  if (now - g_last_resolve_tick < 1000) return;
  g_last_resolve_tick = now;

  if (g_engine == nullptr) g_engine = ue3::find_instance("DishonoredEngine");
  ue3::UFunction* console_command = ue3::find_object("Function Engine.PlayerController.ConsoleCommand");
  if (g_engine == nullptr || console_command == nullptr) {
    if (++g_resolve_attempts == 30) {
      log("WARNING: after 30s still missing:{}{}", g_engine ? "" : " DishonoredEngine",
          console_command ? "" : " PlayerController.ConsoleCommand");
    }
    return;
  }
  g_names.destroyed = ue3::find_name_index("Destroyed");
  g_names.client_prepare_map_change = ue3::find_name_index("ClientPrepareMapChange");
  g_names.on_add_abstract_item = ue3::find_name_index("OnAddAbstractItem");
  g_names.on_objective_action = ue3::find_name_index("OnObjectiveAction");
  g_names.pre_exit = ue3::find_name_index("PreExit");
  g_names.on_add_power = ue3::find_name_index("OnAddPower");
  g_names.on_give_upgrade = ue3::find_name_index("OnGiveUpgrade");
  g_names.on_give_pickup = ue3::find_name_index("OnGivePickup");
  g_names.on_add_inventory_item = ue3::find_name_index("OnAddInventoryItem");
  g_names.pickup_base = ue3::find_name_index("DisPickup_Base");
  g_names.abstract_item_pickup = ue3::find_name_index("DisAbstractItemPickup");
  g_names.whale_bone_charm = ue3::find_name_index("DisWhaleBoneCharm");
  g_names.player_pawn = ue3::find_name_index("DishonoredPlayerPawn");
  g_names.upgrade_tweaks = ue3::find_name_index("DisTweaks_Upgrade");
  g_console_command = console_command;
  log("found DishonoredEngine ({}) and PlayerController.ConsoleCommand", ue3::object_name(g_engine));
}

void __fastcall process_event_hook(ue3::UObject* self, void* edx, ue3::UFunction* function, void* params,
                                   void* result) {
  if (g_names.pre_exit >= 0 && function != nullptr && function->name.index == g_names.pre_exit &&
      !g_shutting_down) {
    g_shutting_down = true;
    log("game is exiting; going idle");
  }
  if (self != nullptr && function != nullptr && g_engine_ready && !g_inside_hook_work && !g_shutting_down) {
    g_inside_hook_work = true;
    if (g_console_command == nullptr) resolve_engine_objects();
    if (g_config.call_log != CallLogMode::Off && g_call_logging) log_call(self, function);
    if (g_console_command != nullptr) detect_events(self, function, params);
    // ProcessEvent runs thousands of times a frame; look at the player at most every 100ms.
    const DWORD now = GetTickCount();
    if (g_console_command != nullptr && now - g_last_check_tick >= 100) {
      g_last_check_tick = now;
      ue3::UObject* controller = local_player_controller();
      // A pawn means we're playing, not in the main menu or a loading screen.
      const bool in_gameplay = controller != nullptr && ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn);
      if (!in_gameplay && g_in_gameplay) g_ever_lost_gameplay = true;
      if (in_gameplay && g_in_gameplay && g_in_menu && g_menu_exit_pending) {
        auto* pawn = ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn);
        const bool replaced = controller != g_menu_controller || pawn != g_menu_pawn;
        if (replaced || now - g_menu_exit_tick > 20000) {
          g_menu_exit_pending = false;
          g_in_menu = false;
          g_gameplay_started_tick = now;
          log("left main menu (new game{})", replaced ? "" : ", by timeout");
          emit_event("gameplay");
        }
      }
      if (in_gameplay && !g_in_gameplay) {
        g_gameplay_started_tick = now;
        if (g_ever_lost_gameplay && g_in_menu) {
          g_in_menu = false;  // came back from a loading screen: a real level
          log("left main menu");
        }
        // Every level streams into one persistent map (DishonoredGameFull_P), so the map name
        // says nothing; the zone comes from pickup paths and mapchange events instead.
        emit_event("gameplay");
      }
      if (in_gameplay != g_in_gameplay) log("{}", in_gameplay ? "in gameplay" : "not in gameplay");
      if (!in_gameplay) g_cheats_tried_for = nullptr;  // a new controller may reuse the old address
      g_in_gameplay = in_gameplay;
      const bool settled = in_gameplay && !g_in_menu && now - g_gameplay_started_tick >= kSettleMs;
      if (g_mark_requested.exchange(false)) {
        log("==== MARK (F8) {} ====",
            location_of(controller ? ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn) : nullptr));
      }
      if (in_gameplay) track_holdings(controller, settled, now);
      else g_holdings.reset();
      g_cheats_ready = settled && ensure_cheat_manager(controller);
      if (g_cheats_ready && g_queue_pending) drain_queue(controller);
    }
    g_inside_hook_work = false;
  }
  g_original(self, edx, function, params, result);
}

}  // namespace

bool install_hooks(const Config& config) {
  g_config = config;
  uintptr_t target = 0;
  if (config.process_event_offset != 0) {
    target = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + config.process_event_offset;
  } else {
    target = find_pattern(kProcessEventPattern);
  }
  if (target == 0) {
    log("ERROR: ProcessEvent not found (pattern did not match exactly once). Set ProcessEventOffset.");
    return false;
  }
  log("ProcessEvent at exe+0x{:X}", target - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));

  if (MH_Initialize() != MH_OK ||
      MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&process_event_hook),
                    reinterpret_cast<void**>(&g_original)) != MH_OK ||
      MH_EnableHook(reinterpret_cast<void*>(target)) != MH_OK) {
    log("ERROR: failed to hook ProcessEvent");
    return false;
  }
  return true;
}

void on_engine_ready() { g_engine_ready = true; }

void queue_game_request(const std::string& request) {
  std::lock_guard lock(g_queue_mutex);
  g_queue.push_back(request);
  g_queue_pending = true;
}

CommandState command_state() {
  if (g_console_command == nullptr || !g_in_gameplay || g_in_menu ||
      GetTickCount() - g_gameplay_started_tick < kSettleMs) {
    return CommandState::Waiting;
  }
  return g_cheats_ready ? CommandState::Ready : CommandState::NoCheats;
}

void request_mark() { g_mark_requested = true; }

void toggle_call_logging() {
  g_call_logging = !g_call_logging;
  log("---- call logging {} ----", g_call_logging ? "resumed" : "paused");
}

std::optional<std::string> get_event(size_t index) {
  std::lock_guard lock(g_events_mutex);
  if (index >= g_events.size()) return std::nullopt;
  return g_events[index];
}

void shut_down() { g_shutting_down = true; }

}  // namespace ap
