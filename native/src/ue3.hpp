#pragma once

// The handful of Unreal Engine 3 structures we need, laid out as in Dishonored's
// 32-bit build (layouts from the generated SDK in ectrc/dismod; see
// research/prior-art.md). Only what's needed to find objects by name and call
// functions through ProcessEvent.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
inline constexpr size_t kPlayerControllerCheatClass = 0x460;    // APlayerController::CheatClass
inline constexpr size_t kActorLocation = 0xC4;                   // AActor::Location (FVector)
inline constexpr size_t kStructSuperField = 0x44;                // UStruct::SuperField (parent class)
inline constexpr size_t kPickupTweaks = 0x3B8;                   // ADisAbstractItemPickup / ADisWhaleBoneCharm::m_pTweaks
inline constexpr size_t kAbstractPickupTweaksItem = 0xDC;        // UDisTweaks_AbstractItemPickup::m_pAbstractItem
inline constexpr size_t kAddAbstractItemActionItem = 0xF8;       // UDisSeqAct_AddAbstractItem::m_pItemToAdd
inline constexpr size_t kObjectiveActionObjectives = 0xFC;       // UDisSeqAct_ObjectiveAction::m_Objectives
inline constexpr size_t kGiveUpgradeActionUpgrade = 0xF8;        // UDisSeqAct_GiveUpgrade::m_pUpgrade
inline constexpr size_t kRemovePowerActionName = 0xF8;           // UDisSeqAct_RemovePower::m_PowerName (FString)
inline constexpr size_t kPlayerPawnPowers = 0xA38;               // ADishonoredPlayerPawn::m_pPowersComponent
inline constexpr size_t kPowersComponentPowers = 0x54;           // UDishonoredPowersComponent::m_Powers (FDishonoredPower)
inline constexpr size_t kPowerSize = 0x1C;                       // FDishonoredPower: m_Name (FName) at 0
inline constexpr size_t kPowerCurrentLevel = 0x18;               // FDishonoredPower::m_CurrentLevel
inline constexpr size_t kActivePowerName = 0x6C;                 // UDishonoredActivePowerComponent::m_PowerName (FName)
inline constexpr size_t kActivePowerOwner = 0x80;                // UDishonoredActivePowerComponent::m_pOwnerPlayerPawn
inline constexpr size_t kActivePowerLevel = 0x88;                // UDishonoredActivePowerComponent::m_CurrentLevel
inline constexpr size_t kGivePickupActionPickup = 0xF8;          // UDisSeqAct_GivePickup::m_pPickup
inline constexpr size_t kAddInventoryActionItem = 0xF8;          // UDisSeqAct_AddInventoryItem::m_pItemToAdd
inline constexpr size_t kAddPowerActionName = 0xF8;              // UDisSeqAct_AddPower::m_PowerName (FString)
inline constexpr size_t kAddPowerActionLevel = 0x104;            // UDisSeqAct_AddPower::m_PowerLevel
inline constexpr size_t kPawnInventory = 0x59C;                  // ADishonoredPawn::m_pInventory
inline constexpr size_t kPlayerPawnBoneCharmSlots = 0xB10;       // ADishonoredPlayerPawn::m_WhaleBoneCharmSlots[10]
inline constexpr size_t kPlayerPawnUpgrades = 0xB50;             // ADishonoredPlayerPawn::m_Upgrades
inline constexpr size_t kStoreDisplayedItems = 0x1C4;            // UDisGFxMoviePlayerStore::m_DisplayedItems (FDisStoreItem*?)
inline constexpr size_t kStoreItemItem = 0x5C;                   // FDisStoreItem::m_Item (UDisTweaksBase*)
inline constexpr size_t kWorldUpgradeTweaks = 0x254;             // ADisUpgrade::m_pTweaks (an upgrade lying in the world)
inline constexpr size_t kUpgradeAttributes = 0x9C;               // UDisTweaks_Upgrade::m_Attributes (FName, float: 0xC each)
inline constexpr size_t kRemoveModifierActionAttribute = 0xF8;   // UDisSeqAct_RemoveAttributeModifier::m_AttributeName
inline constexpr size_t kRemoveModifierActionModifier = 0x100;   // UDisSeqAct_RemoveAttributeModifier::m_ModifierName
inline constexpr size_t kUpgradeHostItemType = 0x90;             // UDisTweaks_Upgrade::m_pHostItemType (UClass*)
inline constexpr size_t kUpgradeDisplayName = 0xB8;              // UDisTweaks_Upgrade::m_Name (FString, localized)
inline constexpr size_t kUpgradeParent = 0xE0;                   // UDisTweaks_Upgrade::m_pParentUpgrade
inline constexpr size_t kInventorySlots = 0x38;                  // UDishonoredInventory::m_Slots (FPawnInventorySlot, 0xC each)
inline constexpr size_t kInventoryAmmo = 0xBC;                   // UDishonoredInventory::m_AmmoInfo (count, capacity)
inline constexpr size_t kInventoryElixirs = 0xD4;                // UDishonoredInventory::m_ElixirCounts[2] (health, mana)
inline constexpr size_t kInventoryAbstractItems = 0xC8;          // UDishonoredInventory::m_AbstractItem (item, quantity, flags)

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
// True if [address, address + size) is committed, readable memory (slow: a system call).
bool is_readable(const void* address, size_t size);

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
// Every object of this exact class (by name index), class default objects excluded.
std::vector<UObject*> find_instances(int32_t class_name_index);
// Same, but also objects of subclasses.
std::vector<UObject*> find_instances_of_subclasses(int32_t class_name_index);
std::string to_string(const FString& text);

}  // namespace ue3
