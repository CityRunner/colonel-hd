# SPDX-License-Identifier: GPL-3.0-or-later
if(NOT SCI_FUZZ)
    add_library(sci_fuzz_replay_main STATIC replay_main.cpp)
    sci_target_defaults(sci_fuzz_replay_main)
endif()

sci_add_fuzz_target(NAME fuzz-byte-reader
    SOURCE fuzz_byte_reader.cpp
    DEPENDS sci-core
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/byte_reader)

sci_add_fuzz_target(NAME fuzz-decompress
    SOURCE fuzz_decompress.cpp
    DEPENDS sci-resource
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/decompress)

sci_add_fuzz_target(NAME fuzz-game-folder
    SOURCE fuzz_game_folder.cpp
    DEPENDS sci-resource
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/game_folder)

sci_add_fuzz_target(NAME fuzz-pic
    SOURCE fuzz_pic.cpp
    DEPENDS sci-pic
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/pic)

sci_add_fuzz_target(NAME fuzz-view-font
    SOURCE fuzz_view.cpp
    DEPENDS sci-text sci-view
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/view_font)

sci_add_fuzz_target(NAME fuzz-vector
    SOURCE fuzz_vector.cpp
    DEPENDS sci-pic sci-vector sci-render
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/pic)

sci_add_fuzz_target(NAME fuzz-json
    SOURCE fuzz_json.cpp
    DEPENDS sci-core
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/json)

sci_add_fuzz_target(NAME fuzz-hints
    SOURCE fuzz_hints.cpp
    DEPENDS sci-vector
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/hints)

sci_add_fuzz_target(NAME fuzz-replay
    SOURCE fuzz_replay.cpp
    DEPENDS sci-vm
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/replay)

sci_add_fuzz_target(NAME fuzz-scene
    SOURCE fuzz_scene.cpp
    DEPENDS sci-vector
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/scene)

sci_add_fuzz_target(NAME fuzz-parser
    SOURCE fuzz_parser.cpp
    DEPENDS sci-text
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/parser)

# The music driver's patch resources, AdLib bank and MT-32 patch (stage 5
# step 0.5, review/BACKLOG_V2.md B-05).
sci_add_fuzz_target(NAME fuzz-patch
    SOURCE fuzz_patch.cpp
    DEPENDS sci-audio
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/patch)

# The configuration file (stage 5 step 0.5, B-05).
sci_add_fuzz_target(NAME fuzz-config
    SOURCE fuzz_config.cpp
    DEPENDS sci-platform
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/config)

# Sound resources through the music player, MIDI parser and AdLib driver
# (stage 5 step 0.5, B-05).
sci_add_fuzz_target(NAME fuzz-sound
    SOURCE fuzz_sound.cpp
    DEPENDS sci-vm sci-audio
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/sound)

# Script resources through sci-vm's script loader, on a synthetic game
# folder (stage 5 step 0.5, B-05); private sci-vm headers as for fuzz-save.
sci_add_fuzz_target(NAME fuzz-script
    SOURCE fuzz_script.cpp
    DEPENDS sci-vm
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/script)
sci_test_private_includes(fuzz-script sci-vm)
target_include_directories(fuzz-script PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/unit/vm ${PROJECT_SOURCE_DIR}/tests/unit/resource)

# The save loader on a synthetic machine (tests/unit/vm/save_world.hpp); the
# seeds are saves of that machine, written by sci-vm-tests with
# SCI_WRITE_SAVE_SEEDS=tests/fuzz/corpus/save.
sci_add_fuzz_target(NAME fuzz-save
    SOURCE fuzz_save.cpp
    DEPENDS sci-vm
    CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/save)
sci_test_private_includes(fuzz-save sci-vm)
target_include_directories(fuzz-save PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/unit/vm ${PROJECT_SOURCE_DIR}/tests/unit/resource)

# The list of fuzz targets and their seed corpora, for scripts/run-fuzzers.sh.
# Keep it last: a target added after it is not fuzzed (B-44;
# scripts/check-fuzz-list.py checks this).
if(SCI_FUZZ)
    get_property(targets GLOBAL PROPERTY SCI_FUZZ_TARGETS)
    list(JOIN targets "\n" lines)
    file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/fuzz-targets.txt CONTENT "${lines}\n")
endif()
