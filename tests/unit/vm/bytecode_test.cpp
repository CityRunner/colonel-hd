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

// vm.cpp op_bt, op_bnt, op_jmp: relative jumps taken on a non-zero, a zero
// accumulator, or always.
SCI_TEST(bytecode_branches) {
    const auto world = run_play([](CodeAssembler& code) {
        const auto skip_zero = code.label();
        const auto skip_true = code.label();
        const auto skip_always = code.label();
        const auto not_taken = code.label();
        load(code, 0);
        code.branch(op::bnt, skip_zero);
        load(code, 99);
        code.var(VarOp::Store, Var::Global, 0);
        code.bind(skip_zero);
        load(code, 1);
        code.branch(op::bt, skip_true);
        load(code, 99);
        code.var(VarOp::Store, Var::Global, 1);
        code.bind(skip_true);
        code.branch(op::jmp, skip_always);
        load(code, 99);
        code.var(VarOp::Store, Var::Global, 2);
        code.bind(skip_always);
        // Not taken: bt on zero, bnt on non-zero.
        load(code, 0);
        code.branch(op::bt, not_taken);
        load(code, 1);
        code.branch(op::bnt, not_taken);
        load(code, 7);
        code.var(VarOp::Store, Var::Global, 3);
        code.bind(not_taken);
    });
    SCI_CHECK(world != nullptr);
    if (world != nullptr) {
        SCI_CHECK(world->global(0) == 0 && world->global(1) == 0 && world->global(2) == 0);
        SCI_CHECK(world->global(3) == 7);
    }
}

// vm.cpp op_push, op_pushi, op_push0/1/2, op_dup, op_toss.
SCI_TEST(bytecode_stack) {
    const auto world = run_play([](CodeAssembler& code) {
        code.op_b(op::pushi, 10).op(op::push2).op(op::toss).op(op::dup); // 10 10
        load(code, 5);
        code.op(op::add).var(VarOp::Store, Var::Global, 0); // 10 + 5
        load(code, 1);
        code.op(op::add).var(VarOp::Store, Var::Global, 1); // 10 + 1
        code.op(op::push0);
        load(code, 3);
        code.op(op::add).var(VarOp::Store, Var::Global, 2);
        code.op(op::push1);
        load(code, 3);
        code.op(op::add).var(VarOp::Store, Var::Global, 3);
        code.op(op::push2);
        load(code, 3);
        code.op(op::add).var(VarOp::Store, Var::Global, 4);
        load(code, 6);
        code.op(op::push);
        load(code, 4);
        code.op(op::sub).var(VarOp::Store, Var::Global, 5);
    });
    SCI_CHECK(world != nullptr);
    if (world != nullptr) {
        SCI_CHECK(world->global(0) == 15 && world->global(1) == 11 && world->global(2) == 3);
        SCI_CHECK(world->global(3) == 4 && world->global(4) == 5 && world->global(5) == 2);
    }
}

// vm.cpp op_link and the temporaries: a loop summing 5 + 4 + ... + 1 with a
// counter decremented in place (op_minus_at).
SCI_TEST(bytecode_temporaries_and_loop) {
    const auto world = run_play([](CodeAssembler& code) {
        const auto loop = code.label();
        const auto done = code.label();
        code.op_b(op::link, 2);
        load(code, 0);
        code.var(VarOp::Store, Var::Temp, 0);
        load(code, 5);
        code.var(VarOp::Store, Var::Temp, 1);
        code.bind(loop);
        code.var(VarOp::Load, Var::Temp, 1);
        code.branch(op::bnt, done);
        code.var(VarOp::Load, Var::Temp, 0);
        code.op(op::push);
        code.var(VarOp::Load, Var::Temp, 1);
        code.op(op::add);
        code.var(VarOp::Store, Var::Temp, 0);
        code.var(VarOp::Decrement, Var::Temp, 1);
        code.branch(op::jmp, loop);
        code.bind(done);
        code.var(VarOp::Load, Var::Temp, 0);
        code.var(VarOp::Store, Var::Global, 0);
    });
    SCI_CHECK(world != nullptr && world->global(0) == 15);
}

// The variable opcodes 0x40 to 0x7f: load and store to the accumulator or
// the stack, increment and decrement, indexed by the accumulator.
SCI_TEST(bytecode_variable_forms) {
    const auto world = run_play([](CodeAssembler& code) {
        load(code, 40);
        code.var(VarOp::Store, Var::Global, 10);           // sag 10
        code.var(VarOp::Increment, Var::Global, 10);       // +ag 10: 41 into acc and the global
        code.var(VarOp::Store, Var::Global, 0);            // 41
        code.var(VarOp::Increment, Var::Global, 10, true); // +sg 10: 42 onto the stack
        code.var(VarOp::Store, Var::Global, 1, true);      // ssg 1: pops 42
        code.var(VarOp::Decrement, Var::Global, 10, true); // -sg 10: 41 onto the stack
        code.var(VarOp::Store, Var::Global, 2, true);      // 41
        code.var(VarOp::Load, Var::Global, 10, true);      // lsg 10: 41 onto the stack
        load(code, 1);
        code.op(op::add).var(VarOp::Store, Var::Global, 3); // 42
        // Indexed: global 20 + acc.
        load(code, 77);
        code.var(VarOp::Store, Var::Global, 22);
        load(code, 2);
        code.var(VarOp::Load, Var::Global, 20, false, true); // lagi 20: global 22
        code.var(VarOp::Store, Var::Global, 4);
        code.op_b(op::pushi, 55);
        load(code, 3);
        code.var(VarOp::Store, Var::Global, 20, false, true); // sagi 20: pops 55 into global 23
        load(code, 1);
        code.var(VarOp::Increment, Var::Global, 22, false, true); // +agi 22: global 23 + 1
        // Locals of script 0 are its globals.
        code.var(VarOp::Load, Var::Local, 23);
        code.var(VarOp::Store, Var::Global, 5);
    });
    SCI_CHECK(world != nullptr);
    if (world != nullptr) {
        SCI_CHECK(world->global(0) == 41 && world->global(1) == 42 && world->global(2) == 41);
        SCI_CHECK(world->global(3) == 42 && world->global(4) == 77 && world->global(5) == 56);
    }
}

// vm.cpp op_call, op_ret and the parameters (lap: param 0 is the argument
// count); op_rest passes a procedure's parameters on to the next call.
SCI_TEST(bytecode_local_calls_and_rest) {
    const auto world = run_play([](CodeAssembler& code) {
        const auto add_two = code.label();
        const auto pass_on = code.label();
        const auto count_and_sum = code.label();
        // add_two(3, 4) and its argument count.
        code.op_b(op::pushi, 2).op_b(op::pushi, 3).op_b(op::pushi, 4).call(add_two, 2);
        code.var(VarOp::Store, Var::Global, 0);
        // pass_on(10, 20, 30) calls count_and_sum with rest 1.
        code.op_b(op::pushi, 3).op_b(op::pushi, 10).op_b(op::pushi, 20).op_b(op::pushi, 30).call(pass_on, 3);
        code.var(VarOp::Store, Var::Global, 2);
        code.op(op::ret);

        code.bind(add_two);
        code.var(VarOp::Load, Var::Param, 0);
        code.var(VarOp::Store, Var::Global, 1);
        code.var(VarOp::Load, Var::Param, 1);
        code.op(op::push);
        code.var(VarOp::Load, Var::Param, 2);
        code.op(op::add).op(op::ret);

        code.bind(pass_on);
        code.op(op::push0).op_b(op::rest, 1).call(count_and_sum, 0).op(op::ret);

        code.bind(count_and_sum);
        code.var(VarOp::Load, Var::Param, 1);
        code.op(op::push);
        code.var(VarOp::Load, Var::Param, 2);
        code.op(op::add).op(op::push);
        code.var(VarOp::Load, Var::Param, 3);
        code.op(op::add);
        code.var(VarOp::Store, Var::Global, 3);
        code.var(VarOp::Load, Var::Param, 0);
        code.op(op::ret);
    });
    SCI_CHECK(world != nullptr);
    if (world != nullptr) {
        SCI_CHECK(world->global(0) == 7 && world->global(1) == 2);
        SCI_CHECK(world->global(2) == 3 && world->global(3) == 60);
    }
}
