#pragma once

// The handful of Unreal Engine 3 structures we need, laid out as in Dishonored's
// 32-bit build (layouts from the generated SDK in ectrc/dismod; see
// research/prior-art.md). Only what's needed to find objects by name and call
// functions through ProcessEvent.

#include <cstddef>
#include <cstdint>
#include <string>

namespace ue3 {

template <typename T>
struct TArray {
  T* data;
  int32_t count;
  int32_t max;
};

struct FVector {
  float x, y, z;
};

struct FName {
  int32_t index;
  int32_t number;
};

struct FNameEntry {
  uint64_t flags;
  int32_t index;  // low bit set = name stored as wide chars
  FNameEntry* hash_next;
  char name[1];  // really variable length, starts at 0x10
};
static_assert(offsetof(FNameEntry, name) == 0x10);

struct UObject {
  void** vtable;
  int32_t internal_index;
  uint64_t flags;
  void* hash_next;
  void* hash_outer_next;
  void* state_frame;
  UObject* linker;
  void* linker_index;
  UObject* outer;
  FName name;
  UObject* klass;
  UObject* archetype;
};
static_assert(offsetof(UObject, outer) == 0x24);
static_assert(offsetof(UObject, name) == 0x28);
static_assert(offsetof(UObject, klass) == 0x30);

using UFunction = UObject;

// Field offsets inside larger classes (from the SDK), read as raw memory.
inline constexpr size_t kEngineGamePlayers = 0x480;  // UEngine::GamePlayers (TArray<ULocalPlayer*>)
inline constexpr size_t kPlayerActor = 0x3C;         // UPlayer::Actor (APlayerController*)
inline constexpr size_t kControllerPawn = 0x248;     // AController::Pawn
inline constexpr size_t kPlayerControllerCheatManager = 0x45C;  // APlayerController::CheatManager
inline constexpr size_t kActorLocation = 0xC4;                   // AActor::Location (FVector)
inline constexpr size_t kStructSuperField = 0x44;                // UStruct::SuperField (parent class)
inline constexpr size_t kPickupTweaks = 0x3B8;                   // ADisAbstractItemPickup / ADisWhaleBoneCharm::m_pTweaks
inline constexpr size_t kAbstractPickupTweaksItem = 0xDC;        // UDisTweaks_AbstractItemPickup::m_pAbstractItem
inline constexpr size_t kAddAbstractItemActionItem = 0xF8;       // UDisSeqAct_AddAbstractItem::m_pItemToAdd
inline constexpr size_t kObjectiveActionObjectives = 0xFC;       // UDisSeqAct_ObjectiveAction::m_Objectives

template <typename T>
T field(const UObject* object, size_t offset) {
  return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(object) + offset);
}

// FString is a TArray<wchar_t> whose count includes the terminating null.
struct FString {
  const wchar_t* data;
  int32_t count;
  int32_t max;
};
static_assert(sizeof(FString) == 12);

// Points at the name/object tables. Returns false (with a reason) if the offsets
// don't look right for this build of the game.
bool init(uintptr_t gnames_address, uintptr_t gobjects_address, std::string& error);
bool ready();

std::string name_string(FName name);
int32_t find_name_index(const std::string& name);  // -1 if absent

std::string object_name(const UObject* object);
std::string class_name(const UObject* object);
// "Function Engine.PlayerController.ConsoleCommand"
std::string full_name(const UObject* object);
// Same without the class: "L_Prison_P.TheWorld.PersistentLevel.DisWhaleBoneCharm_0"
std::string path_name(const UObject* object);
// Outermost package, e.g. the map an actor belongs to ("L_Prison_P").
std::string package_name(const UObject* object);
// True if the object's class is `class_name_index` or inherits from it.
bool is_a(const UObject* object, int32_t class_name_index);
// Finds an object by full name. Linear scan: cache the result.
UObject* find_object(const std::string& full);
// First object of this class that isn't a class default object ("Default__...").
UObject* find_instance(const std::string& class_name);

}  // namespace ue3
