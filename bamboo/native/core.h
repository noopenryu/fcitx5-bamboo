/* Copyright (C) 2018 Luong Thanh Lam. MIT; see LICENSE.core. */
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace bamboo {
using Rune = char32_t;
using Text = std::u32string;
using Mode = unsigned;
enum Modes : unsigned {
    VietnameseMode = 1,
    EnglishMode = 2,
    ToneLess = 4,
    MarkLess = 8,
    LowerCase = 16,
    FullText = 32,
    PunctuationMode = 64,
    InReverseOrder = 128
};
enum Flags : unsigned {
    EfreeToneMarking = 1,
    EstdToneStyle = 2,
    EautoCorrectEnabled = 4,
    EstdFlags = 7
};
enum EffectType {
    Appending,
    MarkTransformation,
    ToneTransformation,
    Replacing
};
enum Mark { MarkNone, MarkHat, MarkBreve, MarkHorn, MarkDash, MarkRaw };
enum Tone { ToneNone, ToneGrave, ToneAcute, ToneHook, ToneTilde, ToneDot };
struct Rule {
    Rune Key = 0;
    uint8_t Effect = 0;
    EffectType Type = Appending;
    Rune EffectOn = 0;
    Rune Result = 0;
    std::vector<Rule> AppendedRules;
};
struct InputMethod {
    std::string Name;
    std::vector<Rule> Rules;
    Text SuperKeys, ToneKeys, AppendingKeys, Keys;
};
using Definition = std::map<std::string, std::string>;
using Definitions = std::map<std::string, Definition>;
using Charsets = std::map<std::string, std::map<Rune, std::string>>;
const Definitions &inputMethodDefinitions();
const Charsets &charsetDefinitions();
std::vector<std::string> inputMethodNames();
std::vector<std::string> charsetNames();
Text decode(const std::string &s);
std::string encode(const Text &s);
std::string encodeCharset(const std::string &name, const std::string &s);
Rune lower(Rune c);
Rune upper(Rune c);
bool isUpper(Rune c);
bool isLower(Rune c);
std::string lowerString(const std::string &s);
std::string upperString(const std::string &s);
bool IsVowel(Rune c);
bool IsAlpha(Rune c);
bool IsSpace(Rune c);
bool IsWordBreakSymbol(Rune c);
bool IsVietnameseRune(Rune c);
bool HasAnyVietnameseRune(const std::string &s);
bool HasAnyVietnameseVower(const std::string &s);
int FindVowelPosition(Rune c);
int FindMarkPosition(Rune c);
Tone FindToneFromChar(Rune c);
Rune AddToneToChar(Rune c, unsigned tone);
Rune AddMarkToTonelessChar(Rune c, unsigned mark);
Rune AddMarkToChar(Rune c, unsigned mark);
bool inKeyList(const Text &keys, Rune c);
std::vector<Rule> ParseRules(Rune key, const std::string &line);
InputMethod ParseInputMethod(const Definitions &definitions,
                             const std::string &name);

struct Transformation;
using Trans = std::shared_ptr<Transformation>;
using Composition = std::vector<Trans>;
struct Transformation {
    Rule rule;
    Trans Target;
    bool IsUpperCase = false;
};

class Core {
public:
    Core(InputMethod method, unsigned flags = EstdFlags);
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) noexcept = default;
    Core &operator=(Core &&) noexcept = default;
    const InputMethod &GetInputMethod() const { return inputMethod_; }
    void SetFlag(unsigned flags) { flags_ = flags; }
    void ProcessKey(Rune key, Mode mode);
    void ProcessString(const std::string &s, Mode mode);
    std::string GetProcessedString(Mode mode) const;
    bool IsValid(bool complete) const;
    bool CanProcessKey(Rune key) const;
    void RemoveLastChar(bool refresh);
    void RestoreLastWord(bool vietnamese);
    void Reset() { composition_.clear(); }

private:
    std::vector<Rule> applicableRules(Rune key) const;
    Composition newComposition(Composition composition, Rune key, bool upper);
    Composition refreshTone(Composition composition);
    Composition composition_;
    InputMethod inputMethod_;
    unsigned flags_;
};
} // namespace bamboo
