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
// Finds an object by full name. Linear scan: cache the result.
UObject* find_object(const std::string& full);

}  // namespace ue3
