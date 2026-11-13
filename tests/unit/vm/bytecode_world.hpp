// SPDX-License-Identifier: GPL-3.0-or-later
//
// A machine over a game made by the script assembler (B-06): vocab 997 with
// the selector names below, vocab 996 with the classes, and the scripts
// the test builds. Script 0 holds the game object; its play method is the
// code under test, which leaves its results in globals.

#pragma once

#include "builders.hpp"
#include "machine.hpp"
#include "sci/resource/resource_manager.hpp"
#include "script_assembler.hpp"
#include "subsystems.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sci::test {

// The selectors of the test games: the four base properties, play, and
// selectors the tests use for properties and methods.
namespace sel {
inline constexpr int species = 0, super_class = 1, info = 2, name = 3, play = 4, value = 5, other = 6, twice = 7,
                     sum = 8, base = 9, count = 10;
} // namespace sel

// The test selectors above, then the names the interpreter looks up
// (src/sci-vm/src/selectors.hpp), which kernel functions read and write.
inline std::vector<std::string> selector_names() {
    return {
        "species",   "superClass",  "-info-",    "name",      "play",     "value",      "other",    "twice",
        "sum",       "base",        "y",         "x",         "view",     "loop",       "cel",      "underBits",
        "nsTop",     "nsLeft",      "nsBottom",  "nsRight",   "lsTop",    "lsLeft",     "lsBottom", "lsRight",
        "signal",    "illegalBits", "brTop",     "brLeft",    "brBottom", "brRight",    "text",     "elements",
        "mode",      "state",       "font",      "type",      "cursor",   "max",        "mark",     "sort",
        "message",   "restore",     "number",    "handle",    "nodePtr",  "client",     "dx",       "dy",
        "b-moveCnt", "b-i1",        "b-i2",      "b-di",      "b-xAxis",  "b-incr",     "xStep",    "yStep",
        "xLast",     "yLast",       "moveSpeed", "canBeHere", "heading",  "mover",      "doit",     "isBlocked",
        "looper",    "priority",    "modifiers", "replay",    "wordFail", "syntaxFail", "claimed",  "delete",
        "z",         "setPri",      "size",      "min",       "sec",      "frame",      "vol",      "pri",
        "moveDone",
    };
}

// The number of a selector of selector_names().
inline int selector_id(std::string_view name) {
    const std::vector<std::string> names = selector_names();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Another resource of a test game.
struct ExtraResource {
    resource::ResourceType type;
    int number;
    Bytes data;
};

inline std::optional<resource::ResourceManager> assembled_game(const std::map<int, Bytes>& scripts,
                                                               const std::vector<int>& class_scripts,
                                                               const std::vector<ExtraResource>& extra = {}) {
    Bytes volume;
    Bytes map;
    const auto add = [&](resource::ResourceType type, int number, const Bytes& data) {
        const auto offset = static_cast<std::uint32_t>(volume.size());
        append_stored_entry(volume, type, static_cast<std::uint16_t>(number), data);
        append_map_entry(map, type, static_cast<std::uint16_t>(number), 1, offset);
    };
    add(resource::ResourceType::Vocab, 996, class_vocab(class_scripts));
    add(resource::ResourceType::Vocab, 997, selector_vocab(selector_names()));
    add(resource::ResourceType::Font, 0, blank_font());
    for (const auto& [number, data] : scripts) {
        add(resource::ResourceType::Script, number, data);
    }
    for (const ExtraResource& resource : extra) {
        add(resource.type, resource.number, resource.data);
    }
    append_map_terminator(map);
    std::vector<resource::GameFile> files;
    files.push_back(resource::GameFile{.name = "resource.map", .bytes = std::move(map)});
    files.push_back(resource::GameFile{.name = "RESOURCE.001", .bytes = std::move(volume)});
    auto manager = resource::ResourceManager::open(std::move(files));
    if (!manager) {
        return std::nullopt;
    }
    return std::move(*manager);
}

// The usual script 0: class 0 (the game's class) and the game object, an
// instance of it whose play method is at `play` in `script.code()`, with
// `globals` global variables. The caller adds code and other objects first.
inline Bytes game_script(ScriptBuilder& script, CodeAssembler::Label play, int globals = 32,
                         std::vector<std::pair<int, CodeAssembler::Label>> class_methods = {}) {
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = std::move(class_methods),
    });
    const std::size_t game = script.add(ObjectSpec{
        .is_class = false,
        .species = 0,
        .super_class = 0,
        .extra_properties = {},
        .methods = {{sel::play, play}},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(static_cast<std::size_t>(globals), 0));
    return script.build();
}

// A machine that has run the game's play method to its end.
struct BytecodeWorld {
    resource::ResourceManager resources;
    vm::Subsystems subsystems;
    std::unique_ptr<vm::Machine> machine;

    explicit BytecodeWorld(resource::ResourceManager game) : resources(std::move(game)) {
        machine = std::make_unique<vm::Machine>(resources, subsystems);
        subsystems.attach(*machine);
    }

    // Loads the game and runs play; false if a fault stopped it.
    [[nodiscard]] bool run() const {
        if (!machine->init_game()) {
            return false;
        }
        machine->run_game();
        return !machine->faults().raised();
    }

    [[nodiscard]] std::int16_t global(std::size_t index) const { return machine->global(index).to_sint16(); }
};

} // namespace sci::test
