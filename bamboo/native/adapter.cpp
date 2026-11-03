/* SPDX-FileCopyrightText: 2018 Luong Thanh Lam
 * SPDX-FileCopyrightText: 2022 CSSlayer <wengxt@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include "adapter.h"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace bamboo {
namespace {
constexpr uint32_t Lock = 2;
constexpr uint32_t InvalidStates =
    (1 << 2) | (1 << 3) | (1 << 25) | (1 << 26) | (1 << 27) | (1 << 28);
constexpr uint32_t Backspace = 0xff08, Tab = 0xff09;
} // namespace
std::shared_ptr<const Dictionary> readDictionary(int fd) {
    auto dictionary = std::make_shared<Dictionary>();
    std::string pending;
    char buffer[8192];
    for (;;) {
        auto count = read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR)
                continue;
            throw std::system_error(errno, std::generic_category(),
                                    "Read Bamboo dictionary");
        }
        if (!count)
            break;
        pending.append(buffer, count);
        size_t start = 0, end;
        while ((end = pending.find('\n', start)) != std::string::npos) {
            auto line = pending.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                dictionary->insert(lowerString(line));
            start = end + 1;
        }
        pending.erase(0, start);
    }
    if (!pending.empty()) {
        if (pending.back() == '\r')
            pending.pop_back();
        if (!pending.empty())
            dictionary->insert(lowerString(pending));
    }
    return dictionary;
}
Adapter::Adapter(InputMethod method,
                 std::shared_ptr<const Dictionary> dictionary,
                 std::shared_ptr<const MacroTable> macros)
    : core_(std::move(method)), dictionary_(std::move(dictionary)),
      macros_(std::move(macros)) {
    if (!dictionary_)
        dictionary_ = std::make_shared<Dictionary>();
    if (!macros_)
        macros_ = std::make_shared<MacroTable>();
}
void Adapter::setOptions(Options options) {
    options_ = std::move(options);
    unsigned flags = EstdFlags;
    if (options_.modernStyle)
        flags &= ~EstdToneStyle;
    else
        flags |= EstdToneStyle;
    if (options_.freeMarking)
        flags |= EfreeToneMarking;
    else
        flags &= ~EfreeToneMarking;
    core_.SetFlag(flags);
}
std::string Adapter::processed(Mode mode) const {
    return core_.GetProcessedString(mode);
}
bool Adapter::macroHas(const std::string &s) const {
    auto it = macros_->find(lowerString(s));
    return it != macros_->end() && !it->second.empty();
}
bool Adapter::macroPrefix(const std::string &s) const {
    auto it = macros_->find(s);
    if (it != macros_->end() && !it->second.empty())
        return true;
    for (const auto &entry : *macros_)
        if (entry.first.compare(0, s.size(), s) == 0)
            return true;
    return false;
}
std::string Adapter::expandMacro(const std::string &s) const {
    auto it = macros_->find(lowerString(s));
    if (it == macros_->end())
        return {};
    auto text = it->second;
    if (options_.autoCapitalizeMacro) {
        auto chars = decode(s);
        if (!chars.empty() && isLower(chars[0]))
            return lowerString(text);
        bool allCapital = true;
        for (size_t i = 1; i < chars.size(); ++i)
            if (isLower(chars[i]) || IsWordBreakSymbol(chars[i]))
                allCapital = false;
        if (allCapital)
            return upperString(text);
    }
    return text;
}
std::pair<bool, std::string> Adapter::macroText() const {
    if (!options_.macroEnabled)
        return {false, {}};
    auto text = processed(PunctuationMode);
    return macroHas(text) ? std::make_pair(true, expandMacro(text))
                          : std::make_pair(false, std::string{});
}
bool Adapter::fallback(bool checkVn) const {
    if (!options_.autoNonVnRestore)
        return false;
    auto text = processed(VietnameseMode | LowerCase);
    auto chars = decode(text);
    if (chars.empty() || macroText().first)
        return false;
    if (options_.ddFreeStyle && !HasAnyVietnameseVower(text) &&
        (chars.back() == U'd' || text.find("đ") != std::string::npos))
        return false;
    if (checkVn && !HasAnyVietnameseRune(text))
        return false;
    return !core_.IsValid(false);
}
bool Adapter::mustFallback() const {
    if (!options_.autoNonVnRestore)
        return false;
    auto text = processed(VietnameseMode | LowerCase);
    if (text.empty())
        return false;
    if (options_.ddFreeStyle && text.find("đ") != std::string::npos)
        return false;
    return options_.spellCheckWithDicts ? dictionary_->count(text) == 0
                                        : !core_.IsValid(true);
}
std::string Adapter::preeditString() const {
    if (options_.macroEnabled)
        return processed(PunctuationMode);
    return processed(fallback(true) ? EnglishMode : VietnameseMode);
}
bool Adapter::printable(uint32_t state, uint32_t key) const {
    if (state & InvalidStates)
        return false;
    if (key == Backspace || IsWordBreakSymbol(key))
        return true;
    if (key == Tab && macroText().first)
        return true;
    return core_.CanProcessKey(key);
}
void Adapter::commitAndReset(std::string s) {
    commit_ = std::move(s);
    preedit_.clear();
    core_.Reset();
}
void Adapter::reset() { commitAndReset({}); }
void Adapter::commitPreedit() { commitAndReset(preeditString()); }
std::string Adapter::pullCommit() { return std::exchange(commit_, {}); }
void Adapter::updatePreedit(const std::string &s) {
    auto text = encodeCharset(options_.outputCharset, s);
    if (text.empty()) {
        preedit_.clear();
        commit_.clear();
    } else
        preedit_ = std::move(text);
}
bool Adapter::processKey(uint32_t key, uint32_t state) {
    auto rawLength = processed(EnglishMode | FullText).size();
    auto old = preeditString();
    if (!restore_ && !core_.CanProcessKey(key) && !rawLength &&
        !options_.macroEnabled)
        return false;
    if (key == Backspace) {
        if (decode(old).size() == 1) {
            commitAndReset({});
            return true;
        }
        if (!rawLength)
            return false;
        core_.RemoveLastChar(true);
        updatePreedit(preeditString());
        return true;
    }
    if (key == Tab) {
        auto macro = macroText();
        if (macro.first) {
            commitAndReset(macro.second);
            return true;
        }
        commitAndReset(HasAnyVietnameseRune(old) && mustFallback()
                           ? processed(EnglishMode)
                           : old);
        return false;
    }
    auto [text, wordBreak] = commitText(key, state);
    auto accepted = printable(state, key);
    if (wordBreak)
        commitAndReset(std::move(text));
    else
        updatePreedit(text);
    return accepted;
}
std::pair<std::string, bool> Adapter::commitText(uint32_t key, uint32_t state) {
    auto old = preeditString();
    if (restore_) {
        restore_ = false;
        core_.RestoreLastWord(!HasAnyVietnameseRune(old));
        return {preeditString(), false};
    }
    auto validKey = printable(state, key);
    auto keyString = validKey ? encode(Text(1, key)) : std::string{};
    Rune rune = key;
    if (validKey && core_.CanProcessKey(rune)) {
        if ((state & Lock) &&
            inKeyList(core_.GetInputMethod().AppendingKeys, rune)) {
            switch (rune) {
            case U'[':
                rune = U'{';
                break;
            case U']':
                rune = U'}';
                break;
            case U'{':
                rune = U'[';
                break;
            case U'}':
                rune = U']';
                break;
            default:
                break;
            }
        }
        core_.ProcessKey(rune, fallback(false) ? EnglishMode : VietnameseMode);
        if (inKeyList(core_.GetInputMethod().AppendingKeys, rune)) {
            auto text =
                processed(fallback(true) ? EnglishMode : VietnameseMode);
            auto full = processed(VietnameseMode);
            // The old adapter explicitly compares the final UTF-8 byte here.
            if (!full.empty() &&
                static_cast<unsigned char>(full.back()) == rune) {
                auto ret = preeditString();
                bool wordBreak =
                    !ret.empty() &&
                    IsWordBreakSymbol(static_cast<unsigned char>(ret.back()));
                if (wordBreak) {
                    core_.RemoveLastChar(false);
                    core_.ProcessKey(U' ', EnglishMode);
                }
                return {ret, wordBreak};
            }
            auto chars = decode(text);
            if (!chars.empty() && rune == chars.back()) {
                bool wordBreak = IsWordBreakSymbol(rune);
                if (wordBreak) {
                    core_.RemoveLastChar(false);
                    core_.ProcessKey(U' ', EnglishMode);
                }
                return {old + encode(Text(1, rune)), wordBreak};
            }
            return {preeditString(), false};
        }
        return {options_.macroEnabled ? processed(PunctuationMode)
                                      : preeditString(),
                false};
    }
    if (options_.macroEnabled) {
        if (validKey && macroPrefix(old + keyString)) {
            core_.ProcessKey(rune, EnglishMode);
            return {old + keyString, false};
        }
        if (macroHas(old))
            return {expandMacro(old) + keyString, true};
    }
    return {nonVnWord(key, state), true};
}
std::string Adapter::nonVnWord(uint32_t key, uint32_t state) {
    auto validKey = printable(state, key);
    auto keyString = validKey ? encode(Text(1, key)) : std::string{};
    auto old = preeditString();
    if (HasAnyVietnameseRune(old) && mustFallback()) {
        core_.RestoreLastWord(false);
        auto text = processed(PunctuationMode | EnglishMode) + keyString;
        if (validKey)
            core_.ProcessKey(key, EnglishMode);
        return text;
    }
    if (validKey)
        core_.ProcessKey(key, EnglishMode);
    return old + keyString;
}
} // namespace bamboo
