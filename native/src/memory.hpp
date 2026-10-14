#pragma once

#include <cstdint>
#include <string_view>

namespace ap {

// Finds an IDA-style byte pattern ("55 8B EC ?? 6A") in the main executable's code.
// Returns 0 if not found, or if it matches more than once.
uintptr_t find_pattern(std::string_view pattern);

}  // namespace ap
