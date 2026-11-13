// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hand-built bytecode run through the interpreter (B-06), with no game
// data: each test assembles a play method that leaves results in globals.
// The expected values follow ScummVM's SCI0 interpreter
// (engines/sci/engine/vm.cpp, run_vm), which src/sci-vm/src/machine.cpp
// ports.

#include "bytecode_world.hpp"
#include "kernel.hpp"
#include "sci_test.hpp"

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <print>
#include <string_view>
#include <utility>
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

namespace {

// The kernel number of a function, from the SCI0 kernel table.
int kernel_number(std::string_view name) {
    sci::vm::Kernel kernel;
    kernel.init(false);
    for (std::size_t i = 0; i < kernel.size(); ++i) {
        if (kernel.function(i).name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::unique_ptr<BytecodeWorld> run_game(const std::map<int, sci::test::Bytes>& scripts,
                                        const std::vector<int>& classes) {
    auto game = sci::test::assembled_game(scripts, classes);
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

// Objects, classes and sends (vm.cpp op_send, op_self, op_super, op_class,
// send_selector; selector.cpp lookupSelector): methods found on the
// object's class and its super class, property selectors read and written
// by a send, the property opcodes on self, and the other calls (callb,
// calle, callk) and addresses (lofsa, lofss, lea, selfID, pushSelf,
// pprev).
SCI_TEST(bytecode_objects_sends_and_calls) {
    namespace sel = sci::test::sel;
    using sci::test::ObjectSpec;
    constexpr int value = 8; // Property 4 (value) as a byte offset; property 5 (other) is 10.
    constexpr int other = 10;
    const int abs_number = kernel_number("Abs");
    SCI_CHECK(abs_number >= 0);

    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const auto base_twice = code.label();
    const auto base_sum = code.label();
    const auto derived_twice = code.label();
    const auto exported = code.label();
    const std::vector<std::pair<int, int>> properties{{sel::value, 3}, {sel::other, 0}};
    script.add(ObjectSpec{.is_class = true,
                          .species = 0,
                          .super_class = -1,
                          .extra_properties = properties,
                          .methods = {{sel::twice, base_twice}, {sel::sum, base_sum}}});
    script.add(ObjectSpec{.is_class = true,
                          .species = 1,
                          .super_class = 0,
                          .extra_properties = properties,
                          .methods = {{sel::twice, derived_twice}}});
    const std::size_t game = script.add(ObjectSpec{.is_class = false,
                                                   .species = 1,
                                                   .super_class = 1,
                                                   .extra_properties = {{sel::value, 10}, {sel::other, 0}},
                                                   .methods = {{sel::play, play}}});
    const std::size_t thing = script.add(ObjectSpec{.is_class = false,
                                                    .species = 0,
                                                    .super_class = 0,
                                                    .extra_properties = {{sel::value, 4}, {sel::other, 0}},
                                                    .methods = {}});
    script.export_object(game);
    script.export_code(exported);
    script.set_locals(std::vector<std::uint16_t>(32, 0));

    const auto store = [&code](int global) { code.var(VarOp::Store, Var::Global, global); };
    code.bind(play);
    // 0: self twice: the derived method calls super's, then adds 1.
    code.op_b(op::pushi, sel::twice).op(op::push0).self(2);
    store(0);
    // 1: thing twice (the base class's method).
    code.op_b(op::pushi, sel::twice).op(op::push0).object_address(op::lofsa, thing).send(2);
    store(1);
    // 2: thing sum: 5 + 6.
    code.op_b(op::pushi, sel::sum).op(op::push2).op_b(op::pushi, 5).op_b(op::pushi, 6);
    code.object_address(op::lofsa, thing).send(4);
    store(2);
    // 3, 4: a property selector read by a send, then written and read again.
    code.op_b(op::pushi, sel::value).op(op::push0).object_address(op::lofsa, thing).send(2);
    store(3);
    code.op_b(op::pushi, sel::value).op(op::push1).op_b(op::pushi, 9).object_address(op::lofsa, thing).send(3);
    code.op_b(op::pushi, sel::value).op(op::push0).object_address(op::lofsa, thing).send(2);
    store(4);
    // 5: class 0 itself receives twice: its value is 3.
    code.op_b(op::pushi, sel::twice).op(op::push0).op_b(op::klass, 0).send(2);
    store(5);
    // 6 to 12: the property opcodes on self.
    code.op_b(op::p_toa, value);
    store(6);
    load(code, 50);
    code.op_b(op::a_top, other).op_b(op::p_tos, other);
    load(code, 1);
    code.op(op::add);
    store(7);
    code.op_b(op::pushi, 60).op_b(op::s_top, other).op_b(op::p_toa, other);
    store(8);
    code.op_b(op::ip_toa, other);
    store(9);
    code.op_b(op::dp_toa, other);
    store(10);
    code.op_b(op::ip_tos, other);
    load(code, 0);
    code.op(op::add);
    store(11);
    code.op_b(op::dp_tos, other);
    load(code, 0);
    code.op(op::add);
    store(12);
    // 13, 14: selfID against pushSelf and lofss of the game object.
    code.op(op::push_self).op(op::self_id).op(op::eq);
    store(13);
    code.object_address(op::lofss, game).op(op::self_id).op(op::eq);
    store(14);
    // 15: pprev pushes the accumulator a comparison started with.
    load(code, 3);
    code.op(op::push);
    load(code, 9);
    code.op(op::gt).op(op::pprev);
    load(code, 1);
    code.op(op::add);
    store(15);
    // 16: callb, export 1 of script 0, with one argument.
    code.op(op::push1).op_b(op::pushi, 5).raw({sci::test::opcode_byte(op::callb, true), 1, 2});
    store(16);
    // 17: calle, export 0 of script 1.
    code.op(op::push1).op_b(op::pushi, 7).raw({sci::test::opcode_byte(op::calle, true), 1, 0, 2});
    store(17);
    // 18: callk Abs(-5).
    code.op(op::push1).op_b(op::pushi, -5).callk(abs_number, 1);
    store(18);
    // 19: lea of global 3: its address in the globals' segment.
    code.raw({sci::test::opcode_byte(op::lea, true), 0, 3});
    store(19);
    code.op(op::ret);

    code.bind(base_twice);
    code.op_b(op::p_toa, value).op(op::push);
    load(code, 2);
    code.op(op::mul).op(op::ret);
    code.bind(base_sum);
    code.var(VarOp::Load, Var::Param, 1).op(op::push).var(VarOp::Load, Var::Param, 2).op(op::add).op(op::ret);
    code.bind(derived_twice);
    code.op_b(op::pushi, sel::twice).op(op::push0).super(0, 2).op(op::push);
    load(code, 1);
    code.op(op::add).op(op::ret);
    code.bind(exported);
    code.var(VarOp::Load, Var::Param, 1).op(op::push);
    load(code, 100);
    code.op(op::add).op(op::ret);

    ScriptBuilder script1;
    const auto times_three = script1.code().label();
    script1.code().bind(times_three);
    script1.code().var(VarOp::Load, Var::Param, 1).op(op::push);
    load(script1.code(), 3);
    script1.code().op(op::mul).op(op::ret);
    script1.export_code(times_three);

    const auto world = run_game({{0, script.build()}, {1, script1.build()}}, {0, 0});
    SCI_CHECK(world != nullptr);
    if (world == nullptr) {
        return;
    }
    const std::vector<int> expected{21, 8, 11, 4, 9, 6, 10, 51, 60, 61, 60, 61, 60, 1, 1, 10, 105, 21, 5};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (world->global(i) != expected[i]) {
            std::println(stderr, "global {}: {} instead of {}", i, world->global(i), expected[i]);
        }
        SCI_CHECK(world->global(i) == expected[i]);
    }
    const sci::vm::Reg address = world->machine->global(19);
    SCI_CHECK(address.is_pointer() && address.offset == 6);
}

namespace {

struct VarCase {
    VarOp operation;
    Var type;
    bool to_stack;
    bool indexed;
};

// Every variable opcode, 0x40 to 0x7f.
std::vector<VarCase> all_var_cases() {
    std::vector<VarCase> cases;
    for (const VarOp operation : {VarOp::Load, VarOp::Store, VarOp::Increment, VarOp::Decrement}) {
        for (const bool indexed : {false, true}) {
            for (const bool to_stack : {false, true}) {
                for (const Var type : {Var::Global, Var::Local, Var::Temp, Var::Param}) {
                    cases.push_back(
                        VarCase{.operation = operation, .type = type, .to_stack = to_stack, .indexed = indexed});
                }
            }
        }
    }
    return cases;
}

// The variable each case works on, and the base the indexed form names (the
// accumulator holds the rest of the index).
int case_index(Var type) {
    switch (type) {
    case Var::Global:
    case Var::Local:
        return 100; // Script 0's locals are the globals.
    case Var::Temp:
        return 0;
    case Var::Param:
        return 1;
    }
    return 0;
}

int case_base(const VarCase& c) {
    return c.indexed && (c.type == Var::Global || c.type == Var::Local) ? case_index(c.type) - 1 : case_index(c.type);
}

int case_acc_index(const VarCase& c) {
    return case_index(c.type) - case_base(c);
}

} // namespace

// vm.cpp, the variable opcodes l[as][gltp][i], s[as][gltp][i],
// +[as][gltp][i], -[as][gltp][i]: each of the 64 run on a variable holding
// 10, its result (accumulator or stack) and the variable's new value kept
// in globals. Stores take 20 from the accumulator, or 30 from the stack;
// an indexed store to the accumulator takes its value from the stack too.
SCI_TEST(bytecode_every_variable_opcode) {
    const std::vector<VarCase> cases = all_var_cases();
    SCI_CHECK(cases.size() == 64);
    constexpr int first_result = 120; // Result of case k in global 120 + 2k, the variable in 121 + 2k.
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const auto procedure = code.label();
    code.bind(play);
    code.op(op::push1).op_b(op::pushi, 0).call(procedure, 1).op(op::ret);

    code.bind(procedure);
    code.op_b(op::link, 1);
    for (std::size_t k = 0; k < cases.size(); ++k) {
        const VarCase& c = cases[k];
        const int index = case_index(c.type);
        const int result = first_result + static_cast<int>(2 * k);
        load(code, 10);
        code.var(VarOp::Store, c.type, index);
        if (c.operation == VarOp::Store && (c.to_stack || c.indexed)) {
            code.op_b(op::pushi, c.to_stack ? 30 : 20);
        }
        load(code, c.indexed ? case_acc_index(c) : 20);
        code.var(c.operation, c.type, case_base(c), c.to_stack, c.indexed);
        if (c.operation != VarOp::Store) {
            code.var(VarOp::Store, Var::Global, result, c.to_stack); // From the accumulator or the stack.
        }
        code.var(VarOp::Load, c.type, index).var(VarOp::Store, Var::Global, result + 1);
    }
    code.op(op::ret);
    const auto bytes = sci::test::game_script(script, play, 256);
    const auto world = run_game({{0, bytes}}, {0});
    SCI_CHECK(world != nullptr);
    if (world == nullptr) {
        return;
    }
    for (std::size_t k = 0; k < cases.size(); ++k) {
        const VarCase& c = cases[k];
        int expected_result = 10;
        int expected_value = 10;
        switch (c.operation) {
        case VarOp::Load:
            break;
        case VarOp::Store:
            expected_value = c.to_stack ? 30 : 20;
            break;
        case VarOp::Increment:
            expected_result = expected_value = 11;
            break;
        case VarOp::Decrement:
            expected_result = expected_value = 9;
            break;
        }
        const std::size_t result = 120 + (2 * k);
        const bool result_ok = c.operation == VarOp::Store || world->global(result) == expected_result;
        const bool value_ok = world->global(result + 1) == expected_value;
        if (!result_ok || !value_ok) {
            std::println(stderr, "opcode {:02x}: result {} (expected {}), variable {} (expected {})",
                         sci::test::var_opcode(c.operation, c.type, c.to_stack, c.indexed), world->global(result),
                         expected_result, world->global(result + 1), expected_value);
        }
        SCI_CHECK(result_ok && value_ok);
    }
}
