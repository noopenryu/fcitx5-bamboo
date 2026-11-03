/* SPDX-FileCopyrightText: 2018 Luong Thanh Lam
 * SPDX-FileCopyrightText: 2022 CSSlayer <wengxt@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "core.h"
#include <set>

namespace bamboo {
using Dictionary = std::set<std::string>;
using MacroTable = std::map<std::string, std::string>;
struct Options {
    bool autoNonVnRestore = true;
    bool ddFreeStyle = true;
    bool macroEnabled = false;
    bool autoCapitalizeMacro = false;
    bool spellCheckWithDicts = true;
    std::string outputCharset = "Unicode";
    bool modernStyle = false;
    bool freeMarking = true;
};
std::shared_ptr<const Dictionary> readDictionary(int fd);
class Adapter {
public:
    Adapter(InputMethod method, std::shared_ptr<const Dictionary> dictionary,
            std::shared_ptr<const MacroTable> macros);
    bool processKey(uint32_t key, uint32_t state);
    void setOptions(Options options);
    void requestRestore() { restore_ = true; }
    void commitPreedit();
    void reset();
    std::string pullCommit();
    const std::string &preedit() const { return preedit_; }

private:
    std::string processed(Mode mode) const;
    std::string preeditString() const;
    bool fallback(bool checkVn) const;
    bool mustFallback() const;
    bool printable(uint32_t state, uint32_t key) const;
    std::pair<bool, std::string> macroText() const;
    std::string expandMacro(const std::string &s) const;
    bool macroHas(const std::string &s) const;
    bool macroPrefix(const std::string &s) const;
    std::pair<std::string, bool> commitText(uint32_t key, uint32_t state);
    std::string nonVnWord(uint32_t key, uint32_t state);
    void commitAndReset(std::string s);
    void updatePreedit(const std::string &s);
    Core core_;
    std::shared_ptr<const Dictionary> dictionary_;
    std::shared_ptr<const MacroTable> macros_;
    Options options_;
    std::string preedit_, commit_;
    bool restore_ = false;
};
} // namespace bamboo
