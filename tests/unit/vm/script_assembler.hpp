// SPDX-License-Identifier: GPL-3.0-or-later
//
// A test-side assembler for SCI0 scripts (B-06): readable opcodes with
// labels, objects and classes with properties and methods, and the
// vocabularies a machine needs to run them (selector names, vocab 997; the
// class table, vocab 996). With it, unit tests run hand-built bytecode
// through the interpreter without game data. The layouts follow ScummVM's
// SCI0 reading of them (engines/sci/engine/script.cpp, object.cpp,
// kernel_tables.h) as src/sci-vm ports it (script.cpp, object.cpp,
// bytecode.cpp).

#pragma once

#include "builders.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sci::test {

// SCI0 opcode numbers (the opcode byte is `op << 1`, bit 0 set for byte
// operands); names as in ScummVM's disassembler.
namespace op {
inline constexpr int bnot = 0x00, add = 0x01, sub = 0x02, mul = 0x03, div = 0x04, mod = 0x05, shr = 0x06, shl = 0x07,
                     bxor = 0x08, band = 0x09, bor = 0x0a, neg = 0x0b, lnot = 0x0c, eq = 0x0d, ne = 0x0e, gt = 0x0f,
                     ge = 0x10, lt = 0x11, le = 0x12, ugt = 0x13, uge = 0x14, ult = 0x15, ule = 0x16, bt = 0x17,
                     bnt = 0x18, jmp = 0x19, ldi = 0x1a, push = 0x1b, pushi = 0x1c, toss = 0x1d, dup = 0x1e,
                     link = 0x1f, call = 0x20, callk = 0x21, callb = 0x22, calle = 0x23, ret = 0x24, send = 0x25,
                     klass = 0x28, self = 0x2a, super = 0x2b, rest = 0x2c, lea = 0x2d, self_id = 0x2e, pprev = 0x30,
                     p_toa = 0x31, a_top = 0x32, p_tos = 0x33, s_top = 0x34, ip_toa = 0x35, dp_toa = 0x36,
                     ip_tos = 0x37, dp_tos = 0x38, lofsa = 0x39, lofss = 0x3a, push0 = 0x3b, push1 = 0x3c, push2 = 0x3d,
                     push_self = 0x3e;
} // namespace op

// The variable groups of opcodes 0x40 to 0x7f.
enum class Var : std::uint8_t { Global = 0, Local = 1, Temp = 2, Param = 3 };
enum class VarOp : std::uint8_t { Load = 0, Store = 1, Increment = 2, Decrement = 3 };

// The opcode of a variable access: `[ls+-][as][gltp][i]` (ScummVM's names).
constexpr int var_opcode(VarOp operation, Var type, bool to_stack = false, bool indexed = false) {
    return 0x40 + (static_cast<int>(operation) * 16) + (indexed ? 8 : 0) + (to_stack ? 4 : 0) + static_cast<int>(type);
}

// The opcode byte: the opcode shifted left, bit 0 set for byte operands.
constexpr std::uint8_t opcode_byte(int opcode, bool byte_operands) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(opcode) << 1U) | (byte_operands ? 1U : 0U));
}

// Bytecode with labels. Branches and calls are relative, so the code does
// not depend on where the script places it.
class CodeAssembler {
public:
    using Label = std::size_t;

    [[nodiscard]] Label label() {
        labels_.emplace_back(std::nullopt);
        return labels_.size() - 1;
    }

    void bind(Label label) { labels_[label] = bytes_.size(); }

    // The offset of the next byte within the code.
    [[nodiscard]] std::size_t here() const { return bytes_.size(); }

    // An opcode without operands.
    CodeAssembler& op(int opcode) {
        bytes_.push_back(opcode_byte(opcode, false));
        return *this;
    }

    // An opcode with one byte operand (the byte form).
    CodeAssembler& op_b(int opcode, int operand) {
        bytes_.push_back(opcode_byte(opcode, true));
        bytes_.push_back(static_cast<std::uint8_t>(operand));
        return *this;
    }

    // An opcode with one word operand (the word form).
    CodeAssembler& op_w(int opcode, int operand) {
        bytes_.push_back(opcode_byte(opcode, false));
        append_u16le(bytes_, static_cast<std::uint16_t>(operand));
        return *this;
    }

    // Raw bytes, for operand layouts the helpers do not cover.
    CodeAssembler& raw(std::initializer_list<std::uint8_t> bytes) {
        bytes_.insert(bytes_.end(), bytes);
        return *this;
    }

    // A variable access with a byte index.
    CodeAssembler& var(VarOp operation, Var type, int index, bool to_stack = false, bool indexed = false) {
        return op_b(var_opcode(operation, type, to_stack, indexed), index);
    }

    // bt, bnt or jmp to a label (word form; the offset counts from the next
    // instruction).
    CodeAssembler& branch(int opcode, Label target) {
        bytes_.push_back(opcode_byte(opcode, false));
        fixups_.push_back(Fixup{.at = bytes_.size(), .target = target, .next = bytes_.size() + 2});
        append_u16le(bytes_, 0);
        return *this;
    }

    // call: a local procedure at a label, with `argc` arguments pushed
    // after their count (word offset, then the frame size in bytes).
    CodeAssembler& call(Label target, int argc) {
        bytes_.push_back(opcode_byte(op::call, false));
        fixups_.push_back(Fixup{.at = bytes_.size(), .target = target, .next = bytes_.size() + 3});
        append_u16le(bytes_, 0);
        bytes_.push_back(static_cast<std::uint8_t>(argc * 2));
        return *this;
    }

    // callk: kernel function `number` with `argc` arguments pushed after
    // their count.
    CodeAssembler& callk(int number, int argc) {
        bytes_.push_back(opcode_byte(op::callk, true));
        bytes_.push_back(static_cast<std::uint8_t>(number));
        bytes_.push_back(static_cast<std::uint8_t>(argc * 2));
        return *this;
    }

    // send / self / super: `words` stack words of selector, argument
    // count and arguments.
    CodeAssembler& send(int words) { return op_b(op::send, words * 2); }

    CodeAssembler& self(int words) { return op_b(op::self, words * 2); }

    CodeAssembler& super(int class_number, int words) {
        bytes_.push_back(opcode_byte(op::super, true));
        bytes_.push_back(static_cast<std::uint8_t>(class_number));
        bytes_.push_back(static_cast<std::uint8_t>(words * 2));
        return *this;
    }

    // lofsa or lofss: the address of an object of the script (by its index
    // in the ScriptBuilder), relative to the next instruction; resolved
    // when the script is built.
    CodeAssembler& object_address(int opcode, std::size_t object) {
        bytes_.push_back(opcode_byte(opcode, false));
        object_fixups_.push_back(ObjectFixup{.at = bytes_.size(), .object = object, .next = bytes_.size() + 2});
        append_u16le(bytes_, 0);
        return *this;
    }

    struct ObjectFixup {
        std::size_t at;
        std::size_t object;
        std::size_t next;
    };

    [[nodiscard]] const std::vector<ObjectFixup>& object_fixups() const { return object_fixups_; }

    // The code with every label resolved.
    [[nodiscard]] Bytes finish() const {
        Bytes out = bytes_;
        for (const Fixup& fixup : fixups_) {
            const std::size_t target = labels_[fixup.target].value_or(0);
            const auto relative =
                static_cast<std::uint16_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(fixup.next));
            out[fixup.at] = static_cast<std::uint8_t>(relative & 0xFFU);
            out[fixup.at + 1] = static_cast<std::uint8_t>(relative >> 8U);
        }
        return out;
    }

    // The offset of a bound label within the code.
    [[nodiscard]] std::size_t offset_of(Label label) const { return labels_[label].value_or(0); }

private:
    struct Fixup {
        std::size_t at;
        Label target;
        std::size_t next;
    };

    Bytes bytes_;
    std::vector<std::optional<std::size_t>> labels_;
    std::vector<Fixup> fixups_;
    std::vector<ObjectFixup> object_fixups_;
};

// An object or class of a script: its properties (selector and value, the
// first four being -species-, -super-, -info- and name) and its methods
// (selector and code label).
struct ObjectSpec {
    bool is_class = false;
    int species = 0;                                   // The class number (of the object's class, or the class itself).
    int super_class = -1;                              // -1: none (0xFFFF).
    std::vector<std::pair<int, int>> extra_properties; // After the four base properties.
    std::vector<std::pair<int, CodeAssembler::Label>> methods;
};

// One SCI0 script: objects and classes, one code block, exports and
// locals. Layout: exports, locals, objects in order, code, terminator.
class ScriptBuilder {
public:
    [[nodiscard]] CodeAssembler& code() { return code_; }

    // Adds an object; returns its index (for exports).
    std::size_t add(ObjectSpec object) {
        objects_.push_back(std::move(object));
        return objects_.size() - 1;
    }

    void set_locals(std::vector<std::uint16_t> locals) { locals_ = std::move(locals); }

    // Exports objects (by index) or code labels, in order.
    void export_object(std::size_t index) { exports_.push_back(Export{.object = index, .label = std::nullopt}); }

    void export_code(CodeAssembler::Label label) { exports_.push_back(Export{.object = std::nullopt, .label = label}); }

    [[nodiscard]] Bytes build() const {
        // Sizes first: exports and locals blocks, then each object block.
        const std::size_t exports_size = exports_.empty() ? 0 : 6 + (exports_.size() * 2);
        const std::size_t locals_size = locals_.empty() ? 0 : 4 + (locals_.size() * 2);
        std::vector<std::size_t> object_at; // The object's address: its first property.
        std::size_t offset = exports_size + locals_size;
        for (const ObjectSpec& object : objects_) {
            object_at.push_back(offset + 12);
            offset += object_size(object);
        }
        const std::size_t code_at = offset + 4; // After the code block's header.
        Bytes code_bytes = code_.finish();
        for (const CodeAssembler::ObjectFixup& fixup : code_.object_fixups()) {
            const auto relative = static_cast<std::uint16_t>(static_cast<std::int64_t>(object_at[fixup.object]) -
                                                             static_cast<std::int64_t>(code_at + fixup.next));
            code_bytes[fixup.at] = static_cast<std::uint8_t>(relative & 0xFFU);
            code_bytes[fixup.at + 1] = static_cast<std::uint8_t>(relative >> 8U);
        }

        Bytes out;
        if (!exports_.empty()) {
            append_u16le(out, 7);
            append_u16le(out, static_cast<std::uint32_t>(exports_size));
            append_u16le(out, static_cast<std::uint32_t>(exports_.size()));
            for (const Export& entry : exports_) {
                const std::size_t address =
                    entry.object ? object_at[*entry.object] : code_at + code_.offset_of(entry.label.value_or(0));
                append_u16le(out, static_cast<std::uint32_t>(address));
            }
        }
        if (!locals_.empty()) {
            append_u16le(out, 10);
            append_u16le(out, static_cast<std::uint32_t>(locals_size));
            for (const std::uint16_t value : locals_) {
                append_u16le(out, value);
            }
        }
        for (const ObjectSpec& object : objects_) {
            append_object(out, object, code_at);
        }
        append_u16le(out, 2);
        append_u16le(out, static_cast<std::uint32_t>(4 + code_bytes.size()));
        out.insert(out.end(), code_bytes.begin(), code_bytes.end());
        append_u16le(out, 0); // Terminator.
        return out;
    }

private:
    struct Export {
        std::optional<std::size_t> object;
        std::optional<CodeAssembler::Label> label;
    };

    static std::size_t property_count(const ObjectSpec& object) { return 4 + object.extra_properties.size(); }

    // Header (type, size, magic, local offset, function area, count: 12
    // bytes), properties, a class's property selectors, the method block
    // (count, selectors, 0, offsets).
    static std::size_t object_size(const ObjectSpec& object) {
        const std::size_t properties = property_count(object);
        return 12 + (properties * 2) + (object.is_class ? properties * 2 : 0) + 2 + (object.methods.size() * 4) + 2;
    }

    void append_object(Bytes& out, const ObjectSpec& object, std::size_t code_at) const {
        const std::size_t properties = property_count(object);
        const std::size_t start = out.size();
        append_u16le(out, object.is_class ? 6 : 1);
        append_u16le(out, static_cast<std::uint32_t>(object_size(object)));
        append_u16le(out, 0x1234); // Magic.
        append_u16le(out, 0);      // Local variables offset (unused in SCI0 loading).
        const std::size_t method_block =
            12 + (properties * 2) + (object.is_class ? properties * 2 : 0); // From the block start.
        // The function area counts from the first property, plus 2.
        append_u16le(out, static_cast<std::uint32_t>(method_block - 12 + 2));
        append_u16le(out, static_cast<std::uint32_t>(properties));
        append_u16le(out, static_cast<std::uint32_t>(object.species));
        append_u16le(out, static_cast<std::uint32_t>(object.super_class < 0 ? 0xFFFF : object.super_class));
        append_u16le(out, object.is_class ? 0x8000 : 0); // -info-: the class flag.
        append_u16le(out, 0);                            // name: none.
        for (const auto& [selector, value] : object.extra_properties) {
            (void)selector;
            append_u16le(out, static_cast<std::uint16_t>(value));
        }
        if (object.is_class) {
            // The selectors of the properties: the four base selectors are 0 to 3.
            for (int selector = 0; selector < 4; ++selector) {
                append_u16le(out, static_cast<std::uint32_t>(selector));
            }
            for (const auto& [selector, value] : object.extra_properties) {
                (void)value;
                append_u16le(out, static_cast<std::uint32_t>(selector));
            }
        }
        append_u16le(out, static_cast<std::uint32_t>(object.methods.size()));
        for (const auto& [selector, label] : object.methods) {
            (void)label;
            append_u16le(out, static_cast<std::uint32_t>(selector));
        }
        append_u16le(out, 0);
        for (const auto& [selector, label] : object.methods) {
            (void)selector;
            append_u16le(out, static_cast<std::uint32_t>(code_at + code_.offset_of(label)));
        }
        (void)start;
    }

    CodeAssembler code_;
    std::vector<ObjectSpec> objects_;
    std::vector<std::uint16_t> locals_;
    std::vector<Export> exports_;
};

// A font of 128 characters that all share one blank glyph of 4 x 6 pixels,
// for the graphics a machine opens at start (font 0).
inline Bytes blank_font() {
    constexpr int characters = 128;
    Bytes out;
    append_u16le(out, 0);
    append_u16le(out, characters);
    append_u16le(out, 6); // Line height.
    const auto glyph = static_cast<std::uint32_t>(6 + (characters * 2));
    for (int i = 0; i < characters; ++i) {
        append_u16le(out, glyph);
    }
    out.insert(out.end(), {4, 6, 0, 0, 0, 0, 0, 0}); // 4 x 6, one byte per row.
    return out;
}

// Vocab 997: the selector names, selector n being names[n].
inline Bytes selector_vocab(const std::vector<std::string>& names) {
    Bytes out;
    append_u16le(out, static_cast<std::uint32_t>(names.size() - 1));
    std::size_t offset = 2 + (names.size() * 2);
    for (const std::string& name : names) {
        append_u16le(out, static_cast<std::uint32_t>(offset));
        offset += 2 + name.size();
    }
    for (const std::string& name : names) {
        append_u16le(out, static_cast<std::uint32_t>(name.size()));
        out.insert(out.end(), name.begin(), name.end());
    }
    return out;
}

// Vocab 996: the class table, class n defined in script scripts[n].
inline Bytes class_vocab(const std::vector<int>& scripts) {
    Bytes out;
    for (const int script : scripts) {
        append_u16le(out, 0);
        append_u16le(out, static_cast<std::uint32_t>(script));
    }
    return out;
}

} // namespace sci::test
