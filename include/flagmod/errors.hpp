#pragma once

#include <exception>
#include <string>

namespace FlagMod {
struct RequiredFlagNotGivenError : std::exception {
	std::string msg;

	RequiredFlagNotGivenError(std::string msg) : msg(msg) {}
};

struct InvalidFlagInput : std::exception {
	std::string msg;

	InvalidFlagInput(std::string msg) : msg(msg) {}
};
}
