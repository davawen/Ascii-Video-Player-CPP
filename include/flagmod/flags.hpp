#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <ranges>

#include "fmt/core.h"

#include "str_parse.hpp"
#include "errors.hpp"
#include "help.hpp"

namespace FlagMod {
	constexpr auto nullopt = std::nullopt;

template <typename T, bool Required>
struct Flag {
	using type = T;
	using out = std::conditional_t<Required, T, std::optional<T>>;

	std::string name;
	std::optional<char> short_name;

	std::optional<T> default_value;
	std::optional<std::string> value;
};

struct Switch {
	std::string name;
	std::optional<char> short_name;

	bool value;
};

template <typename T>
struct Positional {
	std::string label;
	
	std::optional<std::string> value;
};

class Flags {
	std::vector<std::string> args;
	Help help;

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
			if(!flag.value.has_value() && !flag.default_value.has_value()) throw RequiredFlagNotGivenError(fmt::format("option --{} requires a value but wasn't given one\n{}\n", flag.name, help.format_flag_help(flag.name)));
			else if(flag.default_value.has_value()) return *flag.default_value;
			else {
				std::optional<T> value = str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(fmt::format("couldn't parse input of --{}, was given {}\n{}\n", flag.name, *flag.value, help.format_flag_help(flag.name))); // Show invalid input instead of failing silently
			}
		}
		else {
			if(!flag.value.has_value()) return nullopt;
			else {
				std::optional<T> value = str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(fmt::format("couldn't parse input of --{}, was given {}\n{}\n", flag.name, *flag.value, help.format_flag_help(flag.name))); 
			}
		}
	}

	template <typename T>
	T parse_flag(const Positional<T> &p) {
		std::optional<T> value;
		if(p.value.has_value() && (value = str_to_value<T>(*p.value)).has_value()) return *value;

		throw RequiredFlagNotGivenError(fmt::format("argument {{{}}} not given", p.label));
	}

public:
	Flags(int argc, char **argv) {
		for(int i = 1; i < argc; i++) {
			args.push_back(argv[i]);
		}

		help.executable = argv[0];
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
			help.print_usage(stderr);
			fmt::print(stderr, "\x1b[1;31m\x1b[4merror:\x1b[0m {}\n", e.msg);
			// TODO: print help
			throw e;
		}
		catch(InvalidFlagInput &e) {
			help.print_usage(stderr);
			fmt::print(stderr, "\x1b[1;31m\x1b[4merror:\x1b[0m {}\n", e.msg);
		}
		catch(std::exception &e) {
			throw e;
		}
	}

	Flags &name(const std::string &name) {
		help.name = name;
		return *this;
	}

	Flags &version(const std::string &version) {
		help.version = version;
		return *this;
	}

	void print_help() {
		help.print_help();
	}

	template <typename T>
	Flag<T, false> flag(const std::string &name, const std::string &help) {
		this->help.flag_help.push_back(Help::FlagHelp { name, nullopt, help, nullopt });
		return Flag<T, false> { name, nullopt, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, false> flag(const std::string &name, char short_name, const std::string &help) {
		this->help.flag_help.push_back(Help::FlagHelp { name, short_name, help, nullopt });
		return Flag<T, false> { name, short_name, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, const std::string &help, const std::optional<T> default_value = nullopt) {
		this->help.flag_help.push_back(Help::FlagHelp { name, nullopt, help, default_value.has_value() ? std::optional{std::to_string(*default_value)} : nullopt });
		return Flag<T, true> { name, nullopt, default_value, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, char short_name, const std::string &help, const std::optional<T> default_value = nullopt) {
		this->help.flag_help.push_back(Help::FlagHelp { name, short_name, help, default_value.has_value() ? std::optional{std::to_string(*default_value)} : nullopt });
		return Flag<T, true> { name, short_name, default_value, nullopt };
	}

	Switch add_switch(const std::string &name, const std::string &help) {
		this->help.switch_help.push_back(Help::SwitchHelp { name, nullopt, help });
		return Switch { name, nullopt, false };
	}
	Switch add_switch(const std::string &name, char short_name, const std::string &help) {
		this->help.switch_help.push_back(Help::SwitchHelp { name, short_name, help });
		return Switch { name, short_name, false };
	}

	template <typename T>
	Positional<T> positional(const std::string &label) {
		this->help.positional_help.push_back(Help::PositionalHelp{ label });
		return Positional<T>{ label, nullopt };
	}
};

}
