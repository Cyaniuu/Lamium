#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The Materials tab's amounts (L-93 screen review, 2026-10-08). Pure.
namespace lamium::schematic {
// An amount as a player stores it: chests of 27 stacks, stacks, items. Items
// that stack to less than 64 use their own stack size.
struct Amount {
    std::uint64_t chests = 0, stacks = 0, items = 0;
    bool operator==(Amount const&) const = default;
};
inline constexpr int chestSlots = 27;
inline Amount amountOf(std::uint64_t count, int maxStack) {
    std::uint64_t stack = maxStack > 0 ? static_cast<std::uint64_t>(maxStack) : 64;
    std::uint64_t stacks = count / stack;
    return {stacks / chestSlots, stacks % chestSlots, count % stack};
}
// ResourceCalculator takes the wanted items in the URL fragment, named like
// "oakplanks" (checked 2026-10-08). Lamium only links to the site; it never
// reads its results or uses its data (GPL-3.0; see SCHEMATIC.md).
inline std::string calculatorName(std::string_view item) {
    if (item.starts_with("minecraft:")) item.remove_prefix(10);
    // Bedrock ids that the site names differently.
    static constexpr std::pair<std::string_view, std::string_view> renamed[]{
        {"planks", "oakplanks"}, {"log", "oaklog"}, {"stonebrick", "stonebricks"}, {"wool", "whitewool"},
        {"concrete", "whiteconcrete"}, {"carpet", "whitecarpet"}, {"stained_glass", "whitestainedglass"}};
    for (auto [bedrock, site] : renamed) if (item == bedrock) return std::string(site);
    std::string out;
    for (char c : item) if (c != '_') out += c;
    return out;
}
inline std::string calculatorUrl(std::vector<std::pair<std::string, std::uint64_t>> const& items) {
    std::string url = "https://resourcecalculator.com/minecraft/#";
    bool first = true;
    for (auto const& [item, count] : items) {
        if (item.empty() || !count) continue;
        if (!first) url += '&';
        url += calculatorName(item) + "=" + std::to_string(count);
        first = false;
    }
    return url;
}
}
