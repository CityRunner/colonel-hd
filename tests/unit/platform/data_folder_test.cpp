// SPDX-License-Identifier: GPL-3.0-or-later
//
// The data folder's resolution order (B-03), on temporary folders.

#include "sci/platform/config.hpp"
#include "sci/platform/data_folder.hpp"
#include "sci_test.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using sci::platform::DataFolderKind;
using sci::platform::resolve_data_folder;

namespace {

namespace fs = std::filesystem;

// A new empty folder in the temporary directory, removed at the end.
class TempFolder {
public:
    explicit TempFolder(const fs::path& name) : path_(fs::temp_directory_path() / name) {
        std::error_code error;
        fs::remove_all(path_, error);
        fs::create_directories(path_);
    }

    ~TempFolder() {
        std::error_code error;
        fs::permissions(path_, fs::perms::owner_all, fs::perm_options::add, error);
        fs::remove_all(path_, error);
    }

    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;

    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void touch(const fs::path& path) {
    std::ofstream(path, std::ios::binary) << "x";
}

} // namespace

SCI_TEST(data_folder_option_wins) {
    const TempFolder exe("colonel-hd-data-option");
    touch(exe.path() / sci::platform::config_file_name);
    const auto chosen = resolve_data_folder(fs::path("/somewhere/else"), exe.path(), "/per/user");
    SCI_CHECK(chosen.kind == DataFolderKind::Option && chosen.folder == fs::path("/somewhere/else"));
}

SCI_TEST(data_folder_is_portable_with_an_ini_or_a_game_folder) {
    const TempFolder exe("colonel-hd-data-portable");
    // Neither: the per-user folder.
    auto chosen = resolve_data_folder(std::nullopt, exe.path(), "/per/user");
    SCI_CHECK(chosen.kind == DataFolderKind::PerUser && chosen.folder == fs::path("/per/user"));
    // The ini.
    touch(exe.path() / sci::platform::config_file_name);
    chosen = resolve_data_folder(std::nullopt, exe.path(), "/per/user");
    SCI_CHECK(chosen.kind == DataFolderKind::Portable && chosen.folder == exe.path());
    // game/ alone.
    fs::remove(exe.path() / sci::platform::config_file_name);
    fs::create_directories(exe.path() / "game");
    chosen = resolve_data_folder(std::nullopt, exe.path(), "/per/user");
    SCI_CHECK(chosen.kind == DataFolderKind::Portable);
}

SCI_TEST(data_folder_skips_a_read_only_executable_folder) {
    const TempFolder exe("colonel-hd-data-read-only");
    touch(exe.path() / sci::platform::config_file_name);
    fs::permissions(exe.path(), fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
                    fs::perm_options::remove);
    // Root can write anyway; the check then has nothing to show.
    if (!sci::platform::folder_is_writable(exe.path())) {
        const auto chosen = resolve_data_folder(std::nullopt, exe.path(), "/per/user");
        SCI_CHECK(chosen.kind == DataFolderKind::PerUser);
    }
}

SCI_TEST(data_folder_finds_the_game_in_game) {
    // A folder name outside ASCII, as a user name on Windows may be.
    const TempFolder data(fs::path(u8"colonel-hd-data-été"));
    SCI_CHECK(!sci::platform::data_game_folder(data.path()).has_value());
    fs::create_directories(data.path() / "game");
    touch(data.path() / "game" / "README.txt");
    SCI_CHECK(!sci::platform::data_game_folder(data.path()).has_value());
    touch(data.path() / "game" / "RESOURCE.MAP");
    const auto game = sci::platform::data_game_folder(data.path());
    SCI_CHECK(game.has_value() && *game == data.path() / "game");
    SCI_CHECK(sci::platform::folder_is_writable(data.path()));
}
