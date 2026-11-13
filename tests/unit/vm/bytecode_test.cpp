// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hand-built bytecode run through the interpreter (B-06), with no game
// data: each test assembles a play method that leaves results in globals.
// The expected values follow ScummVM's SCI0 interpreter
// (engines/sci/engine/vm.cpp, run_vm), which src/sci-vm/src/machine.cpp
// ports.

#include "bytecode_world.hpp"
#include "sci_test.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <vector>

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

namespace {

// A value into the accumulator (ldi, word form: any int16).
void load(CodeAssembler& code, int value) {
    code.op_w(op::ldi, value);
}

struct BinaryCase {
    int left;
    int opcode;
    int right;
    int expected; // As int16.
};

// Each case: left pushed, right in the accumulator, the opcode, the result
// into global i.
std::unique_ptr<BytecodeWorld> run_binary(const std::vector<BinaryCase>& cases) {
    return run_play([&](CodeAssembler& code) {
        for (std::size_t i = 0; i < cases.size(); ++i) {
            load(code, cases[i].left);
            code.op(op::push);
            load(code, cases[i].right);
            code.op(cases[i].opcode).var(VarOp::Store, Var::Global, static_cast<int>(i));
        }
    });
}

void check_cases(const std::unique_ptr<BytecodeWorld>& world, const std::vector<BinaryCase>& cases) {
    SCI_CHECK(world != nullptr);
    if (world == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < cases.size(); ++i) {
        if (world->global(i) != static_cast<std::int16_t>(cases[i].expected)) {
            std::println(stderr, "case {}: opcode {:02x}, {} and {}: {} instead of {}", i, cases[i].opcode,
                         cases[i].left, cases[i].right, world->global(i), cases[i].expected);
        }
        SCI_CHECK(world->global(i) == static_cast<std::int16_t>(cases[i].expected));
    }
}

} // namespace

// vm.cpp op_add ... op_or: signed arithmetic on int16, C division
// (truncating), SCI0's modulo (the result made non-negative, with the
// divisor's absolute value), unsigned shifts and bitwise operations.
SCI_TEST(bytecode_arithmetic_and_bitwise) {
    const std::vector<BinaryCase> cases{
        {7, op::add, 5, 12},
        {7, op::sub, 5, 2},
        {7, op::mul, -3, -21},
        {-7, op::div, 2, -3},
        {7, op::div, 2, 3},
        {-7, op::mod, 3, 2},
        {7, op::mod, -3, 1},
        {7, op::mod, 3, 1},
        {-32768, op::shr, 3, 4096},
        {3, op::shl, 4, 48},
        {0x0f0f, op::bxor, 0x00ff, 0x0ff0},
        {0x0ff0, op::band, 0x00ff, 0x00f0},
        {0x0ff0, op::bor, 0x00ff, 0x0fff},
        {32767, op::add, 1, -32768}, // Wraps as int16.
    };
    check_cases(run_binary(cases), cases);
}

// vm.cpp op_eq_ ... op_ule_: signed and unsigned comparisons give 1 or 0.
SCI_TEST(bytecode_comparisons) {
    const std::vector<BinaryCase> cases{
        {3, op::eq, 3, 1},  {3, op::eq, 4, 0},   {3, op::ne, 4, 1},   {3, op::ne, 3, 0},    {-1, op::gt, 1, 0},
        {2, op::gt, 1, 1},  {1, op::ge, 1, 1},   {0, op::ge, 1, 0},   {-1, op::lt, 1, 1},   {1, op::lt, 1, 0},
        {1, op::le, 1, 1},  {2, op::le, 1, 0},   {-1, op::ugt, 1, 1}, {1, op::ugt, -1, 0},  {-1, op::uge, -1, 1},
        {1, op::uge, 2, 0}, {1, op::ult, -1, 1}, {-1, op::ult, 1, 0}, {-1, op::ule, -1, 1}, {-1, op::ule, 1, 0},
    };
    check_cases(run_binary(cases), cases);
}

// vm.cpp op_bnot, op_neg, op_not: on the accumulator alone.
SCI_TEST(bytecode_unary) {
    const auto world = run_play([](CodeAssembler& code) {
        load(code, 0x00ff);
        code.op(op::bnot).var(VarOp::Store, Var::Global, 0);
        load(code, 5);
        code.op(op::neg).var(VarOp::Store, Var::Global, 1);
        load(code, 0);
        code.op(op::lnot).var(VarOp::Store, Var::Global, 2);
        load(code, 9);
        code.op(op::lnot).var(VarOp::Store, Var::Global, 3);
    });
    SCI_CHECK(world != nullptr);
    if (world != nullptr) {
        SCI_CHECK(world->global(0) == static_cast<std::int16_t>(0xff00));
        SCI_CHECK(world->global(1) == -5 && world->global(2) == 1 && world->global(3) == 0);
    }
}
