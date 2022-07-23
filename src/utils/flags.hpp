#pragma once

#include <string>

namespace utils::flags
{
	bool has_flag(const std::string& flag);
	std::optional<std::string> get_flag(const std::string& flag);
}
