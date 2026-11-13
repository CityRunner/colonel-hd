// SPDX-License-Identifier: GPL-3.0-or-later
//
// The home-made game (B-07): a synthetic SCI0 game, built at test time by the
// script assembler, run through run_deterministic as the real game is, with
// every kernel call traced. Its results are checked in the trace; the
// expected values follow ScummVM's kernel functions (engines/sci/engine/
// kstring.cpp, klists.cpp, kmath.cpp, kscripts.cpp, kmisc.cpp). No game data.

#include "bytecode_world.hpp"
#include "kernel.hpp"
#include "pic_builder.hpp"
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

Trace run_traced(const std::map<int, sci::test::Bytes>& scripts, const std::vector<int>& classes,
                 const std::vector<sci::test::ExtraResource>& extra = {}) {
    Trace trace;
    auto resources = sci::test::assembled_game(scripts, classes, extra);
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
        for (std::size_t i = trace.lines.size() > 4 ? trace.lines.size() - 4 : 0; i < trace.lines.size(); ++i) {
            std::println(stderr, "  {}", trace.lines[i]);
        }
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

namespace {

// A picture: a red rectangle filled green on white (pic_builder.hpp).
sci::test::Bytes small_picture() {
    sci::test::PicBuilder pic;
    pic.color(4).rectangle(20, 20, 300, 170).color(2).fill(100, 100);
    return pic.end();
}

// The actor class's properties: what kAnimate, kCanBeHere, kDirLoop and
// kBaseSetter read and write, with a view at (100, 120).
std::vector<std::pair<int, int>> actor_properties() {
    using sci::test::selector_id;
    std::vector<std::pair<int, int>> properties;
    const std::vector<std::pair<std::string_view, int>> values{
        {"y", 120},         {"x", 100},    {"view", 1},     {"loop", 0},     {"cel", 0},
        {"underBits", 0},   {"nsTop", 0},  {"nsLeft", 0},   {"nsBottom", 0}, {"nsRight", 0},
        {"lsTop", 0},       {"lsLeft", 0}, {"lsBottom", 0}, {"lsRight", 0},  {"signal", 0},
        {"illegalBits", 0}, {"brTop", 0},  {"brLeft", 0},   {"brBottom", 0}, {"brRight", 0},
        {"priority", 0},    {"z", 0},      {"xStep", 3},    {"yStep", 2},    {"heading", 0},
    };
    properties.reserve(values.size());
    for (const auto& [name, value] : values) {
        properties.emplace_back(selector_id(name), value);
    }
    return properties;
}

} // namespace

// The graphics kernel functions (kgraphics.cpp, kevent.cpp's neighbours):
// a picture drawn and shown, a window with a title, text displayed and
// measured, the ports, a view's loops and cels, a cel drawn and one added
// to the picture, an actor animated, and the Graph subfunctions.
SCI_TEST(home_game_graphics) {
    using sci::test::ObjectSpec;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const std::size_t title = script.add_string("Title");
    const std::size_t text = script.add_string("Hello there");
    const std::size_t actor_class = 1;
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = {},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = static_cast<int>(actor_class),
        .super_class = 0,
        .extra_properties = actor_properties(),
        .methods = {},
    });
    const std::size_t game = script.add(ObjectSpec{
        .is_class = false,
        .species = 0,
        .super_class = 0,
        .extra_properties = {},
        .methods = {{sci::test::sel::play, play}},
    });
    const std::size_t actor = script.add(ObjectSpec{
        .is_class = false,
        .species = 1,
        .super_class = 1,
        .extra_properties = actor_properties(),
        .methods = {},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(64, 0));
    const auto string = [&code](std::size_t index) { code.string_address(op::lofss, index); };
    const auto global = [&code](int index) { code.var(VarOp::Load, Var::Global, index, true); };
    const auto keep = [&code](int index) { code.var(VarOp::Store, Var::Global, index); };
    const auto the_actor = [&code, actor] { code.object_address(op::lofss, actor); };
    const auto rect = [&code] { code.raw({sci::test::opcode_byte(op::lea, true), 0, 50}).op(op::push); };
    code.bind(play);

    kcall(code, "DrawPic", 1, [&] { push(code, 1); });
    kcall(code, "Show", 1, [&] { push(code, 1); });
    kcall(code, "PicNotValid", 0, [] {});
    kcall(code, "GetPort", 0, [] {});
    keep(0);
    kcall(code, "NewWindow", 9, [&] {
        push(code, 20), push(code, 20), push(code, 120), push(code, 200), string(title);
        push(code, 0), push(code, -1), push(code, 0), push(code, 15);
    });
    keep(1);
    kcall(code, "Display", 1, [&] { string(text); });
    kcall(code, "TextSize", 4, [&] { rect(), string(text), push(code, 0), push(code, 0); });
    kcall(code, "SetPort", 1, [&] { global(0); });
    kcall(code, "DisposeWindow", 1, [&] { global(1); });

    kcall(code, "NumLoops", 1, [&] { the_actor(); });
    kcall(code, "NumCels", 1, [&] { the_actor(); });
    kcall(code, "CelWide", 3, [&] { push(code, 1), push(code, 0), push(code, 1); });
    kcall(code, "CelHigh", 3, [&] { push(code, 1), push(code, 0), push(code, 1); });
    kcall(code, "DrawCel", 6,
          [&] { push(code, 1), push(code, 0), push(code, 0), push(code, 50), push(code, 50), push(code, 5); });
    kcall(code, "AddToPic", 7, [&] {
        push(code, 1), push(code, 0), push(code, 1), push(code, 60), push(code, 160), push(code, 6), push(code, 0);
    });

    // The cast: a list holding the actor, animated twice.
    kcall(code, "NewList", 0, [] {});
    keep(2);
    kcall(code, "NewNode", 2, [&] { the_actor(), the_actor(); });
    keep(3);
    kcall(code, "AddToEnd", 2, [&] { global(2), global(3); });
    kcall(code, "Animate", 2, [&] { global(2), push(code, 0); });
    kcall(code, "Animate", 2, [&] { global(2), push(code, 0); });
    kcall(code, "CanBeHere", 2, [&] { the_actor(), global(2); }); // SCI0 passes the cast.
    kcall(code, "OnControl", 3, [&] { push(code, 1), push(code, 100), push(code, 100); });
    kcall(code, "CoordPri", 1, [&] { push(code, 100); });
    kcall(code, "DirLoop", 2, [&] { the_actor(), push(code, 90); });
    kcall(code, "BaseSetter", 1, [&] { the_actor(); });

    // Graph subfunctions (kgraphics.cpp kGraph).
    kcall(code, "Graph", 1, [&] { push(code, 2); });
    kcall(code, "Graph", 6,
          [&] { push(code, 4), push(code, 10), push(code, 10), push(code, 30), push(code, 150), push(code, 1); });
    kcall(code, "Graph", 6,
          [&] { push(code, 7), push(code, 10), push(code, 10), push(code, 40), push(code, 60), push(code, 1); });
    keep(4);
    kcall(code, "Graph", 7, [&] {
        push(code, 11), push(code, 10), push(code, 10), push(code, 40), push(code, 60), push(code, 1), push(code, 5);
    });
    kcall(code, "Graph", 2, [&] { push(code, 8), global(4); });
    kcall(code, "Graph", 5, [&] { push(code, 12), push(code, 10), push(code, 10), push(code, 40), push(code, 60); });
    kcall(code, "Graph", 5, [&] { push(code, 13), push(code, 10), push(code, 10), push(code, 40), push(code, 60); });
    code.op(op::ret);

    const Trace trace =
        run_traced({{0, script.build()}}, {0, 0},
                   {
                       {.type = sci::resource::ResourceType::Pic, .number = 1, .data = small_picture()},
                       {.type = sci::resource::ResourceType::View, .number = 1, .data = sci::test::small_view()},
                   });
    SCI_CHECK(!trace.fault.has_value());
    const std::vector<std::pair<std::pair<std::string_view, int>, std::string_view>> expected{
        {{"NumLoops", 0}, "1"}, {{"NumCels", 0}, "2"},   {{"CelWide", 0}, "8"},
        {{"CelHigh", 0}, "8"},  {{"CanBeHere", 0}, "1"}, {{"GraphGetColorCount", 0}, "16"},
    };
    for (const auto& [call, value] : expected) {
        const std::string got = trace.result(call.first, call.second);
        if (got != value) {
            std::println(stderr, "{} #{}: {} instead of {}", call.first, call.second, got, value);
        }
        SCI_CHECK(got == value);
    }
}
