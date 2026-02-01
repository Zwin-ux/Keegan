#pragma once
#include <string>

namespace util {

// Attempts to set the current working directory to the project root
// so that assets and config files can be loaded using relative paths.
// Returns true if successful or if config found in current dir.
bool fixWorkingDirectory();

} // namespace util
