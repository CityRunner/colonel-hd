// SPDX-License-Identifier: GPL-3.0-or-later

#include "sample_vocabulary.hpp"
#include "sci/text/parser.hpp"
#include "sci_test.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using sci::test::vocab::add_branch;
using sci::test::vocab::add_word;
using sci::test::vocab::group_door;
using sci::test::vocab::group_key;
using sci::test::vocab::group_lock;
using sci::test::vocab::group_look;
using sci::test::vocab::group_take;
using sci::test::vocab::noun;
using sci::test::vocab::sample_branches;
using sci::test::vocab::sample_suffixes;
using sci::test::vocab::sample_words;
using sci::test::vocab::verb;
using sci::text::ParseNode;
using sci::text::ResultWord;
using sci::text::Sentence;
using sci::text::Vocabulary;

namespace {

Vocabulary sample_vocabulary(sci::text::ParserOptions options = {}) {
    const std::vector<std::uint8_t> words = sample_words();
    const std::vector<std::uint8_t> suffixes = sample_suffixes();
    const std::vector<std::uint8_t> branches = sample_branches();
    return {words, suffixes, branches, options};
}

std::optional<Sentence> tokenize(const Vocabulary& vocabulary, std::string_view text) {
    std::string error;
    return vocabulary.tokenize(text, error);
}

// Tokenizes and parses; the parse tree, or nothing.
std::optional<std::vector<ParseNode>> parse(Vocabulary& vocabulary, std::string_view text) {
    std::optional<Sentence> sentence = tokenize(vocabulary, text);
    if (!sentence) {
        return std::nullopt;
    }
    vocabulary.replace_pronouns(*sentence);
    if (!vocabulary.parse(*sentence)) {
        return std::nullopt;
    }
    vocabulary.store_pronoun_reference();
    const auto nodes = vocabulary.nodes();
    return std::vector<ParseNode>{nodes.begin(), nodes.end()};
}

int said(std::initializer_list<std::uint8_t> spec, std::span<const ParseNode> tree) {
    const std::vector<std::uint8_t> bytes(spec);
    return sci::text::said(bytes, tree);
}

// Said spec bytes: a word group, and the operators.
constexpr std::uint8_t comma = 0xF0;
constexpr std::uint8_t slash = 0xF2;
constexpr std::uint8_t bracket_open = 0xF5;
constexpr std::uint8_t bracket_close = 0xF6;
constexpr std::uint8_t greater = 0xF9;
constexpr std::uint8_t end = 0xFF;

} // namespace

SCI_TEST(parser_words_are_found_with_shared_prefixes) {
    const Vocabulary vocabulary = sample_vocabulary();
    SCI_CHECK(vocabulary.warnings().empty());
    SCI_CHECK(vocabulary.lookup_word("look") == sci::text::WordMeanings{ResultWord{verb, group_look}});
    SCI_CHECK(vocabulary.lookup_word("lock") == sci::text::WordMeanings{ResultWord{verb, group_lock}});
    SCI_CHECK(vocabulary.lookup_word("door") == sci::text::WordMeanings{ResultWord{noun, group_door}});
    SCI_CHECK(vocabulary.lookup_word("lo-ok") == sci::text::WordMeanings{ResultWord{verb, group_look}});
    SCI_CHECK(vocabulary.lookup_word("xyzzy").empty());
}

SCI_TEST(parser_suffixes_map_to_the_stem_with_the_class_mask) {
    const Vocabulary vocabulary = sample_vocabulary();
    SCI_CHECK(vocabulary.suffixes().size() == 3);
    SCI_CHECK(vocabulary.lookup_word("doors") == sci::text::WordMeanings{ResultWord{noun, group_door}});
    SCI_CHECK(vocabulary.lookup_word("looking") == sci::text::WordMeanings{ResultWord{verb, group_look}});
    // "-ing" applies to verbs only, "-s" to nouns only.
    SCI_CHECK(vocabulary.lookup_word("dooring").empty());
    SCI_CHECK(vocabulary.lookup_word("looks").empty());
}

SCI_TEST(parser_numbers_are_words_except_in_early_sci0) {
    const Vocabulary vocabulary = sample_vocabulary();
    SCI_CHECK(vocabulary.lookup_word("42") == sci::text::WordMeanings{ResultWord{noun, sci::text::magic_number_group}});
    SCI_CHECK(vocabulary.lookup_word("4x").empty());
    const Vocabulary early = sample_vocabulary({.sci0 = true, .sci0_early = true});
    SCI_CHECK(early.lookup_word("42").empty());
}

SCI_TEST(parser_tokenize_lowers_and_skips_punctuation) {
    const Vocabulary vocabulary = sample_vocabulary();
    std::string error;
    const auto sentence = vocabulary.tokenize("LOOK,  d'oor!", error);
    SCI_CHECK(sentence && error.empty());
    if (sentence) {
        SCI_CHECK(sentence->size() == 2);
        SCI_CHECK(sentence->size() == 2 && (*sentence)[1] == sci::text::WordMeanings{ResultWord{noun, group_door}});
    }
    const auto unknown = vocabulary.tokenize("look Xyzzy", error);
    SCI_CHECK(!unknown && error == "xyzzy");
    const auto empty = vocabulary.tokenize(" ?! ", error);
    SCI_CHECK(empty && empty->empty() && error.empty());
}

// What the milestone 6a corpus showed both engines do with the game's
// vocabulary: a word stored with a dash or an apostrophe is never found
// (the lookup removes dashes, the tokenizer drops apostrophes), and bytes
// above 127 are letters, with code page 437's accented capitals lowered.
SCI_TEST(parser_stored_dashes_never_match_and_high_bytes_are_letters) {
    std::vector<std::uint8_t> words = sample_words();
    add_word(words, 0, "good-looking", noun, 0x40);
    add_word(words, 0, "o'neil", noun, 0x41);
    const Vocabulary vocabulary(words, std::nullopt, std::nullopt);
    SCI_CHECK(vocabulary.words().size() == 8);
    SCI_CHECK(vocabulary.lookup_word("good-looking").empty());
    SCI_CHECK(vocabulary.lookup_word("goodlooking").empty());
    std::string error;
    SCI_CHECK(!vocabulary.tokenize("look o'neil", error) && error == "oneil");
    SCI_CHECK(!vocabulary.tokenize("look Good-Looking", error) && error == "good-looking");
    SCI_CHECK(!vocabulary.tokenize("LOOK \x8e\x80\x99 door", error) && error == "\x84\x87\x94");
    SCI_CHECK(!vocabulary.tokenize("look caf\xc3\xa9", error) && error == "caf\xc3\xa9");
    // A tab is neither a letter nor a separator: it is dropped.
    SCI_CHECK(!vocabulary.tokenize("look\tdoor", error) && error == "lookdoor");
}

SCI_TEST(parser_synonyms_replace_groups) {
    const Vocabulary vocabulary = sample_vocabulary();
    auto sentence = tokenize(vocabulary, "take key");
    SCI_CHECK(sentence.has_value());
    if (sentence) {
        const std::vector<sci::text::Synonym> synonyms{{.replaceant = group_key, .replacement = group_door}};
        Vocabulary::synonymize(*sentence, synonyms);
        SCI_CHECK((*sentence)[0][0].group == group_take && (*sentence)[1][0].group == group_door);
    }
}

SCI_TEST(parser_builds_the_tree_said_expects) {
    Vocabulary vocabulary = sample_vocabulary();
    SCI_CHECK(vocabulary.has_rules() && vocabulary.branches().size() == 7);
    const auto tree = parse(vocabulary, "look door");
    SCI_CHECK(tree.has_value());
    if (tree) {
        const std::vector<ParseNode>& t = *tree;
        // (141 13f ...)
        SCI_CHECK(t[0].type == sci::text::NodeType::Branch && t[1].value == 0x141 && t[3].value == 0x13f);
        SCI_CHECK(said({0, group_look, slash, 0, group_door, end}, t) == sci::text::said_full_match);
    }
    SCI_CHECK(parse(vocabulary, "look").has_value());
    SCI_CHECK(!parse(vocabulary, "look door door").has_value());
    // As in ScummVM, no words leave the start rules as the result; kParse
    // never parses an empty sentence.
    SCI_CHECK(vocabulary.parse(Sentence{}));
}

SCI_TEST(parser_pronouns_refer_to_the_last_object) {
    Vocabulary vocabulary = sample_vocabulary();
    SCI_CHECK(parse(vocabulary, "look door").has_value());
    auto sentence = tokenize(vocabulary, "take it");
    SCI_CHECK(sentence.has_value());
    if (sentence) {
        vocabulary.replace_pronouns(*sentence);
        SCI_CHECK((*sentence)[1] == sci::text::WordMeanings{ResultWord{noun, group_door}});
    }
    const auto tree = parse(vocabulary, "take it");
    SCI_CHECK(tree && said({0, group_take, slash, 0, group_door, end}, *tree) == sci::text::said_full_match);
}

SCI_TEST(parser_said_matches_words_and_operators) {
    Vocabulary vocabulary = sample_vocabulary();
    const auto look = parse(vocabulary, "look");
    const auto look_door = parse(vocabulary, "look door");
    const auto take_door = parse(vocabulary, "take door");
    SCI_CHECK(look && look_door && take_door);
    if (!look || !look_door || !take_door) {
        return;
    }
    using sci::text::said_full_match;
    using sci::text::said_no_match;
    using sci::text::said_partial_match;
    SCI_CHECK(said({0, group_look, end}, *look) == said_full_match);
    SCI_CHECK(said({0, group_take, end}, *look) == said_no_match);
    SCI_CHECK(said({0, group_look, comma, 0, group_take, end}, *look) == said_full_match);
    SCI_CHECK(said({0, group_look, slash, 0, group_door, end}, *look) == said_no_match);
    SCI_CHECK(said({0, group_look, slash, 0, group_key, end}, *look_door) == said_no_match);
    SCI_CHECK(said({0, group_look, slash, 0, group_door, end}, *take_door) == said_no_match);
    SCI_CHECK(said({slash, 0, group_door, end}, *take_door) == said_full_match);
    // An optional object: "look[/door]".
    SCI_CHECK(said({0, group_look, bracket_open, slash, 0, group_door, bracket_close, end}, *look) == said_full_match);
    SCI_CHECK(said({0, group_look, bracket_open, slash, 0, group_door, bracket_close, end}, *look_door) ==
              said_full_match);
    // Any word: 0x0fff.
    SCI_CHECK(said({0x0F, 0xFF, slash, 0, group_door, end}, *take_door) == said_full_match);
    // '>' leaves the event unclaimed.
    SCI_CHECK(said({0, group_look, greater, end}, *look_door) == said_partial_match);
}

SCI_TEST(parser_said_rejects_broken_specs) {
    Vocabulary vocabulary = sample_vocabulary();
    const auto look = parse(vocabulary, "look");
    SCI_CHECK(look.has_value());
    if (!look) {
        return;
    }
    std::vector<std::string> warnings;
    const std::vector<std::uint8_t> broken{bracket_close, 0, group_look, end};
    SCI_CHECK(sci::text::said(broken, *look, &warnings) == sci::text::said_no_match);
    SCI_CHECK(warnings.size() == 1);
    // A missing terminator reads as one.
    const std::vector<std::uint8_t> unterminated{0, group_look};
    SCI_CHECK(sci::text::said(unterminated, *look) == sci::text::said_full_match);
    const std::vector<std::uint8_t> too_long(300, comma);
    SCI_CHECK(sci::text::said(too_long, *look) == sci::text::said_no_match);
}

SCI_TEST(parser_broken_vocabularies_disable_the_parser) {
    const std::vector<std::uint8_t> tiny(10, 1);
    const Vocabulary small(tiny, std::nullopt, std::nullopt);
    SCI_CHECK(!small.has_rules() && small.warnings().size() == 1 && small.lookup_word("look").empty());

    std::vector<std::uint8_t> truncated = sample_words();
    truncated.resize(truncated.size() - 4); // Cuts into the last word.
    const std::vector<std::uint8_t> branches = sample_branches();
    const Vocabulary cut(truncated, std::nullopt, branches);
    SCI_CHECK(!cut.has_rules() && !cut.warnings().empty());

    const Vocabulary no_branches(sample_words(), sample_suffixes(), std::nullopt);
    SCI_CHECK(!no_branches.has_rules() && no_branches.lookup_word("doors").size() == 1);

    // 0x14f -> (141 14f <0x14f>) | a verb nests forever: ScummVM would
    // expand it until memory runs out.
    std::vector<std::uint8_t> recursive;
    add_branch(recursive, 0x13f, {0, 0x14f});
    add_branch(recursive, 0x14f, {0x141, 0x14f, 0x146, verb});
    add_branch(recursive, 0x14f, {0x146, verb});
    const Vocabulary endless(sample_words(), std::nullopt, recursive);
    SCI_CHECK(!endless.has_rules() && endless.warnings().size() == 1);

    const std::vector<std::uint8_t> zeros(std::size_t{26} * 2, 0);
    const Vocabulary disguised(zeros, std::nullopt, std::nullopt);
    SCI_CHECK(!disguised.warnings().empty());
}
