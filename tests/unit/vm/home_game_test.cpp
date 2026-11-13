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
#include "sample_vocabulary.hpp"
#include "save_game.hpp"
#include "sci/vm/interpreter.hpp"
#include "sci_test.hpp"
#include "state.hpp"

#include <array>
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
                 const std::vector<sci::test::ExtraResource>& extra = {}, std::vector<sci::vm::InputEvent> input = {}) {
    Trace trace;
    auto resources = sci::test::assembled_game(scripts, classes, extra);
    SCI_CHECK(resources.has_value());
    if (!resources) {
        return trace;
    }
    sci::vm::RunOptions options;
    options.input = std::move(input);
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
        {"y", 120},      {"x", 100},       {"view", 1},     {"loop", 0},        {"cel", 0},   {"underBits", 0},
        {"nsTop", 0},    {"nsLeft", 0},    {"nsBottom", 0}, {"nsRight", 0},     {"lsTop", 0}, {"lsLeft", 0},
        {"lsBottom", 0}, {"lsRight", 0},   {"signal", 0},   {"illegalBits", 0}, {"brTop", 0}, {"brLeft", 0},
        {"brBottom", 0}, {"brRight", 0},   {"priority", 0}, {"z", 0},           {"xStep", 3}, {"yStep", 2},
        {"heading", 0},  {"moveSpeed", 0},
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

namespace {

// Properties by selector name, for objects of one class.
std::vector<std::pair<int, int>> properties_of(const std::vector<std::pair<std::string_view, int>>& values) {
    std::vector<std::pair<int, int>> properties;
    properties.reserve(values.size());
    for (const auto& [name, value] : values) {
        properties.emplace_back(sci::test::selector_id(name), value);
    }
    return properties;
}

// A dialog control: what kDrawControl, kHiliteControl and kEditControl read.
std::vector<std::pair<int, int>> control_properties(int type, int left, int top) {
    return properties_of({
        {"type", type},
        {"state", 1},
        {"nsTop", top},
        {"nsLeft", left},
        {"nsBottom", top + 10},
        {"nsRight", left + 60},
        {"text", 0},
        {"font", 0},
        {"mode", 0},
        {"cursor", 0},
        {"max", 20},
        {"view", 1},
        {"loop", 0},
        {"cel", 0},
        {"x", 4},
        {"brTop", 0},
    });
}

} // namespace

// Dialog controls, menus and movement (kgraphics.cpp kDrawControl,
// kHiliteControl, kEditControl; kmenu.cpp; kmovement.cpp): each control
// type drawn, the button highlighted, the edit field edited; a menu added,
// the status line and the menu bar drawn, a menu attribute read and set,
// a menu selection with an empty event; a mover driving the actor by
// InitBresen and DoBresen, and SetJump.
SCI_TEST(home_game_controls_menus_movement) {
    using sci::test::ObjectSpec;
    using sci::test::selector_id;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const auto can_be_here = code.label();
    const std::size_t ok = script.add_string("OK");
    const std::size_t label = script.add_string("Label");
    const std::size_t edit_text = script.add_string("edit me");
    const std::size_t entry_one = script.add_string("one");
    script.add_string("two");
    script.add_string("");
    const std::size_t menu_title = script.add_string("File");
    const std::size_t menu_items = script.add_string("Save:Quit");
    const std::size_t status = script.add_string("Status line");
    // Classes: 0 the game's, 1 actors, 2 controls, 3 events, 4 movers.
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = {},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 1,
        .super_class = 0,
        .extra_properties = actor_properties(),
        .methods = {{selector_id("canBeHere"), can_be_here}},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 2,
        .super_class = 0,
        .extra_properties = control_properties(1, 0, 0),
        .methods = {},
    });
    const auto event_properties =
        properties_of({{"type", 0}, {"message", 0}, {"modifiers", 0}, {"y", 0}, {"x", 0}, {"claimed", 0}});
    script.add(ObjectSpec{
        .is_class = true,
        .species = 3,
        .super_class = 0,
        .extra_properties = event_properties,
        .methods = {},
    });
    const auto mover_properties = properties_of({
        {"client", 0},
        {"x", 160},
        {"y", 150},
        {"dx", 0},
        {"dy", 0},
        {"b-moveCnt", 0},
        {"b-i1", 0},
        {"b-i2", 0},
        {"b-di", 0},
        {"b-xAxis", 0},
        {"b-incr", 0},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 4,
        .super_class = 0,
        .extra_properties = mover_properties,
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
    std::vector<std::size_t> controls;
    controls.reserve(5);
    const std::vector<int> types{1, 2, 3, 4, 6};
    for (std::size_t i = 0; i < types.size(); ++i) {
        controls.push_back(script.add(ObjectSpec{
            .is_class = false,
            .species = 2,
            .super_class = 2,
            .extra_properties = control_properties(types[i], 20, 20 + (static_cast<int>(i) * 20)),
            .methods = {},
        }));
    }
    const std::size_t event = script.add(ObjectSpec{
        .is_class = false,
        .species = 3,
        .super_class = 3,
        .extra_properties = event_properties,
        .methods = {},
    });
    const std::size_t mover = script.add(ObjectSpec{
        .is_class = false,
        .species = 4,
        .super_class = 4,
        .extra_properties = mover_properties,
        .methods = {},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(64, 0));
    const auto object = [&code](std::size_t index) { code.object_address(op::lofss, index); };
    const auto string = [&code](std::size_t index) { code.string_address(op::lofss, index); };
    // Sets a property to an address: selector, 1, the address, sent to the object.
    const auto set_pointer = [&code](std::size_t target, std::string_view selector, const auto& push_value) {
        code.op_w(op::pushi, selector_id(selector)).op(op::push1);
        push_value();
        code.object_address(op::lofsa, target).send(3);
    };
    code.bind(play);
    kcall(code, "DrawPic", 1, [&] { push(code, 1); });
    const std::vector<std::size_t> texts{ok, label, edit_text, ok, entry_one};
    for (std::size_t i = 0; i < controls.size(); ++i) {
        set_pointer(controls[i], "text", [&] { string(texts[i]); });
        kcall(code, "DrawControl", 1, [&] { object(controls[i]); });
    }
    kcall(code, "HiliteControl", 1, [&] { object(controls[0]); });
    kcall(code, "EditControl", 2, [&] { object(controls[2]), object(event); });

    kcall(code, "AddMenu", 2, [&] { string(menu_title), string(menu_items); });
    kcall(code, "DrawStatus", 1, [&] { string(status); });
    kcall(code, "DrawMenuBar", 1, [&] { push(code, 1); });
    kcall(code, "GetMenu", 2, [&] { push(code, 0x101), push(code, 0x6d); });
    kcall(code, "SetMenu", 3, [&] { push(code, 0x101), push(code, 0x6d), push(code, 1); });
    kcall(code, "MenuSelect", 1, [&] { object(event); });
    kcall(code, "DrawMenuBar", 1, [&] { push(code, 0); });

    set_pointer(mover, "client", [&] { object(actor); });
    kcall(code, "InitBresen", 1, [&] { object(mover); });
    for (int step = 0; step < 4; ++step) {
        kcall(code, "DoBresen", 1, [&] { object(mover); });
    }
    kcall(code, "SetJump", 4, [&] { object(actor), push(code, 20), push(code, -10), push(code, 2); });
    code.op(op::ret);
    // The actor's canBeHere: always true, so every step of the mover lands.
    code.bind(can_be_here);
    code.op_b(op::ldi, 1).op(op::ret);

    const Trace trace =
        run_traced({{0, script.build()}}, {0, 0, 0, 0, 0},
                   {
                       {.type = sci::resource::ResourceType::Pic, .number = 1, .data = small_picture()},
                       {.type = sci::resource::ResourceType::View, .number = 1, .data = sci::test::small_view()},
                   });
    SCI_CHECK(!trace.fault.has_value());
    SCI_CHECK(trace.result("DoBresen", 3) != "<no call>");
    SCI_CHECK(trace.result("SetJump", 0) != "<no call>");
}

// The parser, sound, the cursor, events and the rest of kmisc (kparse.cpp,
// ksound.cpp, kgraphics.cpp kSetCursor, kevent.cpp, kmisc.cpp, kfile.cpp
// kDeviceInfo and friends): sentences parsed (one that parses, one with an
// unknown word, one that does not parse) and a Said spec matched; the
// synonyms set from an empty list of scripts; a sound initialised, played,
// paused, faded, stopped and disposed while time passes; the cursor shown,
// hidden, shaped and moved; a key and a mouse press read as events and
// mapped; the device and path queries.
SCI_TEST(home_game_parser_sound_events) {
    namespace vocab = sci::test::vocab;
    using sci::test::ObjectSpec;
    using sci::test::selector_id;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const auto word_fail = code.label();
    const auto syntax_fail = code.label();
    const std::size_t look_door = script.add_string("look door");
    const std::size_t unknown = script.add_string("xyzzy door");
    const std::size_t no_parse = script.add_string("look door key");
    const std::size_t take_it = script.add_string("take it");
    const std::array<char, 6> spec_bytes{
        0, vocab::group_look, static_cast<char>(0xF2), 0, vocab::group_door, static_cast<char>(0xFF),
    };
    const std::size_t said_spec = script.add_string(std::string(spec_bytes.begin(), spec_bytes.end()));
    const std::size_t path = script.add_string("C:\\GAMES");
    const auto buffer = [&code](int global) {
        code.raw({sci::test::opcode_byte(op::lea, true), 0, static_cast<std::uint8_t>(global)}).op(op::push);
    };
    const auto keep = [&code](int index) { code.var(VarOp::Store, Var::Global, index); };
    const auto global = [&code](int index) { code.var(VarOp::Load, Var::Global, index, true); };
    // Classes: 0 the game's (with the parser's failure methods), 1 events,
    // 2 sounds, 3 regions (scripts with synonyms: a number and a list).
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = {{selector_id("wordFail"), word_fail}, {selector_id("syntaxFail"), syntax_fail}},
    });
    const auto event_properties =
        properties_of({{"type", 0}, {"message", 0}, {"modifiers", 0}, {"y", 0}, {"x", 0}, {"claimed", 0}});
    script.add(ObjectSpec{
        .is_class = true,
        .species = 1,
        .super_class = 0,
        .extra_properties = event_properties,
        .methods = {},
    });
    const auto sound_properties = properties_of({
        {"number", 1},
        {"loop", 1},
        {"priority", 0},
        {"state", 0},
        {"signal", 0},
        {"vol", 127},
        {"handle", 0},
        {"dataInc", 0},
        {"nodePtr", 0},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 2,
        .super_class = 0,
        .extra_properties = sound_properties,
        .methods = {},
    });
    const auto region_properties = properties_of({{"number", 0}, {"elements", 0}});
    script.add(ObjectSpec{
        .is_class = true,
        .species = 3,
        .super_class = 0,
        .extra_properties = region_properties,
        .methods = {},
    });
    const std::size_t game = script.add(ObjectSpec{
        .is_class = false,
        .species = 0,
        .super_class = 0,
        .extra_properties = {},
        .methods = {{sci::test::sel::play, play}},
    });
    const std::size_t event = script.add(ObjectSpec{
        .is_class = false,
        .species = 1,
        .super_class = 1,
        .extra_properties = event_properties,
        .methods = {},
    });
    const std::size_t sound = script.add(ObjectSpec{
        .is_class = false,
        .species = 2,
        .super_class = 2,
        .extra_properties = sound_properties,
        .methods = {},
    });
    const std::size_t region = script.add(ObjectSpec{
        .is_class = false,
        .species = 3,
        .super_class = 3,
        .extra_properties = region_properties,
        .methods = {},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(64, 0));
    const auto object = [&code](std::size_t index) { code.object_address(op::lofss, index); };
    const auto string = [&code](std::size_t index) { code.string_address(op::lofss, index); };
    const auto sound_call = [&](int subfunction, int argc, const auto& push_args) {
        kcall(code, "DoSound", argc + 1, [&] {
            push(code, subfunction);
            push_args();
        });
    };
    code.bind(play);

    // The parser.
    kcall(code, "Parse", 2, [&] { string(look_door), object(event); });
    kcall(code, "Said", 1, [&] { string(said_spec); });
    kcall(code, "Parse", 2, [&] { string(take_it), object(event); });
    kcall(code, "Parse", 2, [&] { string(unknown), object(event); });
    kcall(code, "Parse", 2, [&] { string(no_parse), object(event); });
    kcall(code, "Said", 1, [&] { string(said_spec); });
    kcall(code, "NewList", 0, [] {});
    keep(0);
    kcall(code, "NewNode", 2, [&] { object(region), object(region); });
    keep(1);
    kcall(code, "AddToEnd", 2, [&] { global(0), global(1); });
    code.op_w(op::pushi, selector_id("elements")).op(op::push1);
    global(0);
    code.object_address(op::lofsa, region).send(3);
    kcall(code, "SetSynonyms", 1, [&] { object(region); });

    // Sound.
    sound_call(0, 1, [&] { object(sound); }); // Init.
    sound_call(1, 1, [&] { object(sound); }); // Play.
    kcall(code, "Wait", 1, [&] { push(code, 20); });
    sound_call(9, 1, [&] { object(sound); });  // Update.
    sound_call(6, 1, [&] { push(code, 1); });  // Pause.
    sound_call(6, 1, [&] { push(code, 0); });  // Resume.
    sound_call(4, 0, [] {});                   // Mute (query).
    sound_call(4, 1, [&] { push(code, 0); });  // Mute off.
    sound_call(8, 0, [] {});                   // Master volume (query).
    sound_call(8, 1, [&] { push(code, 10); }); // Master volume.
    sound_call(11, 0, [] {});                  // Polyphony.
    kcall(code, "Wait", 1, [&] { push(code, 40); });
    kcall(code, "GetEvent", 2, [&] { push(code, sci::vm::event_any), object(event); });
    sound_call(10, 1, [&] { object(sound); }); // Fade.
    sound_call(5, 1, [&] { object(sound); });  // Stop.
    sound_call(2, 1, [&] { object(sound); });  // Restore.
    sound_call(7, 0, [] {});                   // Resume after restore.
    sound_call(3, 1, [&] { object(sound); });  // Dispose.
    sound_call(12, 0, [] {});                  // Stop all.

    // The cursor.
    kcall(code, "SetCursor", 2, [&] { push(code, 0), push(code, 1); });
    kcall(code, "SetCursor", 2, [&] { push(code, 0), push(code, 0); });
    kcall(code, "SetCursor", 4, [&] { push(code, 0), push(code, 1), push(code, 100), push(code, 80); });

    // Events: the key, then the mouse press, then nothing.
    for (int i = 0; i < 3; ++i) {
        kcall(code, "GetEvent", 2, [&] { push(code, sci::vm::event_any), object(event); });
        kcall(code, "MapKeyToDir", 1, [&] { object(event); });
        kcall(code, "GlobalToLocal", 1, [&] { object(event); });
        kcall(code, "LocalToGlobal", 1, [&] { object(event); });
    }
    kcall(code, "Joystick", 1, [&] { push(code, 12); });

    // Devices and paths.
    kcall(code, "DeviceInfo", 3, [&] { push(code, 0), string(path), buffer(40); });
    kcall(code, "DeviceInfo", 2, [&] { push(code, 1), buffer(40); });
    kcall(code, "DeviceInfo", 3, [&] { push(code, 2), string(path), string(path); });
    kcall(code, "DeviceInfo", 1, [&] { push(code, 3); });
    kcall(code, "DeviceInfo", 2, [&] { push(code, 5), buffer(40); });
    kcall(code, "DeviceInfo", 4, [&] { push(code, 6), buffer(40), buffer(50), push(code, 100); });
    kcall(code, "GetCWD", 1, [&] { buffer(40); });
    kcall(code, "CheckFreeSpace", 1, [&] { string(path); });
    kcall(code, "ValidPath", 1, [&] { string(path); });
    kcall(code, "SetDebug", 0, [] {});
    kcall(code, "GetSaveDir", 0, [] {});
    kcall(code, "GameIsRestarting", 0, [] {});
    kcall(code, "GameIsRestarting", 1, [&] { push(code, 0); });
    code.op(op::ret);
    // The game's wordFail and syntaxFail: nothing to say.
    code.bind(word_fail);
    code.op(op::ret);
    code.bind(syntax_fail);
    code.op(op::ret);

    const std::vector<std::uint8_t> song{0x00, 0x90, 0x3C, 0x40, 0x1E, 0xCF, 0x05, 0x1E, 0xBF, 0x60, 0x02, 0x00, 0xFC};
    sci::test::Bytes sound_resource(0x21, 0);
    sound_resource[2] = 0x04; // Channel 0 plays on AdLib.
    sound_resource.insert(sound_resource.end(), song.begin(), song.end());
    sci::test::Bytes cursor(68, 0);
    cursor[3] = 1; // The hotspot in the centre.
    sci::vm::SciEvent key;
    key.type = sci::vm::event_key_down;
    key.character = sci::vm::key_up;
    sci::vm::SciEvent press;
    press.type = sci::vm::event_mouse_press;
    press.mouse = sci::vm::Point{.x = 50, .y = 60};
    using sci::resource::ResourceType;
    const Trace trace = run_traced({{0, script.build()}}, {0, 0, 0, 0},
                                   {
                                       {.type = ResourceType::Vocab, .number = 0, .data = vocab::sample_words()},
                                       {.type = ResourceType::Vocab, .number = 900, .data = vocab::sample_branches()},
                                       {.type = ResourceType::Vocab, .number = 901, .data = vocab::sample_suffixes()},
                                       {.type = ResourceType::Sound, .number = 1, .data = sound_resource},
                                       {.type = ResourceType::Cursor, .number = 0, .data = cursor},
                                   },
                                   {
                                       {.tick = 0, .event = key, .move_only = false},
                                       {.tick = 0, .event = press, .move_only = false},
                                   });
    SCI_CHECK(!trace.fault.has_value());
    SCI_CHECK(trace.result("Said", 0) == "1");
    SCI_CHECK(trace.result("Parse", 0) == "1");
    SCI_CHECK(trace.result("MapKeyToDir", 0) == "1");
    SCI_CHECK(trace.result("GameIsRestarting", 1) != "<no call>");
}

namespace {

// A home-made game whose play method leaves state of every kind behind: a
// picture, an open window, a cast list with an actor animated, a clone, a
// playing sound and some globals.
std::optional<sci::resource::ResourceManager> rich_state_game() {
    using sci::test::ObjectSpec;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const std::size_t title = script.add_string("Saved");
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = {},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 1,
        .super_class = 0,
        .extra_properties = actor_properties(),
        .methods = {},
    });
    const auto sound_properties = properties_of({
        {"number", 1},
        {"loop", -1},
        {"priority", 0},
        {"state", 0},
        {"signal", 0},
        {"vol", 127},
        {"handle", 0},
        {"dataInc", 0},
        {"nodePtr", 0},
    });
    script.add(ObjectSpec{
        .is_class = true,
        .species = 2,
        .super_class = 0,
        .extra_properties = sound_properties,
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
    const std::size_t sound = script.add(ObjectSpec{
        .is_class = false,
        .species = 2,
        .super_class = 2,
        .extra_properties = sound_properties,
        .methods = {},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(64, 0));
    const auto object = [&code](std::size_t index) { code.object_address(op::lofss, index); };
    const auto keep = [&code](int index) { code.var(VarOp::Store, Var::Global, index); };
    const auto global = [&code](int index) { code.var(VarOp::Load, Var::Global, index, true); };
    code.bind(play);
    kcall(code, "DrawPic", 1, [&] { push(code, 1); });
    kcall(code, "NewWindow", 9, [&] {
        push(code, 20), push(code, 20), push(code, 120), push(code, 200);
        code.string_address(op::lofss, title);
        push(code, 0), push(code, -1), push(code, 0), push(code, 15);
    });
    keep(1);
    kcall(code, "NewList", 0, [] {});
    keep(2);
    kcall(code, "NewNode", 2, [&] { object(actor), object(actor); });
    keep(3);
    kcall(code, "AddToEnd", 2, [&] { global(2), global(3); });
    kcall(code, "Animate", 2, [&] { global(2), push(code, 0); });
    kcall(code, "Clone", 1, [&] { object(actor); });
    keep(4);
    kcall(code, "DoSound", 2, [&] { push(code, 0), object(sound); });
    kcall(code, "DoSound", 2, [&] { push(code, 1), object(sound); });
    kcall(code, "Wait", 1, [&] { push(code, 10); });
    code.op_b(op::ldi, 77);
    keep(5);
    code.op(op::ret);
    sci::test::Bytes sound_resource(0x21, 0);
    sound_resource[2] = 0x04;
    const std::vector<std::uint8_t> song{0x00, 0x90, 0x3C, 0x40, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFC};
    sound_resource.insert(sound_resource.end(), song.begin(), song.end());
    using sci::resource::ResourceType;
    return sci::test::assembled_game({{0, script.build()}}, {0, 0, 0},
                                     {
                                         {.type = ResourceType::Pic, .number = 1, .data = small_picture()},
                                         {.type = ResourceType::View, .number = 1, .data = sci::test::small_view()},
                                         {.type = ResourceType::Sound, .number = 1, .data = sound_resource},
                                     });
}

} // namespace

// A save of the home-made game read into a fresh machine of the same game
// gives the same persistent state: every component but the execution.
SCI_TEST(home_game_save_and_restore_round_trip) {
    auto source_game = rich_state_game();
    auto target_game = rich_state_game();
    SCI_CHECK(source_game.has_value() && target_game.has_value());
    if (!source_game || !target_game) {
        return;
    }
    sci::test::BytecodeWorld source(std::move(*source_game));
    SCI_CHECK(source.run());
    SCI_CHECK(source.global(5) == 77);
    const sci::vm::StateHash before = sci::vm::state_hash(*source.machine);
    const std::vector<std::uint8_t> file = sci::vm::write_save(*source.machine, "home", "test");
    SCI_CHECK(sci::vm::state_hash(*source.machine).total == before.total);

    sci::test::BytecodeWorld target(std::move(*target_game));
    SCI_CHECK(target.machine->init_game());
    SCI_CHECK(sci::vm::state_hash(*target.machine).persistent != before.persistent);
    SCI_CHECK(sci::vm::read_save(*target.machine, file).has_value());
    const sci::vm::StateHash after = sci::vm::state_hash(*target.machine);
    SCI_CHECK(after.persistent == before.persistent);
    for (std::size_t i = 1; i < before.components.size() && i < after.components.size(); ++i) {
        if (after.components[i].digest != before.components[i].digest) {
            std::println(stderr, "component {} differs", before.components[i].name);
        }
        SCI_CHECK(after.components[i].digest == before.components[i].digest);
    }
    SCI_CHECK(target.global(5) == 77);
}

// Menus driven by input (kmenu.cpp kMenuSelect, GfxMenu): Escape opens the
// bar and the arrow keys walk the menus and items until Enter chooses one;
// Ctrl and Alt keys and a Said event match items; a press on the bar
// follows the mouse until the release.
SCI_TEST(home_game_menus_with_input) {
    using sci::test::ObjectSpec;
    using sci::test::selector_id;
    ScriptBuilder script;
    CodeAssembler& code = script.code();
    const auto play = code.label();
    const std::size_t file_title = script.add_string("File");
    const std::size_t file_items = script.add_string("Save`^s:--!:Quit`@q");
    const std::size_t speed_title = script.add_string("Speed");
    const std::size_t speed_items = script.add_string("Faster`#2:Slower");
    script.add(ObjectSpec{
        .is_class = true,
        .species = 0,
        .super_class = -1,
        .extra_properties = {},
        .methods = {},
    });
    const auto event_properties =
        properties_of({{"type", 0}, {"message", 0}, {"modifiers", 0}, {"y", 0}, {"x", 0}, {"claimed", 0}});
    script.add(ObjectSpec{
        .is_class = true,
        .species = 1,
        .super_class = 0,
        .extra_properties = event_properties,
        .methods = {},
    });
    const std::size_t game = script.add(ObjectSpec{
        .is_class = false,
        .species = 0,
        .super_class = 0,
        .extra_properties = {},
        .methods = {{sci::test::sel::play, play}},
    });
    const std::size_t event = script.add(ObjectSpec{
        .is_class = false,
        .species = 1,
        .super_class = 1,
        .extra_properties = event_properties,
        .methods = {},
    });
    script.export_object(game);
    script.set_locals(std::vector<std::uint16_t>(64, 0));
    const auto string = [&code](std::size_t index) { code.string_address(op::lofss, index); };
    // The event's type, message, modifiers, y and x, in one send.
    const auto set_event = [&](int type, int message, int modifiers, int y, int x) {
        for (const auto& [selector, value] : std::vector<std::pair<std::string_view, int>>{
                 {"type", type},
                 {"message", message},
                 {"modifiers", modifiers},
                 {"y", y},
                 {"x", x},
             }) {
            code.op_w(op::pushi, selector_id(selector)).op(op::push1);
            push(code, value);
        }
        code.object_address(op::lofsa, event).send(15);
    };
    const auto select = [&] { kcall(code, "MenuSelect", 1, [&] { code.object_address(op::lofss, event); }); };
    code.bind(play);
    kcall(code, "AddMenu", 2, [&] { string(file_title), string(file_items); });
    kcall(code, "AddMenu", 2, [&] { string(speed_title), string(speed_items); });
    kcall(code, "DrawMenuBar", 1, [&] { push(code, 1); });
    set_event(sci::vm::event_key_down, sci::vm::key_escape, 0, 0, 0);
    select();
    set_event(sci::vm::event_key_down, 19, sci::vm::modifier_ctrl, 0, 0); // Ctrl+S.
    select();
    set_event(sci::vm::event_key_down, 'q', sci::vm::modifier_alt, 0, 0); // Alt+Q.
    select();
    set_event(sci::vm::event_said, 0, 0, 0, 0);
    select();
    set_event(sci::vm::event_mouse_press, 0, 0, 2, 10); // On the bar.
    select();
    code.op(op::ret);

    const auto key = [](std::uint16_t character) {
        sci::vm::SciEvent event;
        event.type = sci::vm::event_key_down;
        event.character = character;
        return sci::vm::InputEvent{.tick = 0, .event = event, .move_only = false};
    };
    const auto mouse = [](std::uint16_t type, std::int16_t x, std::int16_t y, bool move_only) {
        sci::vm::SciEvent event;
        event.type = type;
        event.mouse = sci::vm::Point{.x = x, .y = y};
        return sci::vm::InputEvent{.tick = 0, .event = event, .move_only = move_only};
    };
    const Trace trace = run_traced({{0, script.build()}}, {0, 0}, {},
                                   {
                                       key(sci::vm::key_right),
                                       key(sci::vm::key_down),
                                       key(sci::vm::key_up),
                                       key(sci::vm::key_left),
                                       key(sci::vm::key_down),
                                       key(sci::vm::key_enter),
                                       mouse(sci::vm::event_none, 10, 2, true),
                                       mouse(sci::vm::event_none, 10, 14, true),
                                       mouse(sci::vm::event_none, 60, 2, true),
                                       mouse(sci::vm::event_none, 60, 14, true),
                                       mouse(sci::vm::event_mouse_release, 60, 14, false),
                                   });
    SCI_CHECK(!trace.fault.has_value());
    // Escape, right, down, up, left, down skips the separator: Quit, item 3.
    SCI_CHECK(trace.result("MenuSelect", 0) == "259");
    SCI_CHECK(trace.result("MenuSelect", 1) == "257"); // Ctrl+S: menu 1, item 1.
    SCI_CHECK(trace.result("MenuSelect", 2) == "259"); // Alt+Q.
    SCI_CHECK(trace.result("MenuSelect", 3) == "0");   // No parse to match.
    SCI_CHECK(trace.result("MenuSelect", 4) != "<no call>");
}
