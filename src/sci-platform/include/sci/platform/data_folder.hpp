// SPDX-License-Identifier: GPL-3.0-or-later
//
// The program's data folder (B-03, docs/ASSUMPTIONS.md "One data folder"):
// it holds colonel-hd.ini, game/, rom/, saves/, cache/ and files/. The
// hints stay next to the executable.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace sci::platform {

enum class DataFolderKind : std::int8_t {
    Option,   // Named with --data.
    Portable, // The executable's folder: it holds colonel-hd.ini or game/ and is writable.
    PerUser,  // %APPDATA%\colonel-hd, or $XDG_DATA_HOME/colonel-hd (~/.local/share/colonel-hd).
};

struct DataFolder {
    std::filesystem::path folder;
    DataFolderKind kind = DataFolderKind::PerUser;
};

// Chooses the data folder: `option` if given; else the executable's folder
// if it holds colonel-hd.ini or game/ and a file can be created in it; else
// `per_user`. Creates nothing.
[[nodiscard]] DataFolder resolve_data_folder(const std::optional<std::filesystem::path>& option,
                                             const std::filesystem::path& executable_folder,
                                             const std::filesystem::path& per_user);

// The per-user folder of this system, from the environment: APPDATA on
// Windows; XDG_DATA_HOME, else HOME/.local/share, on other systems. Empty
// if none of them is set.
[[nodiscard]] std::filesystem::path per_user_data_folder();

// Whether a file can be created in the folder (it is created and removed).
[[nodiscard]] bool folder_is_writable(const std::filesystem::path& folder);

// The game folder of the data folder (game/) if it holds a resource map,
// whatever the case of its name; else nothing.
[[nodiscard]] std::optional<std::filesystem::path> data_game_folder(const std::filesystem::path& data_folder);

} // namespace sci::platform
