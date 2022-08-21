#pragma once

#include <string>
#include <optional>

namespace FlagMod {

template <typename T>
std::optional<T> str_to_value(const std::string &input);

template <> inline std::optional<std::string> str_to_value(const std::string &input) { return input; }
template <> inline std::optional<int> str_to_value(const std::string &input) {
	const char *nptr = input.c_str();
	char *endptr = NULL;
	long out = strtol(nptr, &endptr, 0);

	if(nptr == endptr) return std::nullopt;
	return out;
}

}
