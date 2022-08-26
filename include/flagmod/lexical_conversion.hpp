#pragma once

#include <string>
#include <charconv>
#include <vector>
#include <optional>
#include <stdexcept>

#include <fmt/core.h>

#include "errors.hpp"

namespace FlagMod {
	namespace detail {

	/// NOTE: Parameter T* will always be null, it is used for disambiguation when template specialization shits its pants (eg: with std::vector<T>)
	// template <typename T>
	// T lexical_conversion(std::string_view input, T *);

	inline std::string lexical_conversion(std::string_view input, std::string *) { return std::string(input);}
	inline char lexical_conversion(std::string_view input, char *) { 
		if(input.length() == 0) throw InvalidArgument("");
		return input[0]; 
	}

	template <typename T>
	inline std::vector<T> lexical_conversion(std::string_view input, std::vector<T> *) {
		std::vector<T> v;

		size_t last_pos = 0;
		for(size_t i = 0; i <= input.length(); i++) {
			if(i == input.length() || (input[i] == ',' && last_pos < input.length())) { // OR short circuit to avoid out of bounds access
				auto substr = input.substr(last_pos, i-last_pos);
				try {
					auto value = lexical_conversion<T>(substr, static_cast<T *>(nullptr));
					v.push_back(std::move(value));
				}
				catch(InvalidArgument &e) {
					throw InvalidArgument(fmt::format("in list parsing of \"{}\": \n{}", substr, e.msg));
				}
				last_pos = i+1;
			}
		}

		return v;
	}

#ifndef DEFINE_NUMERIC_STR_TO_VALUE
	#define DEFINE_NUMERIC_STR_TO_VALUE(type) \
		inline type lexical_conversion(std::string_view input, type *) { \
			type out; \
			auto [__, ec] = std::from_chars(input.data(), input.data() + input.size(), out); \
			if(ec == std::errc::invalid_argument) throw InvalidArgument("invalid input for numeric type " #type ": not a number"); \
			else if(ec == std::errc::result_out_of_range) throw InvalidArgument("input too big for numeric type " #type); \
			else return out; \
		}
#endif

	DEFINE_NUMERIC_STR_TO_VALUE(int)
	DEFINE_NUMERIC_STR_TO_VALUE(unsigned int)
	DEFINE_NUMERIC_STR_TO_VALUE(long)
	DEFINE_NUMERIC_STR_TO_VALUE(unsigned long)
	DEFINE_NUMERIC_STR_TO_VALUE(long long)
	DEFINE_NUMERIC_STR_TO_VALUE(unsigned long long)
	DEFINE_NUMERIC_STR_TO_VALUE(float)
	DEFINE_NUMERIC_STR_TO_VALUE(double)
	DEFINE_NUMERIC_STR_TO_VALUE(long double)

	#undef DEFINE_NUMERIC_STR_TO_VALUE
	}

template <typename T>
T lexical_conversion(std::string_view input) {
	return detail::lexical_conversion(input, static_cast<T *>(nullptr));
}

template <typename T>
concept lexically_convertible = requires {
	detail::lexical_conversion(std::string_view{}, static_cast<T*>(nullptr));
};

}
