// SPDX-License-Identifier: GPL-3.0-or-later
//
// Golden outputs of all resources (review/BACKLOG_V2.md B-18, stage 5 step
// 0.3, scope of Q-02): one SHA-256 per image, local only (needs the game).
//
//   sci-golden-resources <game> <file> [--hints <directory>] [--check] [--only pictures|sprites]
//
// Lines, in this order (hashes only, nothing of the game itself):
//   pic <n> visual1|priority1|control1 <hash>   the 1x maps of path 1
//   pic <n> original1 <hash>                     the original-mode frame at 1x
//   pic <n> hd<N> <hash>                         HD image, N = 1 ... 16
//   pic <n> hdpri<N> <hash>                      HD priority map, N = 1 ... 16
//   view <n> <loop> <cel> 1x|hd<N> <hash>        every cel, N = 1, 2, 5, 10
//   font <n> <glyph> 1x|hd<N> <hash>            every glyph, N = 1, 2, 5, 10
//   cursor <n> 1x|hd<N> <hash>                   every cursor, N = 1, 2, 5, 10
// The HD scenes are analysed as the program does (sci::platform::analyse,
// both planes), with the repository's hints (--hints hints). The pic hd1,
// hd2, hd5, hd10 and original1 lines must equal tests/golden/pictures.txt.
// --check compares with the file and prints every difference by resource,
// N and layer; exits with 1 on any.

#include "game_folder.hpp"
#include "layers.hpp"
#include "picture.hpp"
#include "sci/platform/analysis_cache.hpp"
#include "sci/platform/thread_executor.hpp"
#include "sci/render/original.hpp"
#include "sci/render/rasterize.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <print>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using sci::resource::ResourceId;
using sci::resource::ResourceManager;
using sci::resource::ResourceType;
namespace golden = sci::tools::golden;

constexpr int max_picture_scale = 16;
constexpr std::array sprite_scales{1, 2, 5, 10};

std::string picture_lines(const ResourceManager& manager, std::uint16_t number,
                          const std::optional<std::filesystem::path>& hints, sci::core::Executor& executor) {
    const std::array numbers{number};
    const auto drawn = sci::tools::draw_pictures(manager, numbers);
    if (!drawn) {
        return std::format("pic {} error\n", number);
    }
    std::string lines;
    lines += std::format("pic {} visual1 {}\n", number, golden::hash(drawn->maps.visual));
    lines += std::format("pic {} priority1 {}\n", number, golden::hash(drawn->maps.priority));
    lines += std::format("pic {} control1 {}\n", number, golden::hash(drawn->maps.control));
    lines += std::format("pic {} original1 {}\n", number, golden::hash(sci::render::render_original(drawn->maps, 1)));
    const auto visual = sci::platform::analyse(manager, hints, sci::platform::SceneVariant{.picture = number});
    const auto priority = sci::platform::analyse(
        manager, hints, sci::platform::SceneVariant{.picture = number, .plane = sci::vector::Plane::Priority});
    if (!visual || !priority) {
        return lines + std::format("pic {} scene error\n", number);
    }
    for (int scale = 1; scale <= max_picture_scale; ++scale) {
        lines += std::format("pic {} hd{} {}\n", number, scale,
                             golden::hash(sci::render::render_hd(*visual, scale, executor).pixels));
    }
    for (int scale = 1; scale <= max_picture_scale; ++scale) {
        lines += std::format("pic {} hdpri{} {}\n", number, scale,
                             golden::hash(sci::render::render_priority(*priority, scale, executor)));
    }
    return lines;
}

template <typename Decode>
auto load(const ResourceManager& manager, ResourceType type, std::uint16_t number, Decode decode)
    -> std::optional<typename decltype(decode(std::span<const std::uint8_t>{}))::value_type> {
    const auto resource = manager.load(ResourceId{.type = type, .number = number});
    if (!resource) {
        return std::nullopt;
    }
    auto decoded = decode(resource->data);
    if (!decoded) {
        return std::nullopt;
    }
    return std::move(*decoded);
}

std::string sprite_lines(const ResourceManager& manager) {
    std::string lines;
    for (const std::uint16_t number : sci::tools::numbers_of(manager, ResourceType::View)) {
        const auto view = load(manager, ResourceType::View, number, sci::view::decode_view);
        if (!view) {
            lines += std::format("view {} error\n", number);
            continue;
        }
        for (std::size_t loop = 0; loop < view->loops.size(); ++loop) {
            for (std::size_t cel = 0; cel < view->loops[loop].cels.size(); ++cel) {
                const sci::view::Cel& c = view->loops[loop].cels[cel];
                lines += std::format("view {} {} {} 1x {}\n", number, loop, cel, golden::cel_1x(c));
                for (const int scale : sprite_scales) {
                    lines += std::format("view {} {} {} hd{} {}\n", number, loop, cel, scale, golden::cel_hd(c, scale));
                }
            }
        }
    }
    for (const std::uint16_t number : sci::tools::numbers_of(manager, ResourceType::Font)) {
        const auto font = load(manager, ResourceType::Font, number, sci::text::decode_font);
        if (!font) {
            lines += std::format("font {} error\n", number);
            continue;
        }
        for (std::size_t glyph = 0; glyph < font->glyphs.size(); ++glyph) {
            const sci::text::Glyph& g = font->glyphs[glyph];
            lines += std::format("font {} {} 1x {}\n", number, glyph, golden::glyph_1x(g));
            for (const int scale : sprite_scales) {
                lines += std::format("font {} {} hd{} {}\n", number, glyph, scale, golden::glyph_hd(g, scale));
            }
        }
    }
    for (const std::uint16_t number : sci::tools::numbers_of(manager, ResourceType::Cursor)) {
        const auto cursor = load(manager, ResourceType::Cursor, number, sci::view::decode_cursor);
        if (!cursor) {
            lines += std::format("cursor {} error\n", number);
            continue;
        }
        lines += std::format("cursor {} 1x {}\n", number, golden::cursor_1x(*cursor));
        for (const int scale : sprite_scales) {
            lines += std::format("cursor {} hd{} {}\n", number, scale, golden::cursor_hd(*cursor, scale));
        }
    }
    return lines;
}

struct Options {
    bool usage = false;
    bool check = false;
    bool pictures = true;
    bool sprites = true;
    std::optional<std::filesystem::path> hints;
};

Options parse_options(std::span<char*> args) {
    Options options;
    options.usage = args.size() < 3;
    for (std::size_t i = 3; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--check") {
            options.check = true;
        } else if (arg == "--hints" && i + 1 < args.size()) {
            options.hints = std::filesystem::path(args[++i]);
        } else if (arg == "--only" && i + 1 < args.size()) {
            const std::string_view what = args[++i];
            options.pictures = what == "pictures";
            options.sprites = what == "sprites";
            options.usage = options.usage || (!options.pictures && !options.sprites);
        } else {
            options.usage = true;
        }
    }
    return options;
}

// The differences, each named by the line's fields before the hash.
int compare(const std::string& expected, const std::string& got) {
    std::istringstream want(expected);
    std::istringstream have(got);
    int differences = 0;
    while (want || have) {
        std::string a;
        std::string b;
        const bool more_wanted = static_cast<bool>(std::getline(want, a));
        const bool more_got = static_cast<bool>(std::getline(have, b));
        if ((more_wanted || more_got) && a != b) {
            const auto key = [](const std::string& line) { return line.substr(0, line.rfind(' ')); };
            if (more_wanted && more_got && key(a) == key(b)) {
                std::println("differs: {}", key(a));
            } else {
                std::println("expected: {}\n     got: {}", a, b);
            }
            ++differences;
        }
    }
    return differences;
}

} // namespace

int main(int argc, char** argv) {
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));
    const Options options = parse_options(args);
    if (options.usage) {
        std::println(stderr, "usage: sci-golden-resources <game> <file> [--hints <directory>] [--check] "
                             "[--only pictures|sprites]");
        return 2;
    }
    sci::tools::install_stderr_log();
    const auto manager = sci::tools::open_game(args[1]);
    if (!manager) {
        return 1;
    }
    sci::platform::ThreadExecutor executor;
    std::string lines;
    if (options.pictures) {
        for (const std::uint16_t number : sci::tools::numbers_of(*manager, ResourceType::Pic)) {
            lines += picture_lines(*manager, number, options.hints, executor);
        }
    }
    if (options.sprites) {
        lines += sprite_lines(*manager);
    }
    const std::filesystem::path path = args[2];
    if (!options.check) {
        std::ofstream out(path);
        out << lines;
        return out ? 0 : 1;
    }
    std::ifstream in(path);
    const std::string expected{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const int differences = compare(expected, lines);
    std::println("{}", differences == 0 ? "all resource hashes match" : std::format("{} differences", differences));
    return differences == 0 ? 0 : 1;
}
