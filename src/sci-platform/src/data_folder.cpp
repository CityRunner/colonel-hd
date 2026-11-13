// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/data_folder.hpp"

#include "sci/platform/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

namespace sci::platform {

namespace fs = std::filesystem;

bool folder_is_writable(const fs::path& folder) {
    std::error_code error;
    if (!fs::is_directory(folder, error)) {
        return false;
    }
    const fs::path probe = folder / ".colonel-hd-write-test";
    {
        std::ofstream file(probe, std::ios::binary);
        if (!file) {
            return false;
        }
    }
    fs::remove(probe, error);
    return true;
}

DataFolder resolve_data_folder(const std::optional<fs::path>& option, const fs::path& executable_folder,
                               const fs::path& per_user) {
    if (option) {
        return DataFolder{.folder = *option, .kind = DataFolderKind::Option};
    }
    std::error_code error;
    const bool has_files = fs::is_regular_file(executable_folder / config_file_name, error) ||
                           fs::is_directory(executable_folder / "game", error);
    if (has_files && folder_is_writable(executable_folder)) {
        return DataFolder{.folder = executable_folder, .kind = DataFolderKind::Portable};
    }
    return DataFolder{.folder = per_user, .kind = DataFolderKind::PerUser};
}

fs::path per_user_data_folder() {
#ifdef _WIN32
    // The wide environment, so that a user name outside the ANSI code page
    // survives.
    if (const wchar_t* appdata = _wgetenv(L"APPDATA"); appdata != nullptr && *appdata != L'\0') {
        return fs::path(appdata) / "colonel-hd";
    }
    return {};
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at start-up, before any thread.
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && *xdg != '\0') {
        return fs::path(xdg) / "colonel-hd";
    }
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at start-up, before any thread.
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return fs::path(home) / ".local" / "share" / "colonel-hd";
    }
    return {};
#endif
}

std::optional<fs::path> data_game_folder(const fs::path& data_folder) {
    const fs::path game = data_folder / "game";
    std::error_code error;
    for (fs::directory_iterator it(game, error), end; !error && it != end; it.increment(error)) {
        std::string name = it->path().filename().string();
        std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == "resource.map" && it->is_regular_file(error)) {
            return game;
        }
    }
    return std::nullopt;
}

} // namespace sci::platform
