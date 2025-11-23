#pragma once

#include <string>
#include "../brain/state_machine.h"

namespace config {

class MoodLoader {
public:
    static brain::MoodPack loadFromFile(const std::string &path, bool &ok);
};

} // namespace config
