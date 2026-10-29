#include "game_hooks.hpp"

#include <windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <deque>
#include <mutex>
#include <map>
#include <sstream>
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
// A pickup destroyed farther than this from the player wasn't picked up by them.
constexpr float kPickupReach = 1000.0f;
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
  int32_t power_buy = -1;  // DisGFxMoviePlayerJournal.PowerBuy(PowerId, Level): buying a power with runes
  int32_t active_power_component = -1;  // class DishonoredActivePowerComponent (one object per active power)
  int32_t req_buy_item = -1;            // DisGFxMoviePlayerStore.Req_BuyItem(ItemIdx): buying at a shop
  int32_t world_upgrade = -1;           // class DisUpgrade: an upgrade picked up in the world (DLC statues)
  int32_t player_pawn_tweaks = -1;      // class DisTweaks_PlayerPawn: holds the base bone charm capacity
  int32_t ranged_weapon = -1;           // class DishonoredWeapon_Ranged: pistol, crossbow (loaded magazine)
  int32_t sword = -1;                   // class DishonoredWepSword
  int32_t sword_tweaks = -1;            // class DisTweaks_WepSword (Twk_Inv_SwordCorvo)
  int32_t player_choice_confirm = -1;   // DisGFxMoviePlayerHUD.OnPlayerChoiceConfirm(Index): travel prompts, dialogue
  int32_t post_render = -1;             // HUD.PostRender: the HUD draws a frame (Canvas valid)
  int32_t hud = -1;                     // class HUD
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

// ---- Limits: what the player may have, sent by the client ----
// Only randomized powers and upgrades have limits. Nothing is blocked or removed until
// the client has sent them, so playing without Archipelago is unaffected.
std::mutex g_limits_mutex;
std::map<std::string, int32_t> g_power_limits;  // lower-case power name -> highest allowed level
std::map<std::string, bool> g_upgrade_allowed;  // randomized upgrade -> received
std::atomic<bool> g_limits_known = false;
std::atomic<int32_t> g_bone_charm_slots = -1;  // Bone Charm Slot items received (-1: leave the game's)
std::map<std::string, bool> g_ammo_allowed;   // randomized ammo type (kAmmoTypes name) -> received
std::map<std::string, bool> g_item_allowed;   // randomized abstract item (RewireTool) -> received
std::map<std::string, bool> g_equip_allowed;  // randomized equipment that can be held (Sword) -> received
// Travel prompts by destination tag ("all" for every prompt) -> allowed. A refused prompt
// acts as if the player picked its "stay" option.
std::map<std::string, bool> g_travel_allowed;
// The prologue's scripted fight hands Corvo a sword; it's allowed there (like Blink in
// the Void) so the fight can't softlock. Set by mapchange; a save loaded mid-prologue
// isn't recognised.
std::atomic<bool> g_in_prologue = false;
// The Void needs Blink to cross it, so script grants there go through; leaving the Void
// (a mapchange) settles and removes them again unless received.
std::atomic<bool> g_in_void = false;
std::map<std::string, DWORD> g_last_power_removal;

std::string lower(std::string text) {
  for (auto& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::optional<int32_t> power_limit(const std::string& power) {
  std::lock_guard lock(g_limits_mutex);
  const auto it = g_power_limits.find(lower(power));
  return it == g_power_limits.end() ? std::nullopt : std::optional(it->second);
}

std::optional<bool> allowed_in(const std::map<std::string, bool>& limits, const std::string& name) {
  std::lock_guard lock(g_limits_mutex);
  const auto it = limits.find(name);
  return it == limits.end() ? std::nullopt : std::optional(it->second);
}

std::optional<bool> upgrade_allowed(const std::string& upgrade) { return allowed_in(g_upgrade_allowed, upgrade); }

// Is this (kind, name) from a purchase or holding something the player may not have yet?
bool blocked(const std::string& kind, const std::string& name) {
  if (kind == "upgrade") return upgrade_allowed(name) == false;
  if (kind == "ammo") return allowed_in(g_ammo_allowed, name) == false;
  if (kind == "item") return allowed_in(g_item_allowed, name) == false;
  if (kind == "equip") return allowed_in(g_equip_allowed, name) == false;
  return false;
}

// A sword not received yet. Unequipping alone isn't enough (attacking or blocking draws
// it again), so it's taken out of the inventory: first empty hands are equipped in its
// place with the pawn's OnEquipItemType (borrowed default DisSeqAct_EquipItemType, its
// "Apply" input marked), then, once no hand holds it, its slot is cleared. An empty sword
// slot is a normal state (Coldridge before the City Watch sword, the Flooded District).
// When Progressive Sword arrives, Corvo's sword is given back through OnAddInventoryItem,
// provided the sewer gear cutscene has happened (PrsnSewer_Weapons note) and the game
// isn't holding the gear in its backup (Flooded District capture).
DWORD g_last_sword_action = 0;
DWORD g_last_sword_grant = 0;

ue3::UObject* tweak_named(int32_t class_name_index, const char* name) {
  const int32_t wanted = ue3::find_name_index(name);
  for (auto* tweak : ue3::find_instances_of_subclasses(class_name_index)) {
    if (tweak->name.index == wanted) return tweak;
  }
  return nullptr;
}

void unequip_hand(ue3::UObject* pawn, uint8_t usage) {
  static ue3::UFunction* equip = ue3::find_object("Function DishonoredGame.DishonoredPawn.OnEquipItemType");
  static ue3::UObject* action =
      ue3::find_object("DisSeqAct_EquipItemType DishonoredGame.Default__DisSeqAct_EquipItemType");
  static ue3::UObject* empty_hands = ue3::find_object("Class DishonoredGame.DishonoredItemEmpty");
  if (equip == nullptr || action == nullptr || empty_hands == nullptr) return;
  auto* fields = reinterpret_cast<uint8_t*>(action);
  auto& type = *reinterpret_cast<ue3::UObject**>(fields + ue3::kEquipActionItemType);
  auto& preferred = *(fields + ue3::kEquipActionUsage);
  auto& flags = *reinterpret_cast<uint32_t*>(fields + ue3::kEquipActionFlags);
  // The handler checks which input fired ("Apply" or "Release (AI only)"), as when a
  // level script activates the action, so mark input 0 for the call.
  const auto inputs = ue3::field<ue3::TArray<uint8_t>>(action, ue3::kSequenceOpInputLinks);
  uint32_t* apply = inputs.count > 0 && inputs.data != nullptr
                        ? reinterpret_cast<uint32_t*>(inputs.data + ue3::kInputLinkHasImpulse)
                        : nullptr;
  const auto saved_type = type;
  const auto saved_usage = preferred;
  const auto saved_flags = flags;
  const uint32_t saved_apply = apply ? *apply : 0;
  type = empty_hands;
  preferred = usage;
  flags = 1;  // m_bDoInstantly
  if (apply) *apply |= 1;
  ue3::UObject* params = action;
  g_original(pawn, nullptr, equip, &params, nullptr);
  type = saved_type;
  preferred = saved_usage;
  flags = saved_flags;
  if (apply) *apply = saved_apply;
}

void give_sword(ue3::UObject* pawn) {
  static ue3::UFunction* add = ue3::find_object("Function DishonoredGame.DishonoredPawn.OnAddInventoryItem");
  static ue3::UObject* action =
      ue3::find_object("DisSeqAct_AddInventoryItem DishonoredGame.Default__DisSeqAct_AddInventoryItem");
  static ue3::UObject* sword = tweak_named(g_names.sword_tweaks, "Twk_Inv_SwordCorvo");
  if (sword == nullptr) sword = tweak_named(g_names.sword_tweaks, "Twk_Inv_SwordCorvo");  // may load later
  if (add == nullptr || action == nullptr || sword == nullptr) {
    log("WARNING: can't give the sword back (OnAddInventoryItem {}, Twk_Inv_SwordCorvo {})",
        add != nullptr && action != nullptr, sword != nullptr);
    return;
  }
  auto& item = *reinterpret_cast<ue3::UObject**>(reinterpret_cast<uint8_t*>(action) + ue3::kAddInventoryActionItem);
  const auto saved = item;
  item = sword;
  ue3::UObject* params = action;
  g_original(pawn, nullptr, add, &params, nullptr);
  item = saved;
  emit_event("give sword Twk_Inv_SwordCorvo");
}

bool has_abstract_item(ue3::UObject* inventory, const std::string& name) {
  const auto abstract = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventoryAbstractItems);
  for (int32_t i = 0; i < abstract.count && abstract.data != nullptr; ++i) {
    if (ue3::object_name(*reinterpret_cast<ue3::UObject**>(abstract.data + i * 0xC)) == name &&
        *reinterpret_cast<int32_t*>(abstract.data + i * 0xC + 4) > 0) {
      return true;
    }
  }
  return false;
}

void enforce_sword(ue3::UObject* pawn, ue3::UObject* inventory, DWORD now) {
  const auto allowed = allowed_in(g_equip_allowed, "Sword");
  if (g_in_prologue || !allowed || now - g_last_sword_action < 1000) return;
  const auto slots = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventorySlots);
  if (slots.data == nullptr) return;
  auto item_in = [&](int32_t slot) { return *reinterpret_cast<ue3::UObject**>(slots.data + slot * 0xC); };
  auto hand_slot = [&](int usage) {
    return ue3::field<int32_t>(inventory, ue3::kInventoryEquipUsage + usage * ue3::kEquipUsageSize);
  };
  int32_t sword_slot = -1;
  for (int32_t i = 0; i < slots.count; ++i) {
    if (ue3::is_a(item_in(i), g_names.sword)) sword_slot = i;
  }
  if (*allowed) {
    const bool backed_up = ue3::field<uint32_t>(inventory, ue3::kInventoryFlags) & 2;  // m_bBackupIsValid
    if (sword_slot < 0 && !backed_up && has_abstract_item(inventory, "PrsnSewer_Weapons") &&
        now - g_last_sword_grant >= 10000) {  // one try every 10 s
      g_last_sword_grant = now;
      give_sword(pawn);
    }
    return;
  }
  if (sword_slot < 0) return;
  g_last_sword_action = now;
  bool held = false;
  for (uint8_t usage = 1; usage <= 2; ++usage) {  // EDisEquipUsage_Primary, _Secondary
    if (hand_slot(usage) == sword_slot) {
      held = true;
      unequip_hand(pawn, usage);
      emit_event(std::format("unequip sword (hand {})", usage));
    }
  }
  if (held) return;  // take it out next time, once it's no longer in a hand
  emit_event(std::format("take sword {} from slot {}", ue3::object_name(item_in(sword_slot)), sword_slot));
  *reinterpret_cast<ue3::UObject**>(slots.data + sword_slot * 0xC) = nullptr;
}

// Equipment not received yet: ammo of that type (and a magazine loaded with it) and
// collectable items like the Rewire Tool are kept at 0.
void enforce_equipment(ue3::UObject* pawn) {
  auto* inventory = ue3::field<ue3::UObject*>(pawn, ue3::kPawnInventory);
  if (inventory == nullptr) return;
  enforce_sword(pawn, inventory, GetTickCount());
  const auto ammo = ue3::field<ue3::TArray<int32_t>>(inventory, ue3::kInventoryAmmo);
  for (int32_t i = 0; i < ammo.count && i < 12 && ammo.data != nullptr; ++i) {
    if (ammo.data[2 * i] > 0 && blocked("ammo", kAmmoTypes[i])) {
      emit_event(std::format("remove ammo {} {}->0", kAmmoTypes[i], ammo.data[2 * i]));
      ammo.data[2 * i] = 0;
    }
  }
  const auto slots = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventorySlots);
  for (int32_t i = 0; i < slots.count && slots.data != nullptr; ++i) {
    auto* weapon = *reinterpret_cast<ue3::UObject**>(slots.data + i * 0xC);
    if (!ue3::is_a(weapon, g_names.ranged_weapon)) continue;
    const uint8_t type = ue3::field<uint8_t>(weapon, ue3::kRangedCurAmmoType);
    auto& loaded = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(weapon) + ue3::kRangedLoadedAmmo);
    if (type < 12 && loaded > 0 && blocked("ammo", kAmmoTypes[type])) {
      emit_event(std::format("unload {} {} {}->0", ue3::class_name(weapon), kAmmoTypes[type], loaded));
      loaded = 0;
    }
  }
  const auto abstract = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventoryAbstractItems);
  for (int32_t i = 0; i < abstract.count && abstract.data != nullptr; ++i) {
    const std::string name = ue3::object_name(*reinterpret_cast<ue3::UObject**>(abstract.data + i * 0xC));
    auto& quantity = *reinterpret_cast<int32_t*>(abstract.data + i * 0xC + 4);
    if (quantity > 0 && blocked("item", name)) {
      emit_event(std::format("remove item {} {}->0", name, quantity));
      quantity = 0;
    }
  }
}

// Takes a power away the way level scripts do: the pawn's OnRemovePower handler with the
// borrowed default DisSeqAct_RemovePower (same trick as grant_upgrade).
void remove_power(ue3::UObject* pawn, const std::string& power) {
  static ue3::UFunction* on_remove_power = nullptr;
  static ue3::UObject* action = nullptr;
  if (on_remove_power == nullptr) {
    on_remove_power = ue3::find_object("Function DishonoredGame.DishonoredPlayerPawn.OnRemovePower");
  }
  if (action == nullptr) {
    action = ue3::find_object("DisSeqAct_RemovePower DishonoredGame.Default__DisSeqAct_RemovePower");
  }
  if (on_remove_power == nullptr || action == nullptr) {
    log("WARNING: OnRemovePower or Default__DisSeqAct_RemovePower not found");
    return;
  }
  const std::wstring wide = widen(power);
  auto* slot = reinterpret_cast<ue3::FString*>(reinterpret_cast<uint8_t*>(action) + ue3::kRemovePowerActionName);
  const ue3::FString saved = *slot;
  *slot = {wide.c_str(), static_cast<int32_t>(wide.size() + 1), static_cast<int32_t>(wide.size() + 1)};
  ue3::UObject* params = action;
  g_original(pawn, nullptr, on_remove_power, &params, nullptr);
  *slot = saved;
}

void add_coins(ue3::UObject* pawn, int32_t amount) {
  auto* inventory = ue3::field<ue3::UObject*>(pawn, ue3::kPawnInventory);
  if (inventory == nullptr) return;
  const auto abstract = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventoryAbstractItems);
  for (int32_t i = 0; i < abstract.count && abstract.data != nullptr; ++i) {
    auto* item = *reinterpret_cast<ue3::UObject**>(abstract.data + i * 0xC);
    if (ue3::object_name(item) == "Coins_AbsItm") {
      *reinterpret_cast<int32_t*>(abstract.data + i * 0xC + 4) += amount;
      return;
    }
  }
}

// Passive powers (Vitality, ...) are entries in the powers component; active powers
// (Blink, ...) are each their own component object owned by the pawn. Those objects are
// found by scanning the object table, so the list is cached per pawn.
std::vector<ue3::UObject*> g_active_powers;
ue3::UObject* g_active_powers_pawn = nullptr;
DWORD g_active_powers_tick = 0;

std::vector<std::pair<std::string, int32_t>> power_levels(ue3::UObject* pawn) {
  std::vector<std::pair<std::string, int32_t>> out;
  if (auto* powers_component = ue3::field<ue3::UObject*>(pawn, ue3::kPlayerPawnPowers)) {
    const auto powers = ue3::field<ue3::TArray<uint8_t>>(powers_component, ue3::kPowersComponentPowers);
    for (int32_t i = 0; i < powers.count && powers.data != nullptr; ++i) {
      const uint8_t* power = powers.data + i * ue3::kPowerSize;
      out.emplace_back(ue3::name_string(*reinterpret_cast<const ue3::FName*>(power)),
                       *reinterpret_cast<const int32_t*>(power + ue3::kPowerCurrentLevel));
    }
  }
  const DWORD now = GetTickCount();
  if (pawn != g_active_powers_pawn || now - g_active_powers_tick > 10000) {
    g_active_powers.clear();
    for (auto* component : ue3::find_instances_of_subclasses(g_names.active_power_component)) {
      if (ue3::field<ue3::UObject*>(component, ue3::kActivePowerOwner) == pawn) g_active_powers.push_back(component);
    }
    g_active_powers_pawn = pawn;
    g_active_powers_tick = now;
  }
  for (auto* component : g_active_powers) {
    out.emplace_back(ue3::name_string(ue3::field<ue3::FName>(component, ue3::kActivePowerName)),
                     ue3::field<int32_t>(component, ue3::kActivePowerLevel));
  }
  return out;
}

// Earlier builds "disarmed" the DLC statues at the pub by pointing their upgrade at Corvo's
// Mask and hiding them. The game's DLC bookkeeping (DisTweaks_PlayerPawn.m_DLCPackInfo)
// tracks the statues by upgrade name, and saves made after that crashed on load or on
// waking up. Statues are now left alone, and those swapped earlier (stored that way in
// saves) get their own upgrade back: matched by the statue's model, or by position for
// the two seen in the user's pub. Scanned every 2 s.
DWORD g_last_world_upgrade_scan = 0;

// Bone charm capacity = a base (a tweak value, 3 in the shipped game) + the bonus of each
// owned capacity upgrade (Piero's two, the four DLC statues). An upgrade's effect stays even
// if it's taken off the list, so capacity upgrades are never removed; instead the base is
// set to (Bone Charm Slot items received) - (bonus owned), which keeps the total at what was
// received whatever the player picked up or the save holds. May go below 0.
float capacity_bonus(ue3::UObject* upgrade) {
  static const int32_t attribute = ue3::find_name_index("Attribute_MaxActivatedBoneCharmCountBonus");
  float bonus = 0;
  const auto attributes = ue3::field<ue3::TArray<uint8_t>>(upgrade, ue3::kUpgradeAttributes);
  for (int32_t i = 0; i < attributes.count && attributes.data != nullptr && attribute >= 0; ++i) {
    if (reinterpret_cast<const ue3::FName*>(attributes.data + i * 0xC)->index == attribute) {
      bonus += *reinterpret_cast<const float*>(attributes.data + i * 0xC + 8);
    }
  }
  return bonus;
}

void apply_bone_charm_base(ue3::UObject* pawn) {
  const int32_t slots = g_bone_charm_slots;
  if (slots < 0 || pawn == nullptr) return;
  const auto owned = ue3::field<ue3::TArray<ue3::UObject*>>(pawn, ue3::kPlayerPawnUpgrades);
  float bonus = 0;
  for (int32_t i = 0; i < owned.count && owned.data != nullptr; ++i) {
    if (owned.data[i] != nullptr) bonus += capacity_bonus(owned.data[i]);
  }
  const int32_t base = slots - static_cast<int32_t>(bonus + 0.5f);
  for (auto* tweaks : ue3::find_instances_of_subclasses(g_names.player_pawn_tweaks)) {
    auto& value = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(tweaks) + ue3::kPawnTweaksBoneCharmBase);
    if (value != base) {
      emit_event(std::format("bone charm base {} {}->{} ({} received, +{:g} owned)", ue3::object_name(tweaks), value,
                             base, slots, bonus));
      value = base;
    }
  }
}

void restore_world_upgrades(ue3::UObject* pawn, DWORD now) {
  if (now - g_last_world_upgrade_scan < 2000) return;
  g_last_world_upgrade_scan = now;
  apply_bone_charm_base(pawn);
  static ue3::UFunction* set_hidden = ue3::find_object("Function Engine.Actor.SetHidden");
  const int32_t mask_name = ue3::find_name_index("Twk_Upgrade_Mask");
  ue3::UObject* mask = nullptr;
  std::vector<ue3::UObject*> statues;
  for (auto* upgrade : ue3::find_instances(g_names.upgrade_tweaks)) {
    if (upgrade->name.index == mask_name) mask = upgrade;
    if (ue3::object_name(upgrade).rfind("Twk_Upgrade_Statue", 0) == 0) statues.push_back(upgrade);
  }
  if (mask == nullptr || set_hidden == nullptr) return;
  // Where the user's pub had its Whale and Rat statues when they were swapped.
  const std::pair<const char*, ue3::FVector> known[] = {
      {"Twk_Upgrade_StatueWhale", {-1377, -25, 1400}},
      {"Twk_Upgrade_StatueRat", {-1214, -1331, 1495}},
  };
  for (auto* actor : ue3::find_instances(g_names.world_upgrade)) {
    auto& tweaks = *reinterpret_cast<ue3::UObject**>(reinterpret_cast<uint8_t*>(actor) + ue3::kWorldUpgradeTweaks);
    if (tweaks != mask) continue;
    auto* component = ue3::field<ue3::UObject*>(actor, ue3::kWorldUpgradeMesh);
    auto* model = component != nullptr ? ue3::field<ue3::UObject*>(component, ue3::kStaticMeshComponentMesh) : nullptr;
    ue3::UObject* original = nullptr;
    for (auto* statue : statues) {
      if (model != nullptr && ue3::field<ue3::UObject*>(statue, ue3::kUpgradeMesh) == model) original = statue;
    }
    const auto at = ue3::field<ue3::FVector>(actor, ue3::kActorLocation);
    for (const auto& [name, where] : known) {
      const float dx = at.x - where.x, dy = at.y - where.y, dz = at.z - where.z;
      if (original != nullptr || dx * dx + dy * dy + dz * dz > 150.0f * 150.0f) continue;
      for (auto* statue : statues) {
        if (ue3::object_name(statue) == name) original = statue;
      }
    }
    if (original == nullptr) {
      static std::unordered_set<ue3::UObject*> reported;
      if (reported.insert(actor).second) {
        log("world upgrade {} is Corvo's Mask (model {}) {}: not a known statue, left alone", ue3::path_name(actor),
            ue3::object_name(model), location_of(actor));
      }
      continue;
    }
    tweaks = original;
    uint32_t hidden = 0;
    g_original(actor, nullptr, set_hidden, &hidden, nullptr);
    emit_event(std::format("restored world upgrade {} {}", ue3::object_name(original), location_of(actor)));
  }
}

// Removes randomized powers above their limit and upgrades not received. Runs every
// half second during settled gameplay, outside the Void.
void enforce_limits(ue3::UObject* pawn, DWORD now) {
  if (!g_limits_known || g_in_void) return;
  enforce_equipment(pawn);
  for (const auto& [name, level] : power_levels(pawn)) {
    const auto limit = power_limit(name);
    if (!limit || level <= *limit) continue;
    auto& last = g_last_power_removal[name];
    if (now - last < 5000) continue;  // give the game time; don't spam if it doesn't work
    last = now;
    emit_event(std::format("remove power {} {}->{}", name, level, *limit));
    if (*limit > 0) {
      queue_game_request(std::format("console AddPower {} {}", name, *limit));  // AddPower sets the level
    } else {
      remove_power(pawn, name);
    }
  }
  auto& owned = *reinterpret_cast<ue3::TArray<ue3::UObject*>*>(reinterpret_cast<uint8_t*>(pawn) +
                                                                ue3::kPlayerPawnUpgrades);
  for (int32_t i = 0; i < owned.count && owned.data != nullptr;) {
    const std::string name = ue3::object_name(owned.data[i]);
    // Capacity upgrades stay: removing one doesn't take its slot away (the base makes up
    // for it, see apply_bone_charm_base), and the DLC gift code tracks the statues.
    const bool capacity = capacity_bonus(owned.data[i]) != 0;
    if (upgrade_allowed(name) == false && !capacity) {
      emit_event("remove upgrade " + name);
      for (int32_t j = i; j + 1 < owned.count; ++j) owned.data[j] = owned.data[j + 1];
      --owned.count;
    } else {
      ++i;
    }
  }
}

// Called before a function runs; returning true skips it. Only for things the client
// has limited: buying a power with runes, and level scripts granting powers/upgrades.
bool should_block(ue3::UObject* self, int32_t name, void* params) {
  if (!g_limits_known || params == nullptr) return false;
  if (name == g_names.power_buy) {
    struct PowerBuyParams {
      ue3::FString power_id;
      int32_t level;
    };
    const auto* buy = static_cast<PowerBuyParams*>(params);
    const std::string power = ue3::to_string(buy->power_id);
    const auto limit = power_limit(power);
    if (limit && buy->level > *limit) {
      emit_event(std::format("blocked buy power {} {}", power, buy->level));
      return true;
    }
    if (!limit) log("PowerBuy {} {} (no limit for that name)", power, buy->level);
    return false;
  }
  if (name == g_names.req_buy_item) {
    // The shop's displayed list holds pointers to its store entries; each names the
    // item it sells. Every pointer is checked before use, since this layout is inferred.
    const int32_t index = *static_cast<int32_t*>(params);
    const auto items = ue3::field<ue3::TArray<uint8_t*>>(self, ue3::kStoreDisplayedItems);
    if (index < 0 || index >= items.count || !ue3::is_readable(items.data + index, sizeof(void*))) return false;
    uint8_t* entry = items.data[index];
    if (!ue3::is_readable(entry + ue3::kStoreItemItem, sizeof(void*))) return false;
    auto* item = *reinterpret_cast<ue3::UObject**>(entry + ue3::kStoreItemItem);
    if (!ue3::is_readable(item, sizeof(ue3::UObject))) return false;
    const std::string item_name = ue3::object_name(item);
    log("shop: buying entry {} = {}", index, item_name);
    if (upgrade_allowed(item_name) == false) {
      emit_event("blocked buy upgrade " + item_name);
      return true;
    }
    return false;
  }
  if (name != g_names.on_add_power && name != g_names.on_give_upgrade) return false;
  if (!ue3::is_a(self, g_names.player_pawn)) return false;
  auto* action = *static_cast<ue3::UObject**>(params);
  if (action == nullptr) return false;
  if (name == g_names.on_add_power && !g_in_void) {
    const std::string power = ue3::to_string(ue3::field<ue3::FString>(action, ue3::kAddPowerActionName));
    const int32_t level = ue3::field<int32_t>(action, ue3::kAddPowerActionLevel);
    const auto limit = power_limit(power);
    if (limit && level > *limit) {
      emit_event(std::format("blocked grant power {} {}", power, level));
      return true;
    }
  } else if (name == g_names.on_give_upgrade) {
    const std::string upgrade =
        ue3::object_name(ue3::field<ue3::UObject*>(action, ue3::kGiveUpgradeActionUpgrade));
    if (upgrade_allowed(upgrade) == false) {
      emit_event("blocked grant upgrade " + upgrade);
      return true;
    }
  }
  return false;
}

// Coins going down while something else goes up in the same reading is a purchase
// (Piero's shop, black markets): report what was bought and the price.
void report_purchases(ue3::UObject* pawn, const Holdings& before, const Holdings& after,
                      const std::string& where) {
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
        // Something randomized bought before it was received: enforcement takes it away,
        // so give the coins back.
        if (g_limits_known && blocked(kind, name)) {
          add_coins(pawn, spent);
          emit_event(std::format("refund {} for {}", spent, name));
        }
      }
    }
  };
  bought(before.items, after.items, "item");
  bought(before.upgrades, after.upgrades, "upgrade");
  bought(before.ammo, after.ammo, "ammo");
}

void track_holdings(ue3::UObject* player_controller, bool settled, DWORD now) {
  ue3::UObject* pawn = settled ? player_pawn(player_controller) : nullptr;
  if (!settled) {
    g_holdings.reset();
    g_active_powers_pawn = nullptr;  // loads free the old components; rescan once settled
  }
  if (pawn == nullptr || now - g_last_holdings_tick < 500) return;  // possessing: keep the baseline
  g_last_holdings_tick = now;
  Holdings current = read_holdings(pawn);
  if (g_holdings) {
    const std::string where = location_of(pawn);
    report_changes(g_holdings->items, current.items, "item", where);
    report_changes(g_holdings->upgrades, current.upgrades, "upgrade", where);
    report_changes(g_holdings->ammo, current.ammo, "ammo", where);
    report_purchases(pawn, *g_holdings, current, where);
  }
  enforce_limits(pawn, now);
  restore_world_upgrades(pawn, now);
  g_holdings = read_holdings(pawn);  // after enforcement, so its removals aren't reported as changes
}



std::string upgrade_attributes(ue3::UObject* upgrade) {
  std::string out;
  const auto attributes = ue3::field<ue3::TArray<uint8_t>>(upgrade, ue3::kUpgradeAttributes);
  for (int32_t i = 0; i < attributes.count && attributes.data != nullptr; ++i) {
    out += std::format("{}{}={:g}", out.empty() ? "" : ",",
                       ue3::name_string(*reinterpret_cast<const ue3::FName*>(attributes.data + i * 0xC)),
                       *reinterpret_cast<const float*>(attributes.data + i * 0xC + 8));
  }
  return out.empty() ? "-" : out;
}

// Research: the pawn's OnRemoveAttributeModifier(attribute, modifier) with the borrowed
// default DisSeqAct_RemoveAttributeModifier, to find how an upgrade's effect is undone.
bool remove_attribute_modifier(ue3::UObject* player_controller, const std::string& args) {
  ue3::UObject* pawn = player_pawn(player_controller);
  if (pawn == nullptr) return false;
  std::istringstream words(args);
  std::string attribute, modifier;
  words >> attribute >> modifier;
  const int32_t attribute_index = ue3::find_name_index(attribute);
  const int32_t modifier_index = ue3::find_name_index(modifier);
  auto* function = ue3::find_object("Function DishonoredGame.DishonoredPlayerPawn.OnRemoveAttributeModifier");
  auto* action = ue3::find_object(
      "DisSeqAct_RemoveAttributeModifier DishonoredGame.Default__DisSeqAct_RemoveAttributeModifier");
  if (attribute_index < 0 || modifier_index < 0 || function == nullptr || action == nullptr) {
    log("removemod {}: not found (attribute name {}, modifier name {}, handler {})", args, attribute_index >= 0,
        modifier_index >= 0, function != nullptr && action != nullptr);
    return true;
  }
  auto* fields = reinterpret_cast<uint8_t*>(action);
  auto& attribute_slot = *reinterpret_cast<ue3::FName*>(fields + ue3::kRemoveModifierActionAttribute);
  auto& modifier_slot = *reinterpret_cast<ue3::FName*>(fields + ue3::kRemoveModifierActionModifier);
  const ue3::FName saved_attribute = attribute_slot, saved_modifier = modifier_slot;
  attribute_slot = {attribute_index, 0};
  modifier_slot = {modifier_index, 0};
  ue3::UObject* params = action;
  g_original(pawn, nullptr, function, &params, nullptr);
  attribute_slot = saved_attribute;
  modifier_slot = saved_modifier;
  log("removemod {}: called OnRemoveAttributeModifier", args);
  return true;
}

// Research: writes what the player has, and every upgrade the game knows, to the log.
// The visible area levels ("L_Ovrsr_P"; sublevels like L_Ovrsr_Script left out), comma-separated.
DWORD g_area_reported_for = 0;  // g_gameplay_started_tick of the last `area` event

std::string current_areas(ue3::UObject* pawn) {
  std::string areas;
  auto* world = pawn != nullptr ? ue3::field<ue3::UObject*>(pawn, ue3::kActorWorldInfo) : nullptr;
  if (world == nullptr) return "-";
  const auto streaming = ue3::field<ue3::TArray<ue3::UObject*>>(world, ue3::kWorldStreamingLevels);
  for (int32_t i = 0; i < streaming.count && streaming.data != nullptr; ++i) {
    auto* level = streaming.data[i];
    if (level == nullptr || !(ue3::field<uint32_t>(level, ue3::kStreamingFlags) & 1)) continue;
    const std::string name = ue3::name_string(ue3::field<ue3::FName>(level, ue3::kStreamingPackageName));
    if (name.size() > 2 && lower(name.substr(name.size() - 2)) == "_p") areas += (areas.empty() ? "" : ",") + name;
  }
  return areas.empty() ? "-" : areas;
}

void dump_player_state(ue3::UObject* player_controller) {
  ue3::UObject* pawn = player_pawn(player_controller);
  log("==== dump ====");
  for (auto* upgrade : ue3::find_instances(g_names.upgrade_tweaks)) {
    log("upgrade {} \"{}\" item={} parent={} attributes={}{}", ue3::object_name(upgrade),
        ue3::to_string(ue3::field<ue3::FString>(upgrade, ue3::kUpgradeDisplayName)),
        ue3::object_name(ue3::field<ue3::UObject*>(upgrade, ue3::kUpgradeHostItemType)),
        ue3::object_name(ue3::field<ue3::UObject*>(upgrade, ue3::kUpgradeParent)), upgrade_attributes(upgrade),
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
  for (const auto& [name, level] : power_levels(pawn)) log("power {} level {}", name, level);
  // Every travel prompt in the loaded level scripts, for mapping them to zones
  // (client/zones.py TRAVEL_PROMPTS) without walking through each door.
  static const int32_t scripted_choice = ue3::find_name_index("DisSeqAct_DialogScriptedChoice");
  for (auto* action : ue3::find_instances(scripted_choice)) {
    const auto choices = ue3::field<ue3::TArray<uint8_t>>(action, ue3::kScriptedChoiceChoices);
    for (int32_t i = 0; i < choices.count && i < 16 && choices.data != nullptr; ++i) {
      const uint8_t* choice = choices.data + i * ue3::kChoiceInfoSize;
      if (!(*reinterpret_cast<const uint32_t*>(choice + ue3::kChoiceInfoFlags) & 1)) continue;
      log("travel prompt {}#{} \"{}\"", ue3::path_name(action), i,
          ue3::to_string(*reinterpret_cast<const ue3::FString*>(choice)));
    }
  }
  // Which areas are loaded: the current zone after a save load (which emits no mapchange).
  if (auto* world = ue3::field<ue3::UObject*>(pawn, ue3::kActorWorldInfo)) {
    const auto streaming = ue3::field<ue3::TArray<ue3::UObject*>>(world, ue3::kWorldStreamingLevels);
    for (int32_t i = 0; i < streaming.count && streaming.data != nullptr; ++i) {
      auto* level = streaming.data[i];
      if (level == nullptr) continue;
      const uint32_t flags = ue3::field<uint32_t>(level, ue3::kStreamingFlags);
      const bool loaded = ue3::field<ue3::UObject*>(level, ue3::kStreamingLoadedLevel) != nullptr;
      if (!loaded && !(flags & 1)) continue;
      log("streaming level {} loaded={} visible={}", ue3::name_string(ue3::field<ue3::FName>(level, ue3::kStreamingPackageName)),
          loaded, flags & 1);
    }
  }
  auto* inventory = ue3::field<ue3::UObject*>(pawn, ue3::kPawnInventory);
  if (inventory == nullptr) return;
  const auto items = ue3::field<ue3::TArray<uint8_t>>(inventory, ue3::kInventorySlots);
  for (int32_t i = 0; i < items.count && items.data != nullptr; ++i) {
    auto* item = *reinterpret_cast<ue3::UObject**>(items.data + i * 0xC);
    auto* required = *reinterpret_cast<ue3::UObject**>(items.data + i * 0xC + 4);
    log("inventory slot {}: {} (requires {})", i, ue3::full_name(item), ue3::object_name(required));
  }
  for (int usage = 0; usage < 3; ++usage) {
    log("hand {} (EDisEquipUsage) holds slot {}", usage,
        ue3::field<int32_t>(inventory, ue3::kInventoryEquipUsage + usage * ue3::kEquipUsageSize));
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
  if (request.rfind("removemod ", 0) == 0) return remove_attribute_modifier(player_controller, request.substr(10));
  if (request == "quittomenu") {
    // The pause menu's "quit to main menu" confirmation (confirmed; "disconnect" is stripped).
    // Its object (pGlobalUIManager.pPauseMenu) has existed in every session seen; if it's
    // missing, load the main menu map directly (untested).
    auto* menu = ue3::find_instance("DisGFxMoviePlayerPauseMenu");
    auto* quit = ue3::find_object("Function DishonoredGame.DisGFxMoviePlayerPauseMenu.OnQuitGameConfirm");
    log("quittomenu: pause menu {}, OnQuitGameConfirm {}", ue3::full_name(menu), quit != nullptr);
    if (menu != nullptr && quit != nullptr) {
      g_original(menu, nullptr, quit, nullptr, nullptr);
    } else {
      run_console_command(player_controller, "open Dishonored_MainMenu");
    }
    return true;
  }
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

// Should this travel option be refused? Options are named "<choice action path>#<index>";
// the client lists those leading into zones whose key hasn't been received. Some level
// scripts serve two missions (Galvani's offices), so an option can also be listed as
// "<area>|<option>" for the area the player is in. "all" (for testing) refuses every one.
bool travel_refused(const std::string& option, const std::string& areas) {
  if (allowed_in(g_travel_allowed, "all") == false || allowed_in(g_travel_allowed, lower(option)) == false) return true;
  size_t start = 0;
  while (start <= areas.size()) {
    const size_t end = (std::min)(areas.find(',', start), areas.size());
    if (allowed_in(g_travel_allowed, lower(areas.substr(start, end - start) + "|" + option)) == false) return true;
    start = end + 1;
  }
  return false;
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
    // Mission scripts also destroy pickups they don't use (Boyle's mansion removes the
    // other variants' paintings and keys): those are far from the player. Real pickups
    // have been within ~300 units.
    bool distant = false;
    if (!during_load) {
      auto* controller = local_player_controller();
      auto* pawn = controller ? ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn) : nullptr;
      if (pawn != nullptr) {
        const auto me = ue3::field<ue3::FVector>(pawn, ue3::kActorLocation);
        const float dx = at.x - me.x, dy = at.y - me.y, dz = at.z - me.z;
        distant = dx * dx + dy * dy + dz * dz > kPickupReach * kPickupReach;
      }
    }
    emit_event(std::format("pickup {} {} {} at={:.0f},{:.0f},{:.0f}{}", ue3::path_name(self), ue3::class_name(self),
                           pickup_item_name(self), at.x, at.y, at.z,
                           during_load ? " (load)" : distant ? " (far)" : ""));
  } else if (name == g_names.client_prepare_map_change) {
    const std::string level = ue3::name_string(*static_cast<ue3::FName*>(params));
    emit_event("mapchange " + level);
    g_in_void = lower(level).find("outsiderdream") != std::string::npos;
    g_in_prologue = lower(level) == "l_tower_p";
    if (level.find("MainMenu") != std::string::npos) {
      g_menu_exit_pending = false;
      if (!g_in_menu) {
        g_in_menu = true;
        log("in main menu");
      }
      // Quitting to the menu drops the pawn before this mapchange; the menu's background
      // pawn then appears. Only a pawn lost after this point means a level is loading.
      g_ever_lost_gameplay = false;
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
  } else if (name == g_names.player_choice_confirm) {
    // A player choice was confirmed. Travel prompts (and the wine, the still, ...) are
    // scripted choices: a level-script DisSeqAct_DialogScriptedChoice held by the player's
    // scripted-choice state. Each choice says whether it leads to a map change. Dialogue
    // uses conversation nodes held by the dialog state instead.
    int32_t& index = *static_cast<int32_t*>(params);
    const int32_t picked = index;
    std::string text = "?", title = "", node = "None";
    bool travel = false;
    for (auto* state : ue3::find_instances(ue3::find_name_index("StatePlayerMasterInScriptedChoice"))) {
      auto* action = ue3::field<ue3::UObject*>(state, ue3::kScriptedChoiceActive);
      if (action == nullptr) continue;
      node = ue3::path_name(action);
      title = ue3::to_string(ue3::field<ue3::FString>(action, ue3::kScriptedChoiceTitle));
      const auto choices = ue3::field<ue3::TArray<uint8_t>>(action, ue3::kScriptedChoiceChoices);
      const auto travels = [&](int32_t i) {
        return (*reinterpret_cast<const uint32_t*>(choices.data + i * ue3::kChoiceInfoSize + ue3::kChoiceInfoFlags) & 1) != 0;
      };
      if (index >= 0 && index < choices.count && choices.data != nullptr) {
        text = ue3::to_string(*reinterpret_cast<const ue3::FString*>(choices.data + index * ue3::kChoiceInfoSize));
        travel = travels(index);
      }
      if (travel) {
        // The prompt's path and the option say where it goes (the destination is in the
        // next level's script, which isn't loaded yet); the client maps them to zones.
        const std::string option = std::format("{}#{}", node, index);
        auto* controller = local_player_controller();
        const std::string areas =
            current_areas(controller ? ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn) : nullptr);
        emit_event(std::format("travel {} \"{}\" from {}", option, text, areas));
        if (g_limits_known && travel_refused(option, areas)) {
          // Pick the prompt's "stay" option instead: the prompt closes as if declined.
          int32_t stay = -1;
          for (int32_t i = 0; i < choices.count && stay < 0; ++i) {
            if (!travels(i)) stay = i;
          }
          if (stay >= 0) {
            emit_event(std::format("blocked travel {} from {}", option, areas));
            if (allowed_in(g_travel_allowed, "all") == false) {
              show_message("{FA8072}Travel refused (test: limit travel all 0)");
            }
            index = stay;
          } else {
            log("travel prompt has no stay option; not refused");
          }
        }
      }
      break;
    }
    if (node == "None") {
      for (auto* state : ue3::find_instances(ue3::find_name_index("StatePlayerMasterInDialog"))) {
        auto* choice = ue3::field<ue3::UObject*>(state, ue3::kDialogActiveChoice);
        if (choice == nullptr) continue;
        node = ue3::path_name(choice);
        const auto texts = ue3::field<ue3::TArray<ue3::FString>>(choice, ue3::kPlayerChoiceTexts);
        if (index >= 0 && index < texts.count && texts.data != nullptr) text = ue3::to_string(texts.data[index]);
        break;
      }
    }
    emit_event(std::format("choice {} \"{}\" title \"{}\" travel={} by {}", picked, text, title, travel ? 1 : 0, node));
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
  g_names.power_buy = ue3::find_name_index("PowerBuy");
  g_names.active_power_component = ue3::find_name_index("DishonoredActivePowerComponent");
  g_names.req_buy_item = ue3::find_name_index("Req_BuyItem");
  g_names.world_upgrade = ue3::find_name_index("DisUpgrade");
  g_names.player_pawn_tweaks = ue3::find_name_index("DisTweaks_PlayerPawn");
  g_names.ranged_weapon = ue3::find_name_index("DishonoredWeapon_Ranged");
  g_names.sword = ue3::find_name_index("DishonoredWepSword");
  g_names.sword_tweaks = ue3::find_name_index("DisTweaks_WepSword");
  g_names.post_render = ue3::find_name_index("PostRender");
  g_names.player_choice_confirm = ue3::find_name_index("OnPlayerChoiceConfirm");
  g_names.hud = ue3::find_name_index("HUD");
  g_names.pickup_base = ue3::find_name_index("DisPickup_Base");
  g_names.abstract_item_pickup = ue3::find_name_index("DisAbstractItemPickup");
  g_names.whale_bone_charm = ue3::find_name_index("DisWhaleBoneCharm");
  g_names.player_pawn = ue3::find_name_index("DishonoredPlayerPawn");
  g_names.upgrade_tweaks = ue3::find_name_index("DisTweaks_Upgrade");
  g_console_command = console_command;
  log("found DishonoredEngine ({}) and PlayerController.ConsoleCommand", ue3::object_name(g_engine));
}

// ---- On-screen messages ("Received X from Y") ----
// Drawn with the HUD's Canvas right after the game's own HUD.PostRender, every frame.
constexpr DWORD kMessageMs = 7000;  // how long each line stays (the last second fades)
constexpr size_t kMaxMessages = 6;

struct Message {
  std::wstring text;
  DWORD shown_at;
};
std::mutex g_messages_mutex;
std::deque<Message> g_messages;
// A notice for the main menu (why the player was sent back there), drawn only in the
// menu, centered at the top, until a game is loaded or kNoticeMs has passed.
constexpr DWORD kNoticeMs = 60000;
Message g_notice;

void clear_notice() {
  std::lock_guard lock(g_messages_mutex);
  g_notice.text.clear();
}

// A message may contain colour markers "{RRGGBB}" (hex); text after one is drawn in that
// colour, default white. The client uses them for Archipelago's item/player colours.
struct Segment {
  std::wstring text;
  uint8_t r, g, b;
};

std::vector<Segment> parse_segments(const std::wstring& text) {
  std::vector<Segment> out;
  uint8_t r = 255, g = 255, b = 255;
  std::wstring current;
  auto hex = [](wchar_t c) { return iswxdigit(c) != 0; };
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == L'{' && i + 7 < text.size() && text[i + 7] == L'}' &&
        std::all_of(text.begin() + i + 1, text.begin() + i + 7, hex)) {
      if (!current.empty()) out.push_back({current, r, g, b});
      current.clear();
      const unsigned long rgb = std::stoul(text.substr(i + 1, 6), nullptr, 16);
      r = static_cast<uint8_t>(rgb >> 16);
      g = static_cast<uint8_t>(rgb >> 8);
      b = static_cast<uint8_t>(rgb);
      i += 7;
    } else {
      current.push_back(text[i]);
    }
  }
  if (!current.empty()) out.push_back({current, r, g, b});
  return out;
}

bool game_paused() {
  static ue3::UFunction* is_paused = ue3::find_object("Function Engine.PlayerController.IsPaused");
  ue3::UObject* controller = local_player_controller();
  if (is_paused == nullptr || controller == nullptr) return false;
  uint32_t paused = 0;
  g_original(controller, nullptr, is_paused, &paused, nullptr);
  return paused & 1;
}

// Draws one line of colour segments at (x, y) and returns its width; with draw = false it
// only measures. Canvas functions are called through the original ProcessEvent.
float draw_segments(ue3::UObject* canvas, const std::wstring& line, float x, float y, uint8_t alpha, float scale,
                    bool draw) {
  static ue3::UFunction* set_pos = ue3::find_object("Function Engine.Canvas.SetPos");
  static ue3::UFunction* set_color = ue3::find_object("Function Engine.Canvas.SetDrawColor");
  static ue3::UFunction* draw_text = ue3::find_object("Function Engine.Canvas.DrawText");
  static ue3::UFunction* text_size = ue3::find_object("Function Engine.Canvas.TextSize");
  if (set_pos == nullptr || set_color == nullptr || draw_text == nullptr || text_size == nullptr) return 0;
  const float start = x;
  for (const auto& segment : parse_segments(line)) {
    const ue3::FString text{segment.text.c_str(), static_cast<int32_t>(segment.text.size() + 1),
                            static_cast<int32_t>(segment.text.size() + 1)};
    if (draw) {
      struct {
        float x, y, z;
      } pos{x, y, 0};
      g_original(canvas, nullptr, set_pos, &pos, nullptr);
      uint8_t color[4] = {segment.r, segment.g, segment.b, alpha};
      g_original(canvas, nullptr, set_color, color, nullptr);
      struct {
        ue3::FString text;
        uint32_t carriage_return;
        float x_scale, y_scale;
        uint32_t render_flags;  // bClipText (bit 0), bEnableShadow (bit 1)
        uint8_t glow[0x24];
      } params{};
      params.text = text;
      params.x_scale = params.y_scale = scale;  // optional params arrive as given through ProcessEvent: 0 = invisible
      params.render_flags = 2;
      g_original(canvas, nullptr, draw_text, &params, nullptr);
    }
    struct {
      ue3::FString text;
      float width, height;
    } size{text, 0, 0};
    g_original(canvas, nullptr, text_size, &size, nullptr);  // unscaled size of the text
    x += size.width * scale;
  }
  return x - start;
}

void draw_messages(ue3::UObject* hud) {
  auto* canvas = ue3::field<ue3::UObject*>(hud, ue3::kHudCanvas);
  if (canvas == nullptr) return;
  std::vector<Message> lines;
  Message notice;
  const DWORD now = GetTickCount();
  {
    std::lock_guard lock(g_messages_mutex);
    while (!g_messages.empty() && now - g_messages.front().shown_at > kMessageMs) g_messages.pop_front();
    lines.assign(g_messages.begin(), g_messages.end());
    if (!g_notice.text.empty() && now - g_notice.shown_at > kNoticeMs) g_notice.text.clear();
    notice = g_notice;
  }
  // Messages stay out of menus and the journal; the notice is only for the main menu.
  const bool show_lines = !lines.empty() && !g_in_menu && !game_paused();
  const bool show_notice = !notice.text.empty() && g_in_menu;
  if (!show_lines && !show_notice) return;
  static ue3::UObject* fallback_font = nullptr;
  static bool font_logged = false;
  auto& font = *reinterpret_cast<ue3::UObject**>(reinterpret_cast<uint8_t*>(canvas) + ue3::kCanvasFont);
  if (font == nullptr) {
    if (fallback_font == nullptr) fallback_font = ue3::find_object("Font EngineFonts.SmallFont");
    if (fallback_font == nullptr) return;
    font = fallback_font;
  }
  if (!font_logged) {
    font_logged = true;
    log("messages: drawing with font {}", ue3::full_name(font));
  }
  const float height = ue3::field<float>(canvas, ue3::kCanvasClipY);
  const float width = ue3::field<float>(canvas, ue3::kCanvasClipX);
  const float scale = height > 0 ? height / 720.0f : 1.0f;
  if (show_notice) {
    static bool notice_logged = false;
    if (!notice_logged) {
      notice_logged = true;
      log("messages: drawing the main menu notice");
    }
    const float notice_scale = scale * 1.25f;
    const float text_width = draw_segments(canvas, notice.text, 0, 0, 255, notice_scale, false);
    draw_segments(canvas, notice.text, (width - text_width) / 2, height * 0.05f, 255, notice_scale, true);
  }
  if (!show_lines) return;
  const float line = 22.0f * scale;
  float y = height * 0.90f - line * static_cast<float>(lines.size());  // bottom left, oldest on top
  for (const auto& message : lines) {
    const DWORD age = now - message.shown_at;
    const uint8_t alpha = age + 1000 > kMessageMs ? static_cast<uint8_t>(255 * (kMessageMs - age) / 1000) : 255;
    draw_segments(canvas, message.text, width * 0.02f, y, alpha, scale, true);
    y += line;
  }
}

void __fastcall process_event_hook(ue3::UObject* self, void* edx, ue3::UFunction* function, void* params,
                                   void* result) {
  // Messages are drawn after the HUD's own frame, so they sit on top of it.
  if (function != nullptr && self != nullptr && g_names.post_render >= 0 &&
      function->name.index == g_names.post_render && !g_shutting_down && !g_inside_hook_work &&
      ue3::is_a(self, g_names.hud)) {
    g_original(self, edx, function, params, result);
    g_inside_hook_work = true;
    draw_messages(self);
    g_inside_hook_work = false;
    return;
  }
  if (g_names.pre_exit >= 0 && function != nullptr && function->name.index == g_names.pre_exit &&
      !g_shutting_down) {
    g_shutting_down = true;
    log("game is exiting; going idle");
  }
  if (self != nullptr && function != nullptr && g_engine_ready && !g_inside_hook_work && !g_shutting_down) {
    g_inside_hook_work = true;
    if (g_console_command == nullptr) resolve_engine_objects();
    if (g_config.call_log != CallLogMode::Off && g_call_logging) log_call(self, function);
    if (g_console_command != nullptr) {
      detect_events(self, function, params);
      if (should_block(self, function->name.index, params)) {
        g_inside_hook_work = false;
        return;  // skip the original: the purchase or grant doesn't happen
      }
    }
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
          clear_notice();
          emit_event("gameplay");
        }
      }
      if (in_gameplay && !g_in_gameplay) {
        g_gameplay_started_tick = now;
        if (g_ever_lost_gameplay && g_in_menu) {
          g_in_menu = false;  // came back from a loading screen: a real level
          log("left main menu");
          clear_notice();
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
      if (settled && g_area_reported_for != g_gameplay_started_tick) {
        // Once per load or transition: where the player is (a save load emits no mapchange).
        g_area_reported_for = g_gameplay_started_tick;
        emit_event("area " + current_areas(ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn)));
      }
      if (in_gameplay) track_holdings(controller, settled, now);
      else {
        g_holdings.reset();
        g_active_powers_pawn = nullptr;
      }
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

void show_notice(const std::string& text) {
  std::lock_guard lock(g_messages_mutex);
  g_notice = {widen(text), GetTickCount()};
}

void show_message(const std::string& text) {
  std::lock_guard lock(g_messages_mutex);
  g_messages.push_back({widen(text), GetTickCount()});
  while (g_messages.size() > kMaxMessages) g_messages.pop_front();
}

bool set_limit(const std::string& kind, const std::string& name, int32_t value) {
  std::lock_guard lock(g_limits_mutex);
  if (kind == "power") {
    g_power_limits[lower(name)] = value;
  } else if (kind == "upgrade") {
    g_upgrade_allowed[name] = value > 0;
  } else if (kind == "bonecharms" && name == "slots") {
    g_bone_charm_slots = value;
  } else if (kind == "ammo") {
    g_ammo_allowed[name] = value > 0;
  } else if (kind == "item") {
    g_item_allowed[name] = value > 0;
  } else if (kind == "equip") {
    g_equip_allowed[name] = value > 0;
  } else if (kind == "travel") {
    const auto it = g_travel_allowed.find(lower(name));
    if (it == g_travel_allowed.end() || it->second != (value > 0)) {
      log("travel {} {}", value > 0 ? "allowed:" : "refused:", name);
    }
    g_travel_allowed[lower(name)] = value > 0;
  } else {
    return false;
  }
  if (!g_limits_known) log("limits received from the client: blocking and removal are on");
  g_limits_known = true;
  return true;
}

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
