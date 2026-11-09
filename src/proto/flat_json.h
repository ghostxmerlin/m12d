#pragma once
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

// Parses a single-level JSON object. Values may be strings, numbers, true, false, null.
// Returns nullopt for anything else: nested objects or arrays, more than 32 keys,
// duplicate keys, malformed JSON, or trailing non-whitespace.
using FlatValue = std::variant<std::nullptr_t, bool, double, std::string>;
std::optional<std::map<std::string, FlatValue>> parse_flat_json(std::string_view s);
