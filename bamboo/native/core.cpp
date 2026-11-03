/* Copyright (C) 2018 Luong Thanh Lam. MIT; see LICENSE.core. */
#include "core.h"
#include <algorithm>
#include <array>
#include <sstream>
#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include <utility>

namespace bamboo {
Text decode(const std::string &s) {
    Text result;
    int32_t i = 0;
    while (i < static_cast<int32_t>(s.size())) {
        UChar32 c;
        U8_NEXT(s.data(), i, static_cast<int32_t>(s.size()), c);
        result.push_back(c < 0 ? 0xfffd : c);
    }
    return result;
}
std::string encode(const Text &s) {
    std::string result;
    for (auto c : s) {
        if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            c = 0xfffd;
        char buffer[4];
        int32_t i = 0;
        U8_APPEND_UNSAFE(buffer, i, c);
        result.append(buffer, i);
    }
    return result;
}
Rune lower(Rune c) { return u_tolower(c); }
Rune upper(Rune c) { return u_toupper(c); }
bool isUpper(Rune c) { return u_isupper(c); }
bool isLower(Rune c) { return u_islower(c); }
std::string lowerString(const std::string &s) {
    auto text = decode(s);
    for (auto &c : text)
        c = lower(c);
    return encode(text);
}
std::string upperString(const std::string &s) {
    auto text = decode(s);
    for (auto &c : text)
        c = upper(c);
    return encode(text);
}
static const Text vowels =
    U"aàáảãạăằắẳẵặâầấẩẫậeèéẻẽẹêềếểễệiìíỉĩịoòóỏõọôồốổỗộơờớởỡợuùúủũụưừứửữựyỳýỷỹỵ";
static const std::map<Rune, Text> marks = {
    {U'a', U"aâă__"}, {U'â', U"aâă__"}, {U'ă', U"aâă__"}, {U'e', U"eê___"},
    {U'ê', U"eê___"}, {U'o', U"oô_ơ_"}, {U'ô', U"oô_ơ_"}, {U'ơ', U"oô_ơ_"},
    {U'u', U"u__ư_"}, {U'ư', U"u__ư_"}, {U'd', U"d___đ"}, {U'đ', U"d___đ"}};
bool inKeyList(const Text &keys, Rune c) { return keys.find(c) != Text::npos; }
int FindVowelPosition(Rune c) {
    auto i = vowels.find(c);
    return i == Text::npos ? -1 : static_cast<int>(i);
}
bool IsVowel(Rune c) { return FindVowelPosition(c) >= 0; }
int FindMarkPosition(Rune c) {
    auto i = marks.find(c);
    return i == marks.end() ? -1 : static_cast<int>(i->second.find(c));
}
Tone FindToneFromChar(Rune c) {
    auto i = FindVowelPosition(c);
    return i < 0 ? ToneNone : static_cast<Tone>(i % 6);
}
Rune AddToneToChar(Rune c, unsigned tone) {
    auto i = FindVowelPosition(c);
    return i < 0 || tone >= 6 ? c : vowels[i - i % 6 + tone];
}
Rune AddMarkToTonelessChar(Rune c, unsigned mark) {
    auto it = marks.find(c);
    if (it != marks.end() && mark < it->second.size() &&
        it->second[mark] != U'_')
        return it->second[mark];
    return c;
}
Rune AddMarkToChar(Rune c, unsigned mark) {
    return AddToneToChar(AddMarkToTonelessChar(AddToneToChar(c, 0), mark),
                         FindToneFromChar(c));
}
bool IsAlpha(Rune c) {
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
}
bool IsSpace(Rune c) { return c == U' '; }
bool IsWordBreakSymbol(Rune c) {
    static const Text punctuation = U",;:.\"'!? <>=+-*/\\_~`@#$%^&(){}[]|";
    return inKeyList(punctuation, c) || (c >= U'0' && c <= U'9');
}
bool IsVietnameseRune(Rune c) {
    return FindToneFromChar(c) != ToneNone || c != AddMarkToTonelessChar(c, 0);
}
bool HasAnyVietnameseRune(const std::string &s) {
    for (auto c : decode(s))
        if (IsVietnameseRune(lower(c)))
            return true;
    return false;
}
bool HasAnyVietnameseVower(const std::string &s) {
    for (auto c : decode(s))
        if (IsVowel(lower(c)))
            return true;
    return false;
}
std::string encodeCharset(const std::string &name, const std::string &s) {
    auto it = charsetDefinitions().find(name);
    if (it == charsetDefinitions().end())
        return s;
    std::string out;
    for (auto c : decode(s)) {
        auto v = it->second.find(c);
        out += v == it->second.end() ? encode(Text(1, c)) : v->second;
    }
    return out;
}
std::vector<std::string> inputMethodNames() {
    std::vector<std::string> out;
    for (const auto &entry : inputMethodDefinitions())
        out.push_back(entry.first);
    return out;
}
std::vector<std::string> charsetNames() {
    std::vector<std::string> out{"Unicode"};
    for (const auto &entry : charsetDefinitions())
        out.push_back(entry.first);
    return out;
}

static bool letter(Rune c) { return u_isalpha(c); }
static std::pair<Rule, bool> appendingRule(Rune key, const Text &value) {
    for (size_t i = 0; i + 1 < value.size(); ++i) {
        if (value[i] != U'_')
            continue;
        auto start = i + 1;
        if (value[start] == U'_')
            ++start;
        if (start >= value.size() || !letter(value[start]))
            continue;
        Rule r;
        r.Key = key;
        r.EffectOn = r.Result = value[start];
        for (size_t j = start + 1; j < value.size() && letter(value[j]); ++j) {
            Rule next;
            next.Key = key;
            next.EffectOn = next.Result = value[j];
            r.AppendedRules.push_back(next);
        }
        return {r, true};
    }
    return {{}, false};
}
std::vector<Rule> ParseRules(Rune key, const std::string &line) {
    static const std::map<std::string, Tone> tones = {
        {"XoaDauThanh", ToneNone}, {"DauSac", ToneAcute},
        {"DauHuyen", ToneGrave},   {"DauNga", ToneTilde},
        {"DauNang", ToneDot},      {"DauHoi", ToneHook}};
    auto tone = tones.find(line);
    if (tone != tones.end()) {
        Rule r;
        r.Key = key;
        r.Type = ToneTransformation;
        r.Effect = tone->second;
        return {r};
    }
    std::vector<Rule> rules;
    auto text = decode(lowerString(line));
    bool matched = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (!IsAlpha(text[i]))
            continue;
        size_t end = i;
        while (end < text.size() && IsAlpha(text[end]))
            ++end;
        if (end + 1 >= text.size() || text[end] != U'_' ||
            !letter(text[end + 1])) {
            i = end;
            continue;
        }
        size_t resultEnd = end + 1;
        while (resultEnd < text.size() && letter(text[resultEnd]))
            ++resultEnd;
        for (size_t j = i; j < end && end + 1 + j - i < resultEnd; ++j) {
            auto effective = text[j], result = text[end + 1 + j - i];
            auto mark = FindMarkPosition(result);
            if (mark < 0)
                continue;
            auto family = marks.find(effective);
            if (family == marks.end())
                continue;
            for (auto c : family->second) {
                if (c == U'_')
                    continue;
                Rule r;
                r.Key = key;
                r.Type = MarkTransformation;
                if (c == result) {
                    r.EffectOn = result;
                    r.Result = effective;
                    rules.push_back(r);
                } else if (IsVowel(c)) {
                    for (unsigned t = 0; t < 6; ++t) {
                        r.EffectOn = AddToneToChar(c, t);
                        r.Effect = mark;
                        r.Result = AddToneToChar(result, t);
                        rules.push_back(r);
                    }
                } else {
                    r.EffectOn = c;
                    r.Effect = mark;
                    r.Result = result;
                    rules.push_back(r);
                }
            }
        }
        auto tailEnd = resultEnd;
        while (tailEnd < text.size() &&
               (text[tailEnd] == U'_' || letter(text[tailEnd])))
            ++tailEnd;
        auto r =
            appendingRule(key, text.substr(resultEnd, tailEnd - resultEnd));
        if (r.second)
            rules.push_back(r.first);
        matched = true;
        break;
    }
    if (!matched) {
        auto r = appendingRule(key, decode(line));
        if (r.second)
            rules.push_back(r.first);
    }
    return rules;
}
InputMethod ParseInputMethod(const Definitions &definitions,
                             const std::string &name) {
    InputMethod im;
    auto it = definitions.find(name);
    if (it == definitions.end())
        return im;
    im.Name = name;
    for (const auto &entry : it->second) {
        auto keys = decode(entry.first);
        if (keys.empty())
            continue;
        auto key = keys.front();
        auto rules = ParseRules(key, entry.second);
        im.Rules.insert(im.Rules.end(), rules.begin(), rules.end());
        if (lowerString(entry.second).find("uo") != std::string::npos)
            im.SuperKeys.push_back(key);
        im.Keys.push_back(key);
    }
    for (const auto &r : im.Rules) {
        if (r.Type == Appending)
            im.AppendingKeys.push_back(r.Key);
        if (r.Type == ToneTransformation)
            im.ToneKeys.push_back(r.Key);
    }
    return im;
}

static Composition slice(const Composition &c, size_t start, size_t end) {
    return {c.begin() + start, c.begin() + end};
}
static Composition join(Composition a, const Composition &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
static Composition push(Composition a, Trans t) {
    a.push_back(std::move(t));
    return a;
}
static Trans makeTrans(Rule r, Trans target = {}, bool upperCase = false) {
    return std::make_shared<Transformation>(
        Transformation{std::move(r), std::move(target), upperCase});
}
static Trans newAppending(Rune key, bool upperCase) {
    Rule r;
    r.Key = r.EffectOn = r.Result = key;
    return makeTrans(r, {}, upperCase);
}
static Text canvas(const Composition &composition, Mode mode) {
    std::map<Trans, Composition> effects;
    Composition list;
    for (const auto &t : composition) {
        if ((mode & EnglishMode) || t->rule.Type == Appending) {
            if (t->rule.Key)
                list.push_back(t);
        } else if (t->Target)
            effects[t->Target].push_back(t);
    }
    Text out;
    for (const auto &t : list) {
        Rune c = (mode & EnglishMode) ? t->rule.Key : t->rule.EffectOn;
        if (!(mode & EnglishMode)) {
            for (const auto &effect : effects[t]) {
                if (effect->rule.Type == MarkTransformation)
                    c = effect->rule.Effect == MarkRaw
                            ? t->rule.Key
                            : AddMarkToChar(c, effect->rule.Effect);
                if (effect->rule.Type == ToneTransformation)
                    c = AddToneToChar(c, effect->rule.Effect);
            }
        }
        if (mode & ToneLess)
            c = AddToneToChar(c, 0);
        if (mode & MarkLess)
            c = AddMarkToChar(c, 0);
        if (mode & LowerCase)
            c = lower(c);
        else if (t->IsUpperCase)
            c = upper(c);
        out.push_back(c);
    }
    return out;
}
static std::string flatten(const Composition &c, Mode m) {
    return encode(canvas(c, m));
}
static Composition appending(const Composition &c) {
    Composition out;
    for (auto t : c)
        if (t->rule.Type == Appending)
            out.push_back(t);
    return out;
}
static Trans lastAppending(const Composition &c) {
    for (auto i = c.rbegin(); i != c.rend(); ++i)
        if ((*i)->rule.Type == Appending)
            return *i;
    return {};
}
static Trans rootTarget(Trans t) {
    while (t->Target)
        t = t->Target;
    return t;
}
static std::pair<Composition, Composition>
atomic(Composition c, Composition last, bool vowel) {
    while (!c.empty()) {
        auto t = c.back();
        if (t && !t->Target && vowel != IsVowel(t->rule.Result))
            break;
        last.insert(last.begin(), t);
        c.pop_back();
    }
    return {c, last};
}
static std::array<Composition, 3> cvcAppending(const Composition &c) {
    auto [head, lc] = atomic(c, {}, false);
    auto [fc, vo] = atomic(head, {}, true);
    if (!lc.empty() && vo.empty() && fc.empty()) {
        fc = lc;
        lc.clear();
    }
    if (fc.size() == 1 && !vo.empty() &&
        ((fc[0]->rule.Result == U'g' && vo[0]->rule.Result == U'i' &&
          vo.size() > 1 && !(vo[1]->rule.Result == U'e' && !lc.empty())) ||
         (fc[0]->rule.Result == U'q' && vo[0]->rule.Result == U'u'))) {
        fc.push_back(vo.front());
        vo.erase(vo.begin());
    }
    return {fc, vo, lc};
}
static std::array<Composition, 3> cvc(const Composition &c) {
    std::map<Trans, Composition> effects;
    Composition list;
    for (auto t : c) {
        if (!t->Target)
            list.push_back(t);
        else
            effects[t->Target].push_back(t);
    }
    auto parts = cvcAppending(list);
    for (auto &part : parts) {
        auto original = part;
        for (auto t : original)
            part = join(part, effects[t]);
    }
    return parts;
}
static const std::vector<std::string> firstSeq = {
    "b d đ g gh m n nh p ph r s t tr v z", "c h k kh qu th", "ch gi l ng ngh x",
    "đ l", "h"};
static const std::vector<std::string> vowelSeq = {
    "ê i ua uê uy y",
    "a iê oa uyê yê",
    "â ă e o oo ô ơ oe u ư uâ uô ươ",
    "oă",
    "uơ",
    "ai ao au âu ay ây eo êu ia iêu iu oai oao oay oeo oi ôi ơi ưa uây ui ưi "
    "uôi ươi ươu ưu uya uyu yêu",
    "ă",
    "i"};
static const std::vector<std::string> lastSeq = {"ch nh", "c ng", "m n p t",
                                                 "k", "c"};
static const std::vector<std::vector<int>> cvMatrix = {
    {0, 1, 2, 5}, {0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 5}, {6}, {7}};
static const std::vector<std::vector<int>> vcMatrix = {
    {0, 2}, {0, 1, 2}, {1, 2}, {1, 2}, {}, {}, {3}, {4}};
static std::vector<int> lookup(const std::vector<std::string> &seq,
                               const std::string &input, bool full,
                               bool complete) {
    std::vector<int> result;
    auto chars = decode(input);
    for (size_t i = 0; i < seq.size(); ++i) {
        std::istringstream stream(seq[i]);
        std::string word;
        while (stream >> word) {
            auto candidate = decode(word);
            if (candidate.size() < chars.size() ||
                (full && candidate.size() > chars.size()))
                continue;
            bool match = true;
            for (size_t k = 0; k < chars.size(); ++k) {
                if (chars[k] != candidate[k] &&
                    !(!complete &&
                      AddMarkToTonelessChar(candidate[k], 0) == chars[k])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                result.push_back(i);
                break;
            }
        }
    }
    return result;
}
static bool matrixMatch(const std::vector<int> &a, const std::vector<int> &b,
                        const std::vector<std::vector<int>> &matrix) {
    for (auto i : a)
        for (auto j : matrix[i])
            for (auto k : b)
                if (j == k)
                    return true;
    return false;
}
static bool validCVC(const std::string &fc, const std::string &vo,
                     const std::string &lc, bool complete) {
    std::vector<int> f, v, l;
    if (!fc.empty()) {
        f = lookup(firstSeq, fc, complete || !vo.empty(), true);
        if (f.empty())
            return false;
    }
    if (!vo.empty()) {
        v = lookup(vowelSeq, vo, complete || !lc.empty(), complete);
        if (v.empty())
            return false;
    }
    if (!lc.empty()) {
        l = lookup(lastSeq, lc, complete, true);
        if (l.empty())
            return false;
    }
    if (v.empty())
        return !f.empty();
    if (!f.empty()) {
        auto ok = matrixMatch(f, v, cvMatrix);
        if (!ok || l.empty())
            return ok;
    }
    return l.empty() || matrixMatch(v, l, vcMatrix);
}
static bool validTone(const Composition &c, Tone tone) {
    if (tone == ToneNone || tone == ToneAcute || tone == ToneDot)
        return true;
    auto parts = cvc(c);
    auto last = flatten(parts[2], EnglishMode | LowerCase);
    return last != "c" && last != "k" && last != "p" && last != "t" &&
           last != "ch";
}
static bool valid(const Composition &c, bool complete) {
    if (c.size() <= 1)
        return true;
    for (auto it = c.rbegin(); it != c.rend(); ++it) {
        if ((*it)->rule.Type == ToneTransformation) {
            if (!validTone(c, static_cast<Tone>((*it)->rule.Effect)))
                return false;
            break;
        }
    }
    auto parts = cvc(c);
    auto m = VietnameseMode | LowerCase | ToneLess;
    return validCVC(flatten(parts[0], m), flatten(parts[1], m),
                    flatten(parts[2], m), complete);
}
static Trans toneTarget(const Composition &c, bool standard) {
    if (c.empty())
        return {};
    auto parts = cvc(c);
    auto vowels = appending(parts[1]);
    Trans target;
    if (vowels.size() == 1)
        return vowels[0];
    if (vowels.size() == 2 && standard) {
        for (auto t : parts[1])
            if (t->rule.Result == U'ơ' || t->rule.Result == U'ê')
                target = t->Target ? t->Target : t;
        if (!target)
            target = vowels[parts[2].empty() ? 0 : 1];
    } else if (vowels.size() == 2) {
        if (!parts[2].empty())
            return vowels[1];
        auto s = flatten(vowels, EnglishMode | LowerCase | ToneLess | MarkLess);
        target = vowels[(s == "oa" || s == "oe" || s == "uy" || s == "ue" ||
                         s == "uo")
                            ? 1
                            : 0];
    } else if (vowels.size() == 3) {
        target = vowels[flatten(vowels, EnglishMode | LowerCase | ToneLess |
                                            MarkLess) == "uye"
                            ? 2
                            : 1];
    }
    return target;
}
static std::pair<Composition, Composition>
lastWord(const Composition &c, const Text &keys, bool punctuation = false) {
    for (size_t k = c.size(); k > 0; --k) {
        auto i = k - 1;
        auto chars = canvas(slice(c, i, c.size()),
                            punctuation ? EnglishMode
                                        : VietnameseMode | LowerCase |
                                              ToneLess | MarkLess);
        if (chars.empty())
            continue;
        auto ch = chars[0];
        if (punctuation ? IsSpace(ch) : (!IsAlpha(ch) && !inKeyList(keys, ch)))
            return {slice(c, 0, i + 1), slice(c, i + 1, c.size())};
    }
    return {{}, c};
}
static std::pair<Composition, Composition> lastSyllable(const Composition &c) {
    auto [previous, last] = lastWord(c, {});
    size_t anchor = 0;
    for (size_t i = 0; i < last.size(); ++i)
        if (!valid(slice(last, anchor, i + 1), false))
            anchor = i;
    return {join(previous, slice(last, 0, anchor)),
            slice(last, anchor, last.size())};
}
static bool freeTarget(const Composition &c, Trans t, EffectType type) {
    for (auto trans : c)
        if (trans->Target == t && trans->rule.Type == type)
            return false;
    return true;
}
static std::pair<Trans, Rule> findTarget(const Composition &c,
                                         const std::vector<Rule> &rules,
                                         unsigned flags) {
    auto str = flatten(c, VietnameseMode);
    for (auto r : rules) {
        if (r.Type != ToneTransformation)
            continue;
        Trans target;
        if (flags & EfreeToneMarking) {
            if (validTone(c, static_cast<Tone>(r.Effect)))
                target = toneTarget(c, flags & EstdToneStyle);
        } else {
            auto last = lastAppending(c);
            if (last && IsVowel(last->rule.EffectOn))
                target = last;
        }
        if (str == flatten(push(c, makeTrans(r, target)), VietnameseMode))
            continue;
        if (r.Effect == ToneNone && freeTarget(c, target, ToneTransformation) &&
            target && FindToneFromChar(target->rule.Result) == ToneNone)
            target.reset();
        return {target, r};
    }
    for (auto it = c.rbegin(); it != c.rend(); ++it) {
        for (auto r : rules) {
            if (r.Type != MarkTransformation ||
                (*it)->rule.Result != r.EffectOn || !r.Effect)
                continue;
            auto target = rootTarget(*it);
            auto tmp = push(c, makeTrans(r, target));
            if (str == flatten(tmp, VietnameseMode))
                continue;
            if (valid(tmp, false))
                return {target, r};
        }
    }
    return {};
}
static Composition undo(const Composition &c, const std::vector<Rule> &rules,
                        unsigned flags) {
    Composition out;
    auto str = flatten(c, VietnameseMode | ToneLess | LowerCase);
    for (auto r : rules) {
        if (r.Type == ToneTransformation) {
            Trans target;
            if (flags & EfreeToneMarking) {
                if (validTone(c, static_cast<Tone>(r.Effect)))
                    target = toneTarget(c, flags & EstdToneStyle);
            } else {
                auto last = lastAppending(c);
                if (last && IsVowel(last->rule.EffectOn))
                    target = last;
            }
            if (target) {
                Rule effect;
                effect.Type = ToneTransformation;
                out.push_back(makeTrans(effect, target));
            }
        } else if (r.Type == MarkTransformation) {
            for (auto it = c.rbegin(); it != c.rend(); ++it) {
                if ((*it)->rule.Result != r.EffectOn)
                    continue;
                Rule effect;
                effect.Type = MarkTransformation;
                auto t = makeTrans(effect, rootTarget(*it));
                if (str !=
                    flatten(push(c, t), VietnameseMode | ToneLess | LowerCase))
                    out.push_back(t);
            }
        }
    }
    return out;
}
static bool uhO(const std::string &s) {
    return s.find("ưo") != std::string::npos ||
           s.find("ươ") != std::string::npos;
}
static bool uOhTail(const std::string &s) {
    auto text = decode(s);
    for (size_t i = 0; i + 2 < text.size(); ++i)
        if (((text[i] == U'u' && text[i + 1] == U'ơ') ||
             (text[i] == U'ư' && text[i + 1] == U'o')) &&
            letter(text[i + 2]))
            return true;
    return false;
}
static Composition generate(const Composition &c,
                            const std::vector<Rule> &rules, unsigned flags,
                            Rune key, bool upperCase) {
    Composition out;
    if (!c.empty()) {
        auto r = c.back()->rule;
        if (r.Type == Appending && r.Key == key && r.Key != r.Result) {
            Rule effect;
            effect.Type = MarkTransformation;
            effect.Effect = MarkRaw;
            return {makeTrans(effect, c.back())};
        }
    }
    auto [target, r] = findTarget(c, rules, flags);
    if (target) {
        out.push_back(makeTrans(r, target, upperCase));
        if (r.Type != MarkTransformation)
            return out;
        auto next = join(c, out);
        if (valid(next, true))
            return out;
        auto [other, vr] = findTarget(next, rules, flags);
        if (other) {
            vr.Key = 0;
            out.push_back(makeTrans(vr, other));
            return out;
        }
    } else {
        if (uhO(flatten(c, VietnameseMode | ToneLess | LowerCase))) {
            auto vowels = appending(cvc(c)[1]);
            if (!vowels.empty()) {
                Rule effect;
                effect.Type = MarkTransformation;
                auto t = makeTrans(effect, vowels[0]);
                auto [other, rule] = findTarget(push(c, t), rules, flags);
                if (other && other != vowels[0])
                    return {t, makeTrans(rule, other, upperCase)};
            }
        }
        auto undone = undo(c, rules, flags);
        if (!undone.empty())
            out = push(undone, newAppending(key, upperCase));
    }
    return out;
}
static Composition fallback(const std::vector<Rule> &rules, Rune key,
                            bool upperCase) {
    auto t = newAppending(key, upperCase);
    for (auto r : rules) {
        if (r.Key == key && r.Type == Appending) {
            bool uppercase = upperCase || isUpper(r.EffectOn);
            r.EffectOn = r.Result = lower(r.EffectOn);
            t = makeTrans(r, {}, uppercase);
            break;
        }
    }
    Composition out{t};
    for (auto r : t->rule.AppendedRules) {
        bool uppercase = upperCase || isUpper(r.EffectOn);
        r.Key = 0;
        r.EffectOn = r.Result = lower(r.EffectOn);
        out.push_back(makeTrans(r, {}, uppercase));
    }
    return out;
}
Core::Core(InputMethod method, unsigned flags)
    : inputMethod_(std::move(method)), flags_(flags) {}
std::vector<Rule> Core::applicableRules(Rune key) const {
    std::vector<Rule> out;
    for (auto r : inputMethod_.Rules)
        if (r.Key == lower(key))
            out.push_back(r);
    return out;
}
bool Core::CanProcessKey(Rune key) const {
    if (IsAlpha(key) || inKeyList(inputMethod_.Keys, key))
        return true;
    return !IsWordBreakSymbol(key) && IsVietnameseRune(key);
}
bool Core::IsValid(bool complete) const {
    return valid(lastWord(composition_, inputMethod_.Keys).second, complete);
}
std::string Core::GetProcessedString(Mode mode) const {
    if (mode & FullText)
        return flatten(composition_, mode);
    if (mode & PunctuationMode)
        return flatten(lastWord(composition_, inputMethod_.Keys, true).second,
                       VietnameseMode);
    return flatten(lastWord(composition_, inputMethod_.Keys).second, mode);
}
Composition Core::refreshTone(Composition c) {
    if (!(flags_ & EfreeToneMarking) || !valid(c, false))
        return {};
    if (cvc(c)[1].empty())
        return {};
    Trans last;
    for (auto it = c.rbegin(); it != c.rend(); ++it)
        if ((*it)->rule.Type == ToneTransformation && (*it)->Target) {
            last = *it;
            break;
        }
    if (!last)
        return {};
    auto target = toneTarget(c, flags_ & EstdToneStyle);
    if (last->Target == target)
        return {};
    // Match the original engine: move the old tone before emitting overrides.
    last->Target = target;
    Rule reset;
    reset.Type = ToneTransformation;
    auto override = last->rule;
    override.Key = 0;
    return {makeTrans(reset, last->Target), makeTrans(override, target)};
}
Composition Core::newComposition(Composition c, Rune key, bool upperCase) {
    auto [previous, last] = lastSyllable(c);
    auto rules = applicableRules(key);
    auto next = generate(last, rules, flags_, key, upperCase);
    if (next.empty()) {
        next = fallback(rules, key, upperCase);
        auto combined = join(last, next);
        if (!inputMethod_.SuperKeys.empty() &&
            uOhTail(flatten(combined, ToneLess | LowerCase))) {
            auto [target, rule] = findTarget(
                combined, applicableRules(inputMethod_.SuperKeys[0]), flags_);
            if (target) {
                rule.Key = 0;
                next.push_back(makeTrans(rule, target));
            }
        }
    }
    next = join(next, refreshTone(join(last, next)));
    return join(previous, join(last, next));
}
void Core::ProcessKey(Rune key, Mode mode) {
    auto low = lower(key);
    if ((mode & EnglishMode) || !CanProcessKey(low)) {
        auto t = newAppending(low, isUpper(key));
        if (mode & InReverseOrder)
            composition_.insert(composition_.begin(), t);
        else
            composition_.push_back(t);
    } else
        composition_ = newComposition(composition_, low, isUpper(key));
}
void Core::ProcessString(const std::string &s, Mode mode) {
    for (auto c : decode(s))
        ProcessKey(c, mode);
}
void Core::RestoreLastWord(bool vietnamese) {
    auto [previous, last] = lastWord(composition_, inputMethod_.Keys);
    if (last.empty())
        return;
    Composition next;
    for (auto t : last) {
        if (vietnamese)
            next = newComposition(next, t->rule.Key, t->IsUpperCase);
        else if (t->rule.Key)
            next.push_back(newAppending(t->rule.Key, t->IsUpperCase));
    }
    composition_ = join(previous, next);
}
void Core::RemoveLastChar(bool refresh) {
    auto last = lastAppending(composition_);
    if (!last)
        return;
    if (!CanProcessKey(last->rule.Key)) {
        composition_.pop_back();
        return;
    }
    auto [previous, word] = lastWord(composition_, inputMethod_.Keys);
    Composition next;
    for (auto t : word)
        if (t != last && t->Target != last)
            next.push_back(t);
    if (refresh)
        next = join(next, refreshTone(next));
    composition_ = join(previous, next);
}
} // namespace bamboo
