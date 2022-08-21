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
	std::string msg;

	RequiredFlagNotGivenError(std::string msg) : msg(msg) {}
};

struct InvalidFlagInput : std::exception {
	std::string msg;

	InvalidFlagInput(std::string msg) : msg(msg) {}
};
	
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

	struct FlagHelp {
		std::string name;
		std::optional<char> short_name;
		std::string help;
	};

	struct SwitchHelp {
		std::string name;
		std::optional<char> short_name;
		std::string help;
	};

	struct PositionalHelp {
		std::string label;
	};

	std::string name_, version_, executable_;
	std::vector<FlagHelp> flag_help;
	std::vector<SwitchHelp> switch_help;
	std::vector<PositionalHelp> positional_help;

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

	std::string format_flag_help(const std::string &name) {
		namespace ranges = std::ranges;

		auto matching = [&name](auto &x){ return x.name == name; };
		if(auto it = ranges::find_if(flag_help, matching); it != flag_help.end()) {
			FlagHelp &f = *it;
			return fmt::format("    {} --{}    {}\n", f.short_name.has_value() ? fmt::format("-{},", *f.short_name) : "   ", f.name, f.help);
		}
		else if(auto it = ranges::find_if(switch_help, matching); it != switch_help.end()) {
			SwitchHelp &s = *it;
			return fmt::format("    {} --{}    {}\n", s.short_name.has_value() ? fmt::format("-{},", *s.short_name) : "   ", s.name, s.help);
		}
		return "";
	}

	/// @returns T if the flag is required or optional<T> if it isn't
	template <typename T, bool R>
	typename Flag<T, R>::out parse_flag(const Flag<T, R> &flag) {
		if constexpr(R) {
			if(!flag.value.has_value() && !flag.default_value.has_value()) throw RequiredFlagNotGivenError(fmt::format("option --{} requires a value but wasn't given one\n{}\n", flag.name, format_flag_help(flag.name)));
			else if(flag.default_value.has_value()) return *flag.default_value;
			else {
				std::optional<T> value = detail::str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(fmt::format("couldn't parse input of --{}, was given {}\n{}\n", flag.name, *flag.value, format_flag_help(flag.name))); // Show invalid input instead of failing silently
			}
		}
		else {
			if(!flag.value.has_value()) return nullopt;
			else {
				std::optional<T> value = detail::str_to_value<T>(*flag.value);
				if(value.has_value()) return *value;
				else throw InvalidFlagInput(fmt::format("couldn't parse input of --{}, was given {}\n{}\n", flag.name, *flag.value, format_flag_help(flag.name))); 
			}
		}
	}

	template <typename T>
	T parse_flag(const Positional<T> &p) {
		std::optional<T> value;
		if(p.value.has_value() && (value = detail::str_to_value<T>(*p.value)).has_value()) return *value;

		throw RequiredFlagNotGivenError(fmt::format("argument {{{}}} not given", p.label));
	}

	void print_usage(FILE *output) {
		fmt::print(output, "Usage: {} [options...]{}\n\n", executable_, std::accumulate(positional_help.begin(), positional_help.end(), std::string(""), 
			[](std::string a, PositionalHelp &b) {
				return std::move(a) + fmt::format(" {{{}}}", b.label);
			}
		));
	}

public:
	Flags(int argc, char **argv) {
		for(int i = 1; i < argc; i++) {
			args.push_back(argv[i]);
		}

		executable_ = argv[0];
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
			print_usage(stderr);
			fmt::print(stderr, "\x1b[1;31m\x1b[4merror:\x1b[0m {}\n", e.msg);
			// TODO: print help
			throw e;
		}
		catch(InvalidFlagInput &e) {
			print_usage(stderr);
			fmt::print(stderr, "\x1b[1;31m\x1b[4merror:\x1b[0m {}\n", e.msg);
		}
		catch(std::exception &e) {
			throw e;
		}
	}

	void print_help(FILE *output = stdout) {
		namespace ranges = std::ranges;
		
		fmt::print(output, "{} version {}\n\n", name_, version_);
		
		print_usage(output);

		if(switch_help.size() > 0) {
			size_t max_length = 0;
			std::vector<std::pair<std::string, const SwitchHelp *>> switches;
			ranges::transform(switch_help, std::back_inserter(switches),
				[&max_length](const SwitchHelp &s) {
					auto str = fmt::format("    {} --{}", s.short_name.has_value() ? fmt::format("-{},", *s.short_name) : "   ", s.name);
					max_length = std::max(max_length, str.length());
					return std::pair{ str, &s };
				}
			);

			fmt::print(output, "FLAGS:{}\n", std::accumulate(switches.cbegin(), switches.cend(), std::string("\n"), 
				[max_length](std::string a, const std::pair<const std::string, const SwitchHelp *> &b) {
					return std::move(a) + std::move(b.first) + fmt::format("{}    {}\n", std::string(max_length - b.first.length(), ' '), b.second->help);
				}
			));
		}

		if(flag_help.size() > 0){
			size_t max_length = 0;
			std::vector<std::pair<std::string, const FlagHelp *>> flags;
			ranges::transform(flag_help, std::back_inserter(flags), 
				[&max_length](const FlagHelp &f) {
					auto str = fmt::format("    {} --{}", f.short_name.has_value() ? fmt::format("-{},", *f.short_name) : "    ", f.name);
					max_length = std::max(max_length, str.length());
					return std::pair{ str, &f };
				}
			);

			fmt::print(output, "OPTIONS:{}\n", std::accumulate(flags.cbegin(), flags.cend(), std::string("\n"), 
				[max_length](std::string a, const std::pair<const std::string, const FlagHelp *> &b) {
					return std::move(a) + std::move(b.first) + fmt::format("{}    {}\n", std::string(max_length - b.first.length(), ' '), b.second->help);
				}
			));
		}
	}

	Flags &name(const std::string &name) {
		name_ = name;
		return *this;
	}

	Flags &version(const std::string &version) {
		version_ = version;
		return *this;
	}

	template <typename T>
	Flag<T, false> flag(const std::string &name, const std::string &help) {
		flag_help.push_back(FlagHelp { name, nullopt, help });
		return Flag<T, false> { name, nullopt, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, false> flag(const std::string &name, char short_name, const std::string &help) {
		flag_help.push_back(FlagHelp { name, short_name, help });
		return Flag<T, false> { name, short_name, nullopt, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, const std::string &help, const std::optional<T> default_value = nullopt) {
		flag_help.push_back(FlagHelp { name, nullopt, help });
		return Flag<T, true> { name, nullopt, default_value, nullopt };
	}
	template <typename T>
	Flag<T, true> flag_required(const std::string &name, char short_name, const std::string &help, const std::optional<T> default_value = nullopt) {
		flag_help.push_back(FlagHelp { name, short_name, help });
		return Flag<T, true> { name, short_name, default_value, nullopt };
	}

	Switch add_switch(const std::string &name, const std::string &help) {
		switch_help.push_back(SwitchHelp { name, nullopt, help });
		return Switch { name, nullopt, false };
	}
	Switch add_switch(const std::string &name, char short_name, const std::string &help) {
		switch_help.push_back(SwitchHelp { name, short_name, help });
		return Switch { name, short_name, false };
	}

	template <typename T>
	Positional<T> positional(const std::string &label) {
		positional_help.push_back(PositionalHelp{ label });
		return Positional<T>{ label, nullopt };
	}
};

}
