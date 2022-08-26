#pragma once

#include <exception>
#include <string>

namespace FlagMod {
struct RequiredFlagNotGiven : std::exception {
	std::string msg;

	RequiredFlagNotGiven(std::string msg) : msg(msg) {}
};

struct InvalidArgument : std::exception {
	std::string msg;

	InvalidArgument(std::string msg) : msg(msg) {}
};
}
