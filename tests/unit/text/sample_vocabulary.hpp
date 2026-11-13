// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small SCI0 vocabulary for tests: vocab.000 words, vocab.901 suffixes
// and vocab.900 grammar branches (the parser tests and the home-made game).

#pragma once

#include "sci/text/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace sci::test::vocab {

inline constexpr int group_look = 0x10;
inline constexpr int group_take = 0x11;
inline constexpr int group_lock = 0x12;
inline constexpr int group_door = 0x20;
inline constexpr int group_key = 0x21;
inline constexpr int group_it = 0x30;

// The classes as stored in ResultWord, shifted left by 4.
inline constexpr int verb = sci::text::class_imperative_verb * 16;
inline constexpr int noun = sci::text::class_noun * 16;
inline constexpr int pronoun = sci::text::class_pronoun * 16;

// Bits [shift, shift + 8) of a non-negative value.
inline std::uint8_t byte_of(int value, unsigned shift) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(value) >> shift) & 0xFFU);
}

// One vocab.000 entry: the characters kept from the previous word, the
// rest of the word (the last character has bit 7 set), then the class and
// group in 12 bits each.
inline void add_word(std::vector<std::uint8_t>& vocab, std::uint8_t kept, std::string_view rest, int word_class,
                     int group) {
    vocab.push_back(kept);
    for (std::size_t i = 0; i < rest.size(); ++i) {
        const auto c = static_cast<std::uint8_t>(rest[i]);
        vocab.push_back(i + 1 == rest.size() ? static_cast<std::uint8_t>(c | 0x80U) : c);
    }
    vocab.push_back(byte_of(word_class, 4));
    vocab.push_back(static_cast<std::uint8_t>((static_cast<unsigned>(byte_of(word_class, 0)) << 4U) |
                                              (static_cast<unsigned>(byte_of(group, 8)) & 0x0FU)));
    vocab.push_back(byte_of(group, 0));
}

inline std::vector<std::uint8_t> sample_words() {
    std::vector<std::uint8_t> vocab(std::size_t{26} * 2, 0);
    vocab[0] = 1; // Any non-zero letter pointer: not the SCI01 format.
    add_word(vocab, 0, "door", noun, group_door);
    add_word(vocab, 0, "it", pronoun, group_it);
    add_word(vocab, 0, "key", noun, group_key);
    add_word(vocab, 0, "look", verb, group_look);
    add_word(vocab, 2, "ck", verb, group_lock); // "lo" + "ck".
    add_word(vocab, 0, "take", verb, group_take);
    return vocab;
}

// vocab.901: '*' alt NUL class '*' word NUL mask, repeated.
inline void add_suffix(std::vector<std::uint8_t>& suffixes, std::string_view alt, int result_class,
                       std::string_view word, int mask) {
    suffixes.insert(suffixes.end(), alt.begin(), alt.end());
    suffixes.push_back(0);
    suffixes.push_back(byte_of(result_class, 8));
    suffixes.push_back(byte_of(result_class, 0));
    suffixes.push_back('*');
    suffixes.insert(suffixes.end(), word.begin(), word.end());
    suffixes.push_back(0);
    suffixes.push_back(byte_of(mask, 8));
    suffixes.push_back(byte_of(mask, 0));
    suffixes.push_back('*');
}

inline std::vector<std::uint8_t> sample_suffixes() {
    std::vector<std::uint8_t> suffixes{'*'};
    add_suffix(suffixes, "s", noun, "", noun);
    add_suffix(suffixes, "ing", verb, "", verb);
    add_suffix(suffixes, "ies", noun, "y", noun); // Matches nothing here.
    return suffixes;
}

// vocab.900: 20-byte branches, an id and up to 9 little-endian words
// (pairs of type and value).
inline void add_branch(std::vector<std::uint8_t>& branches, int id, std::initializer_list<int> data) {
    const auto put = [&](int value) {
        branches.push_back(byte_of(value, 0));
        branches.push_back(byte_of(value, 8));
    };
    put(id);
    std::size_t count = 0;
    for (const int value : data) {
        put(value);
        ++count;
    }
    for (; count < 9; ++count) {
        put(0);
    }
}

// A sentence is a verb, optionally followed by a noun:
//   0x12f -> (141 149 <0x149>) (142 14a <0x14a>) | (141 149 <0x149>)
//   0x149 -> (141 14f <0x14f>);  0x14a -> (141 14f <0x14f>)
//   0x14f -> a verb | a noun
// which gives the trees Said builds: (141 13f (141 149 (141 14f look))
// (142 14a (141 14f door))).
inline std::vector<std::uint8_t> sample_branches() {
    std::vector<std::uint8_t> branches;
    add_branch(branches, 0x13f, {0, 0x12f});
    add_branch(branches, 0x12f, {0x141, 0x149, 0x142, 0x14a});
    add_branch(branches, 0x12f, {0x141, 0x149});
    add_branch(branches, 0x149, {0x141, 0x14f});
    add_branch(branches, 0x14a, {0x141, 0x14f});
    add_branch(branches, 0x14f, {0x146, verb});
    add_branch(branches, 0x14f, {0x146, noun});
    add_branch(branches, 0, {}); // The empty rule some vocabularies end with.
    return branches;
}

} // namespace sci::test::vocab
