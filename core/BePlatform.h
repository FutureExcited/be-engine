#pragma once

#include <filesystem>

namespace BePlatform {
    auto GetExecutableDirectory() -> std::filesystem::path;
    auto MoveWorkingDirectoryToExecutableDir() -> void;
}
