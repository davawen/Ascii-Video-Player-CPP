#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <array>
#include <optional>
#include <tuple>
#include <functional>

#include "fmt/core.h"

namespace FlagMod {
	constexpr auto nullopt = std::nullopt;

	namespace detail {

	template <typename T>
	std::optional<T> str_to_value(const std::string &input);

	template <> std::optional<std::string> str_to_value(const std::string &input) { return input; }
	template<> std::optional<int> str_to_value(const std::string &input) {
		const char *nptr = input.c_str();
		char *endptr = NULL;
		long out = strtol(nptr, &endptr, 0);

		if(nptr == endptr) return nullopt;
		return out;
	}

	}
struct RequiredFlagNotGivenError : std::exception {
	std::string name;

	RequiredFlagNotGivenError(std::string name) : name(name) {}
};

struct InvalidFlagInput : std::exception {
	std::string name, input;

	InvalidFlagInput(std::string name, std::string input) : name(name), input(input) {}
};
	
template <typename T, bool Required>
struct Flag {
	using type = T;
	using out = std::conditional_t<Required, T, std::optional<T>>;

	std::string name;
	std::optional<char> short_name;
	std::string help;

	std::optional<T> default_value;
	std::optional<std::string> value;
};

struct Switch {
	std::string name;
	std::optional<char> short_name;
	std::string help;

	bool value;
};

template <typename T>
struct Positional {
	std::string label;
	
	std::optional<std::string> value;
};

class Flags {
	std::vector<std::string> args;

	/// Base case for `flag_present`
	std::pair<std::nullptr_t, size_t> flag_present(const std::string &) {
		return { nullptr, 0 };
	}

	/// Ignore non-flag arguments
	template <typename T, typename ... Ts>
	auto flag_present(const std::string &str, T &, Ts & ... flags) {
		return flag_present(str, flags...);
	}

	/// @returns { nullptr, _ } if flag isn't detected, else returns a pointer to the found flag's string value and the position of the character right after the flag in the string
	template <typename T, bool R, typename ... Ts>
	std::pair<std::optional<std::string> *, size_t> flag_present(const std::string &str, Flag<T, R> &flag, Ts & ... flags) {
		if(str.starts_with("--" + flag.name)) {
			return { &flag.value, flag.name.size() + 2 };
		}
		else if(flag.short_name.has_value() && str.starts_with(std::string("-") + *flag.short_name)) {
			return { &flag.value, 2 };
		}
		else {
			return flag_present(str, flags...); // Calls base case if no flag remain
		}
	}

	/// Base case for `switch_present`
	bool switch_present(const std::string &) {
		return false;
	}

	/// Ignore non-switch arguments
	template <typename T, typename ... Ts>
	bool switch_present(const std::string &str, T &, Ts & ... flags) {
		return switch_present(str, flags...);
	}

	/// Turns on switches present in the given string
	/// @return wether at least one switch was present
	template <typename ... Ts>
	bool switch_present(const std::string &str, Switch &s, Ts & ... flags) {
		if(
			str.starts_with("--" + s.name) ||
			(s.short_name.has_value() && str.starts_with('-') && str.find(*s.short_name) != std::string::npos)
		) {
			s.value = true;
			return switch_present(str, flags...) || true;
		}
		else {
			return switch_present(str, flags...);
		}
	}

	void set_positional(const std::string &) {}

	template <typename T, typename ... Ts>
	void set_positional(const std::string &str, T &, Ts & ... flags) {
		return set_positional(str, flags...);
	}

	template <typename T, typename ... Ts>
	void set_positional(const std::string &str, Positional<T> &positional, Ts & ... flags) {
		if(!positional.value.has_value()) {
			positional.value = str;
		}
		else {
			set_positional(str, flags...);
		}
	}

	bool parse_flag(const Switch &s) {
		return s.value;
	}

	/// @returns T if the flag is required or optional<T> if it isn't
	template <typename T, bool R>
	typename Flag<T, R>::out parse_flag(const Flag<T, R> &flag) {
		if constexpr(R) {
			if(!flag.value.has_value() && !flag.default_value.has_value()) throw RequiredFlagNotGivenError(flag.name);
			else if(flag.default_value.has_value()) return *flag.default_value;
			else {
				std::optional<T> value = detail::str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(flag.name, *flag.value);
			}
		}
		else {
			if(!flag.value.has_value()) return nullopt;
			else {
				std::optional<T> value = detail::str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(flag.name, *flag.value); // Show invalid input instead of failing silently
			}
		}
	}

	template <typename T>
	T parse_flag(const Positional<T> &p) {
		std::optional<T> value;
		if(p.value.has_value() && (value = detail::str_to_value<T>(*p.value)).has_value()) return *value;

		throw RequiredFlagNotGivenError(p.label);
	}


public:
	Flags(int argc, char **argv) {
		for(int i = 1; i < argc; i++) {
			args.push_back(argv[i]);
		}
	}

	/// Takes a parameter pack of FlagMod::Flag, FlagMod::Switch and FlagMod::Positional
	template <typename ... Ts>
	auto parse(Ts ... flags) {
		try {
			for(size_t idx = 0; idx < args.size(); idx++) {
				const auto &str = args[idx];

				auto [ flag_value, chr_idx ] = flag_present(str, flags...);
				if constexpr(!std::is_same_v<decltype(flag_value), std::nullptr_t>) { // needed to avoid compilation error on nullptr_t assignment
					if(flag_value != nullptr) { 
						if(chr_idx >= str.length() && idx+1 < args.size()) {
							*flag_value = args[idx + 1];
							idx++; // advance index
						}
						else if(str[chr_idx] == '=') {
							*flag_value = str.substr(chr_idx+1);
						}
						continue;
					}
				}

				bool switch_found = switch_present(str, flags...);

				if(flag_value == nullptr && !switch_found) {
					set_positional(str, flags...);
				}
			}

			return std::tuple{ parse_flag(flags)... };
		}
		catch(RequiredFlagNotGivenError &e) {
			fmt::print(stderr, "error: required flag {} wasn't given\n", e.name);
			// TODO: print help
			throw e;
		}
		catch(InvalidFlagInput &e) {
			fmt::print(stderr, "error: couldn't parse input of flag {} ({})\n", e.name, e.input);
		}
		catch(std::exception &e) {
			throw e;
		}
	}

	template <typename T>
	Flag<T, false> flag(const std::string &name, const std::string &help) {
		return Flag<T, false> { name, nullopt, help, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, false> flag(const std::string &name, char short_name, const std::string &help) {
		return Flag<T, false> { name, short_name, help, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, const std::string &help, const std::optional<T> default_value = nullopt) {
		return Flag<T, true> { name, nullopt, help, default_value, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, char short_name, const std::string &help, const std::optional<T> default_value = nullopt) {
		return Flag<T, true> { name, short_name, help, default_value, nullopt };
	}

	Switch add_switch(const std::string &name, const std::string &help) {
		return Switch { name, nullopt, help, false };
	}
	Switch add_switch(const std::string &name, char short_name, const std::string &help) {
		return Switch { name, short_name, help, false };
	}

	template <typename T>
	Positional<T> positional(const std::string &label) {
		return Positional<T>{ label, nullopt };
	}
};


}
