// SPDX-License-Identifier: GPL-3.0-or-later
//
// The files the game opens (kFOpen) in the program (B-32): written into the
// files folder, read from there first and else from the game folder, with
// plain names only.

#include "sci/platform/file_store.hpp"
#include "sci/resource/resource_manager.hpp"
#include "sci_test.hpp"

#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

// A folder in the temporary directory that does not exist yet, removed at
// the end.
class ScratchFolder {
public:
    explicit ScratchFolder(const fs::path& name) : path_(fs::temp_directory_path() / name) {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    ~ScratchFolder() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    ScratchFolder(const ScratchFolder&) = delete;
    ScratchFolder& operator=(const ScratchFolder&) = delete;
    ScratchFolder(ScratchFolder&&) = delete;
    ScratchFolder& operator=(ScratchFolder&&) = delete;

    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

} // namespace

SCI_TEST(game_files_are_read_by_any_case_and_written_files_win) {
    const ScratchFolder folder("colonel-hd-file-store-test");
    std::vector<sci::resource::GameFile> game;
    game.push_back(sci::resource::GameFile{.name = "PARTY.TXT", .bytes = {1, 2, 3}});
    sci::platform::DirectoryFileStore store(folder.path() / "files", game);

    SCI_CHECK(store.read("party.txt") == std::vector<std::uint8_t>({1, 2, 3}));
    SCI_CHECK(!store.read("missing.txt").has_value());

    // Writing creates the folder; the written file is read before the game's.
    const std::vector<std::uint8_t> written{9, 8};
    SCI_CHECK(store.write("party.txt", written));
    SCI_CHECK(fs::exists(folder.path() / "files" / "party.txt"));
    SCI_CHECK(store.read("party.txt") == written);
}

SCI_TEST(names_cannot_reach_other_folders) {
    const ScratchFolder folder("colonel-hd-file-store-names");
    sci::platform::DirectoryFileStore store(folder.path() / "files", {});
    const std::vector<std::uint8_t> data{7};
    SCI_CHECK(store.write("../escape.txt", data));
    SCI_CHECK(store.write("c:\\dir\\name.txt", data));
    SCI_CHECK(!fs::exists(folder.path() / "escape.txt"));
    SCI_CHECK(fs::exists(folder.path() / "files" / ".._escape.txt"));
    SCI_CHECK(fs::exists(folder.path() / "files" / "c__dir_name.txt"));
    SCI_CHECK(store.read("../escape.txt") == data);
}
