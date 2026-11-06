// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loads arbitrary bytes as script 0 of a synthetic game folder into a
// machine (sci-vm's script loader: exports, locals, objects, classes,
// relocation and the script patches), as the interpreter loads a script.
// The loader must load it or report it malformed, never fail otherwise.
// The game folder is built as the save tests build theirs
// (tests/unit/vm/save_world.hpp); no game data.

#include "builders.hpp"
#include "machine.hpp"
#include "sci/resource/resource_manager.hpp"
#include "seg_manager.hpp"
#include "subsystems.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming): name fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > 0xFF00) { // A stored resource holds at most 64 KiB.
        return 0;
    }
    const std::span<const std::uint8_t> input(data, size);
    const sci::test::Bytes script(input.begin(), input.end());
    sci::test::Bytes volume;
    sci::test::append_stored_entry(volume, sci::resource::ResourceType::Script, 0, script);
    sci::test::Bytes map;
    sci::test::append_map_entry(map, sci::resource::ResourceType::Script, 0, 1, 0);
    sci::test::append_map_terminator(map);
    std::vector<sci::resource::GameFile> files;
    files.push_back(sci::resource::GameFile{.name = "resource.map", .bytes = std::move(map)});
    files.push_back(sci::resource::GameFile{.name = "RESOURCE.001", .bytes = std::move(volume)});
    auto resources = sci::resource::ResourceManager::open(std::move(files));
    if (!resources) {
        return 0;
    }
    sci::vm::Subsystems subsystems;
    const auto machine = std::make_unique<sci::vm::Machine>(*resources, subsystems);
    subsystems.attach(*machine);
    [[maybe_unused]] const auto segment = machine->segments().script_segment(0, sci::vm::ScriptLoad::Lock);
    return 0;
}
