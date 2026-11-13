// SPDX-License-Identifier: GPL-3.0-or-later
//
// The home-made game (B-07): a synthetic SCI0 game, built at test time by the
// script assembler, run through run_deterministic as the real game is, with
// every kernel call traced. Its results are checked in the trace; the
// expected values follow ScummVM's kernel functions (engines/sci/engine/
// kstring.cpp, klists.cpp, kmath.cpp, kscripts.cpp, kmisc.cpp). No game data.

#include "bytecode_world.hpp"
#include "kernel.hpp"
#include "sci/vm/interpreter.hpp"
#include "sci_test.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using sci::test::CodeAssembler;
using sci::test::ScriptBuilder;
using sci::test::Var;
using sci::test::VarOp;
namespace op = sci::test::op;

namespace {

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

// The trace of a run: "<call> <tick> <name>(<arguments>) = <result>".
struct Trace {
    std::vector<std::string> lines;
    std::optional<std::string> fault;

    // The result of the n-th call (from 0) of a kernel function, as written.
    [[nodiscard]] std::string result(std::string_view name, int nth = 0) const {
        int seen = 0;
        for (const std::string& line : lines) {
            const auto space = line.find(' ', line.find(' ') + 1);
            const std::string_view call = std::string_view(line).substr(space + 1);
            const bool named = call.starts_with(name) && call.size() > name.size() && call[name.size()] == '(';
            if (named && seen++ == nth) {
                return std::string(call.substr(call.rfind(" = ") + 3));
            }
        }
        return "<no call>";
    }
};

Trace run_traced(const std::map<int, sci::test::Bytes>& scripts, const std::vector<int>& classes) {
    Trace trace;
    auto resources = sci::test::assembled_game(scripts, classes);
    SCI_CHECK(resources.has_value());
    if (!resources) {
        return trace;
    }
    sci::vm::RunOptions options;
    options.trace = [&trace](std::string_view line) { trace.lines.emplace_back(line); };
    const sci::vm::RunResult result = sci::vm::run_deterministic(*resources, options);
    trace.fault = result.fault;
    if (result.fault) {
        std::println(stderr, "fault: {}", *result.fault);
    }
    return trace;
}

// Calls kernel function `name` with the arguments `push_args` pushes (it
// pushes `argc` values after the count this pushes first).
void kcall(CodeAssembler& code, std::string_view name, int argc, const auto& push_args) {
    code.op_b(op::pushi, argc);
    push_args();
    code.callk(kernel_number(name), argc);
}

void push(CodeAssembler& code, int value) {
    code.op_w(op::pushi, value);
}

} // namespace

// Strings, lists, maths, scripts and the garbage collector, each kernel
// function called with values whose results ScummVM's would give.
SCI_TEST(home_game_strings_lists_maths_scripts) {
    namespace sel = sci::test::sel;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const std::size_t hello = script.add_string("hello");
    const std::size_t help = script.add_string("help");
    const std::size_t format = script.add_string("%d+%d");
    const std::size_t number = script.add_string("42");
    const auto buffer = [&code] { code.raw({sci::test::opcode_byte(op::lea, true), 0, 40}).op(op::push); };
    const auto string = [&code](std::size_t index) { code.string_address(op::lofss, index); };
    const auto global = [&code](int index) { code.var(VarOp::Load, Var::Global, index, true); };
    const auto keep = [&code](int index) { code.var(VarOp::Store, Var::Global, index); };
    code.bind(play);

    // Strings (kstring.cpp).
    kcall(code, "StrLen", 1, [&] { string(hello); });
    kcall(code, "StrCmp", 2, [&] { string(hello), string(hello); });
    kcall(code, "StrCmp", 3, [&] { string(hello), string(help), push(code, 3); });
    kcall(code, "StrCpy", 2, [&] { buffer(), string(hello); });
    kcall(code, "StrCat", 2, [&] { buffer(), string(help); });
    kcall(code, "StrLen", 1, [&] { buffer(); });
    kcall(code, "StrAt", 2, [&] { buffer(), push(code, 1); });
    kcall(code, "StrAt", 3, [&] { buffer(), push(code, 0), push(code, 'J'); });
    kcall(code, "StrAt", 2, [&] { buffer(), push(code, 0); });
    kcall(code, "StrEnd", 1, [&] { buffer(); });
    kcall(code, "Format", 4, [&] { buffer(), string(format), push(code, 3), push(code, 4); });
    kcall(code, "StrLen", 1, [&] { buffer(); });
    kcall(code, "ReadNumber", 1, [&] { string(number); });

    // Lists (klists.cpp): two nodes, found, walked, one deleted, one added
    // at the front.
    kcall(code, "NewList", 0, [] {});
    keep(0);
    kcall(code, "NewNode", 2, [&] { push(code, 7), push(code, 1); });
    keep(1);
    kcall(code, "AddToEnd", 2, [&] { global(0), global(1); });
    kcall(code, "NewNode", 2, [&] { push(code, 8), push(code, 2); });
    keep(2);
    kcall(code, "AddToEnd", 2, [&] { global(0), global(2); });
    kcall(code, "FirstNode", 1, [&] { global(0); });
    keep(3);
    kcall(code, "NodeValue", 1, [&] { global(3); });
    kcall(code, "NextNode", 1, [&] { global(1); });
    keep(4);
    kcall(code, "NodeValue", 1, [&] { global(4); });
    kcall(code, "LastNode", 1, [&] { global(0); });
    kcall(code, "PrevNode", 1, [&] { global(2); });
    kcall(code, "FindKey", 2, [&] { global(0), push(code, 2); });
    kcall(code, "EmptyList", 1, [&] { global(0); });
    kcall(code, "DeleteKey", 2, [&] { global(0), push(code, 1); });
    kcall(code, "NewNode", 2, [&] { push(code, 9), push(code, 3); });
    keep(5);
    kcall(code, "AddToFront", 2, [&] { global(0), global(5); });
    kcall(code, "NewNode", 2, [&] { push(code, 10), push(code, 4); });
    keep(6);
    kcall(code, "AddAfter", 3, [&] { global(0), global(5), global(6); });
    kcall(code, "FirstNode", 1, [&] { global(0); });
    keep(3);
    kcall(code, "NextNode", 1, [&] { global(3); });
    keep(4);
    kcall(code, "NodeValue", 1, [&] { global(4); });
    kcall(code, "DisposeList", 1, [&] { global(0); });

    // Maths (kmath.cpp).
    kcall(code, "Sqrt", 1, [&] { push(code, 16); });
    kcall(code, "Abs", 1, [&] { push(code, -3); });
    kcall(code, "GetAngle", 4, [&] { push(code, 0), push(code, 0), push(code, 10), push(code, 0); });
    kcall(code, "GetDistance", 2, [&] { push(code, 3), push(code, 4); });
    kcall(code, "SinMult", 2, [&] { push(code, 90), push(code, 100); });
    kcall(code, "CosMult", 2, [&] { push(code, 0), push(code, 100); });
    kcall(code, "SinDiv", 2, [&] { push(code, 90), push(code, 100); });
    kcall(code, "CosDiv", 2, [&] { push(code, 0), push(code, 100); });
    kcall(code, "Random", 2, [&] { push(code, 5), push(code, 5); });

    // Scripts and objects (kscripts.cpp): a clone of the game object, and
    // script 1 loaded by ScriptID and disposed of.
    kcall(code, "Clone", 1, [&] { code.op(op::push_self); });
    keep(7);
    kcall(code, "IsObject", 1, [&] { global(7); });
    kcall(code, "IsObject", 1, [&] { push(code, 5); });
    kcall(code, "RespondsTo", 2, [&] { global(7), push(code, sel::play); });
    kcall(code, "RespondsTo", 2, [&] { global(7), push(code, sel::sum); });
    kcall(code, "DisposeClone", 1, [&] { global(7); });
    kcall(code, "ScriptID", 2, [&] { push(code, 1), push(code, 0); });
    keep(8);
    kcall(code, "IsObject", 1, [&] { global(8); });
    kcall(code, "DisposeScript", 1, [&] { push(code, 1); });

    // The garbage collector (gc.cpp, through kFlushResources) and a few
    // others (kmisc.cpp).
    kcall(code, "FlushResources", 1, [&] { push(code, 0); });
    kcall(code, "GetTime", 0, [] {});
    kcall(code, "Wait", 1, [&] { push(code, 1); });
    kcall(code, "HaveMouse", 0, [] {});
    kcall(code, "MemoryInfo", 1, [&] { push(code, 0); });
    code.op(op::ret);

    // Script 1: an object exported as entry 0, for ScriptID.
    ScriptBuilder script1;
    const std::size_t exported = script1.add(sci::test::ObjectSpec{
        .is_class = false,
        .species = 0,
        .super_class = 0,
        .extra_properties = {},
        .methods = {},
    });
    script1.export_object(exported);

    const Trace trace = run_traced({{0, sci::test::game_script(script, play, 64)}, {1, script1.build()}}, {0});
    SCI_CHECK(!trace.fault.has_value());
    const std::vector<std::pair<std::pair<std::string_view, int>, std::string_view>> expected{
        // Strings.
        {{"StrLen", 0}, "5"},
        {{"StrCmp", 0}, "0"},
        {{"StrCmp", 1}, "0"},
        {{"StrLen", 1}, "9"},
        {{"StrAt", 0}, "101"}, // 'e'
        {{"StrAt", 2}, "74"},  // 'J', written by the second call
        {{"StrLen", 2}, "3"},  // "3+4"
        {{"ReadNumber", 0}, "42"},
        // Lists.
        {{"NodeValue", 0}, "7"},
        {{"NodeValue", 1}, "8"},
        {{"EmptyList", 0}, "0"},
        {{"NodeValue", 2}, "10"}, // After the front node (9) comes the one added after it.
        // Maths.
        {{"Sqrt", 0}, "4"},
        {{"Abs", 0}, "3"},
        {{"GetAngle", 0}, "90"},
        {{"GetDistance", 0}, "5"},
        {{"SinMult", 0}, "100"},
        {{"CosMult", 0}, "100"},
        {{"SinDiv", 0}, "100"},
        {{"CosDiv", 0}, "100"},
        {{"Random", 0}, "5"},
        // Scripts and objects.
        {{"IsObject", 0}, "1"},
        {{"IsObject", 1}, "0"},
        {{"RespondsTo", 0}, "1"},
        {{"RespondsTo", 1}, "0"},
        {{"IsObject", 2}, "1"},
    };
    for (const auto& [call, value] : expected) {
        const std::string got = trace.result(call.first, call.second);
        if (got != value) {
            std::println(stderr, "{} #{}: {} instead of {}", call.first, call.second, got, value);
        }
        SCI_CHECK(got == value);
    }
}
