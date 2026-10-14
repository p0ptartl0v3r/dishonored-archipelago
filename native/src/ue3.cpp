#include "ue3.hpp"

#include <windows.h>

#include <cstring>

namespace ue3 {
namespace {

TArray<FNameEntry*>* g_names = nullptr;
TArray<UObject*>* g_objects = nullptr;

// Reading through a bad pointer during validation must not crash the game.
bool readable(const void* address, size_t size) {
  MEMORY_BASIC_INFORMATION info{};
  if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0) return false;
  if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
  constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
  return (info.Protect & kReadable) && reinterpret_cast<uintptr_t>(address) + size <= end;
}

template <typename T>
bool safe_read(const void* address, T& out) {
  if (!readable(address, sizeof(T))) return false;
  std::memcpy(&out, address, sizeof(T));
  return true;
}

std::string entry_string(const FNameEntry* entry) {
  if (entry == nullptr) return {};
  if (entry->index & 1) {
    std::string out;
    for (auto* c = reinterpret_cast<const wchar_t*>(entry->name); *c; ++c) out.push_back(static_cast<char>(*c));
    return out;
  }
  return entry->name;
}

}  // namespace

bool init(uintptr_t gnames_address, uintptr_t gobjects_address, std::string& error) {
  auto* names = reinterpret_cast<TArray<FNameEntry*>*>(gnames_address);
  auto* objects = reinterpret_cast<TArray<UObject*>*>(gobjects_address);

  TArray<FNameEntry*> name_table{};
  TArray<UObject*> object_table{};
  if (!safe_read(names, name_table) || !safe_read(objects, object_table)) {
    error = "name/object table address is not readable";
    return false;
  }
  if (name_table.count <= 0 || name_table.count > name_table.max || object_table.count <= 0 ||
      object_table.count > object_table.max) {
    error = "name/object tables are empty or malformed (engine not started yet, or wrong offsets)";
    return false;
  }
  FNameEntry* first = nullptr;
  char first_chars[5] = {};
  if (!safe_read(name_table.data, first) || first == nullptr || !safe_read(first->name, first_chars) ||
      std::string(first_chars, 4) != "None") {
    error = "name 0 is not \"None\" (wrong GNames offset for this game build)";
    return false;
  }
  g_names = names;
  g_objects = objects;
  return true;
}

bool ready() { return g_names != nullptr && g_objects != nullptr; }

std::string name_string(FName name) {
  if (!ready() || name.index < 0 || name.index >= g_names->count) return {};
  std::string out = entry_string(g_names->data[name.index]);
  if (name.number > 0) out += "_" + std::to_string(name.number - 1);
  return out;
}

int32_t find_name_index(const std::string& name) {
  if (!ready()) return -1;
  for (int32_t i = 0; i < g_names->count; ++i) {
    if (g_names->data[i] != nullptr && entry_string(g_names->data[i]) == name) return i;
  }
  return -1;
}

std::string object_name(const UObject* object) { return object ? name_string(object->name) : "None"; }

std::string class_name(const UObject* object) { return object ? object_name(object->klass) : "None"; }

std::string full_name(const UObject* object) {
  if (object == nullptr || object->klass == nullptr) return "None";
  std::string path = object_name(object);
  for (const UObject* outer = object->outer; outer != nullptr; outer = outer->outer) {
    path = object_name(outer) + "." + path;
  }
  return class_name(object) + " " + path;
}

UObject* find_object(const std::string& full) {
  if (!ready()) return nullptr;
  // Compare the cheap short name first; only build full names for candidates.
  const auto last = full.find_last_of(". ");
  const int32_t short_name = find_name_index(full.substr(last + 1));
  if (short_name < 0) return nullptr;
  for (int32_t i = 0; i < g_objects->count; ++i) {
    UObject* object = g_objects->data[i];
    if (object != nullptr && object->name.index == short_name && full_name(object) == full) return object;
  }
  return nullptr;
}

UObject* find_instance(const std::string& wanted_class) {
  if (!ready()) return nullptr;
  const int32_t class_index = find_name_index(wanted_class);
  if (class_index < 0) return nullptr;
  for (int32_t i = 0; i < g_objects->count; ++i) {
    UObject* object = g_objects->data[i];
    if (object != nullptr && object->klass != nullptr && object->klass->name.index == class_index &&
        object_name(object).rfind("Default__", 0) != 0) {
      return object;
    }
  }
  return nullptr;
}

}  // namespace ue3
