#pragma once
#include <cctype>
#include <optional>
#include <string_view>

// L-115: an entity's rest pose from its setup animations, without the entity.
// Only Molang that needs nothing from the entity is evaluated: numbers,
// `this` (the bone's own rest value: "90 - this" turns a bone to 90 from
// wherever it rests), + - * / and parentheses.
// Anything else (queries, variables, conditions) is not a rest pose.
namespace lamium::schematic {
class ConstantMolang {
public:
    ConstantMolang(std::string_view text, float self) : text(text), self(self) {}
    std::optional<float> evaluate() {
        auto value = sum();
        skip();
        if (!value || at < text.size()) return std::nullopt;
        return value;
    }

private:
    std::string_view text;
    float self = 0;
    size_t at = 0;
    void skip() { while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at; }
    bool take(char c) {
        skip();
        if (at < text.size() && text[at] == c) { ++at; return true; }
        return false;
    }
    std::optional<float> sum() {
        auto value = product();
        while (value) {
            if (take('+')) { auto next = product(); if (!next) return std::nullopt; *value += *next; }
            else if (take('-')) { auto next = product(); if (!next) return std::nullopt; *value -= *next; }
            else break;
        }
        return value;
    }
    std::optional<float> product() {
        auto value = unary();
        while (value) {
            if (take('*')) { auto next = unary(); if (!next) return std::nullopt; *value *= *next; }
            else if (take('/')) { auto next = unary(); if (!next || *next == 0) return std::nullopt; *value /= *next; }
            else break;
        }
        return value;
    }
    std::optional<float> unary() {
        if (take('-')) { auto value = unary(); if (value) *value = -*value; return value; }
        if (take('+')) return unary();
        if (take('(')) {
            auto value = sum();
            if (!value || !take(')')) return std::nullopt;
            return value;
        }
        skip();
        if (text.substr(at, 4) == "this" && (at + 4 == text.size() || (!std::isalnum(static_cast<unsigned char>(text[at + 4])) && text[at + 4] != '_' && text[at + 4] != '.'))) {
            at += 4;
            return self;
        }
        size_t start = at;
        while (at < text.size() && (std::isdigit(static_cast<unsigned char>(text[at])) || text[at] == '.')) ++at;
        if (start == at) return std::nullopt;
        float value = 0, scale = 0;
        for (size_t i = start; i < at; ++i) {
            if (text[i] == '.') { if (scale) return std::nullopt; scale = 1; continue; }
            if (scale) { scale /= 10; value += (text[i] - '0') * scale; }
            else value = value * 10 + (text[i] - '0');
        }
        if (at < text.size() && (text[at] == 'f' || text[at] == 'F')) ++at;
        return value;
    }
};
inline std::optional<float> constantMolang(std::string_view text, float self = 0) { return ConstantMolang(text, self).evaluate(); }
} // namespace lamium::schematic
