// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hand-built bytecode run through the interpreter (B-06), with no game
// data: each test assembles a play method that leaves results in globals.
// The expected values follow ScummVM's SCI0 interpreter
// (engines/sci/engine/vm.cpp, run_vm), which src/sci-vm/src/machine.cpp
// ports.

#include "bytecode_world.hpp"
#include "sci_test.hpp"

#include <cstdio>
#include <memory>
#include <print>

using sci::test::BytecodeWorld;
using sci::test::CodeAssembler;
using sci::test::ScriptBuilder;
using sci::test::Var;
using sci::test::VarOp;
namespace op = sci::test::op;

namespace {

// Runs a game whose play method is the code `body` writes, then returns.
template <typename Body>
std::unique_ptr<BytecodeWorld> run_play(Body body) {
    ScriptBuilder script;
    const CodeAssembler::Label play = script.code().label();
    script.code().bind(play);
    body(script.code());
    script.code().op(op::ret);
    const auto bytes = sci::test::game_script(script, play);
    auto game = sci::test::assembled_game({{0, bytes}}, {0});
    SCI_CHECK(game.has_value());
    if (!game) {
        return nullptr;
    }
    auto world = std::make_unique<BytecodeWorld>(std::move(*game));
    const bool ran = world->run();
    if (!ran) {
        std::println(stderr, "fault: {}", world->machine->faults().message());
    }
    SCI_CHECK(ran);
    return world;
}

} // namespace

SCI_TEST(bytecode_play_stores_a_global) {
    const auto world = run_play([](CodeAssembler& code) { code.op_b(op::ldi, 5).var(VarOp::Store, Var::Global, 0); });
    SCI_CHECK(world != nullptr && world->global(0) == 5);
}
