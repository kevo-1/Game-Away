#pragma once

#include <string>

namespace GameAway {

// Generate a random alphanumeric token of specified length
std::string generateToken(size_t length = 6);

// Get the computer's hostname
std::string getPcName();

} // namespace GameAway
