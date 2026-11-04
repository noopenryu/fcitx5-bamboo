/* SPDX-License-Identifier: MIT */
#include "adapter.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

using namespace bamboo;
namespace {
void require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
std::vector<std::string> fields(const std::string &line) {
    std::vector<std::string> out;
    size_t start = 0, end;
    while ((end = line.find('\t', start)) != std::string::npos) {
        out.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    out.push_back(line.substr(start));
    return out;
}
std::string unhex(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i += 2)
        out.push_back(std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
bool boolean(const std::string &s) { return s == "true"; }
void coreFixtures() {
    std::ifstream stream(std::string(BAMBOO_FIXTURE_DIR) + "/core.tsv");
    require(stream.good(), "Cannot open core fixtures");
    std::unique_ptr<Core> engine;
    std::string line, method, history;
    unsigned lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        auto f = fields(line);
        if (f[0] == "I") {
            method = unhex(f[1]);
            history.clear();
            engine = std::make_unique<Core>(
                ParseInputMethod(inputMethodDefinitions(), method),
                std::stoul(f[2]));
        } else if (f[0] == "K") {
            auto c = std::stoul(f[1]);
            history += encode(Text(1, c));
            engine->ProcessKey(c, std::stoul(f[2]));
        } else if (f[0] == "G") {
            auto got = engine->GetProcessedString(std::stoul(f[1]));
            auto expected = unhex(f[2]);
            require(got == expected,
                    "core fixture line " + std::to_string(lineNumber) +
                        " method=" + method + " keys=" + history +
                        " mode=" + f[1] + " expected=[" + expected + "] got=[" +
                        got + "]");
        } else if (f[0] == "V")
            require(engine->IsValid(boolean(f[1])) == boolean(f[2]),
                    "validity fixture line " + std::to_string(lineNumber) +
                        " method=" + method + " keys=" + history);
        else if (f[0] == "C")
            require(engine->CanProcessKey(std::stoul(f[1])) == boolean(f[2]),
                    "CanProcessKey fixture");
        else if (f[0] == "D")
            engine->RemoveLastChar(boolean(f[1]));
        else if (f[0] == "R")
            engine->RestoreLastWord(boolean(f[1]));
        else if (f[0] == "F")
            engine->SetFlag(std::stoul(f[1]));
        else if (f[0] == "Z") {
            engine->Reset();
            history.clear();
        } else
            throw std::runtime_error("Unknown fixture instruction");
    }
    require(lineNumber > 1000, "Empty core fixtures");
    std::cout << "Core parity: " << lineNumber
              << " operations and assertions\n";
}
void adapterFixtures() {
    std::ifstream stream(std::string(BAMBOO_FIXTURE_DIR) + "/adapter.tsv");
    require(stream.good(), "Cannot open adapter fixtures");
    auto dictionary = std::make_shared<Dictionary>(
        Dictionary{"chuẩn", "tiếng", "việt", "đài"});
    auto macros = std::make_shared<MacroTable>(
        MacroTable{{"btw", "By The Way"}, {"vn", "Việt Nam"}});
    std::unique_ptr<Adapter> engine;
    std::string line, method, history;
    unsigned lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        auto f = fields(line);
        if (f[0] == "I") {
            method = unhex(f[1]);
            history.clear();
            auto definitions = inputMethodDefinitions();
            if (method == "Custom")
                definitions = {
                    {"Custom",
                     {{"q", "__ươ"}, {"w", "UOA_ƯƠĂ__Ư"}, {"s", "DauSac"}}}};
            engine = std::make_unique<Adapter>(
                ParseInputMethod(definitions, method), dictionary, macros);
        } else if (f[0] == "O") {
            auto mask = std::stoul(f[1]);
            Options options;
            options.autoNonVnRestore = mask & 1;
            options.ddFreeStyle = mask & 2;
            options.macroEnabled = mask & 4;
            options.autoCapitalizeMacro = mask & 8;
            options.spellCheckWithDicts = mask & 16;
            options.modernStyle = mask & 32;
            options.freeMarking = mask & 64;
            options.outputCharset = unhex(f[2]);
            engine->setOptions(options);
        } else if (f[0] == "K") {
            auto key = std::stoul(f[1]);
            history += encode(Text(1, key));
            auto accepted = engine->processKey(key, std::stoul(f[2]));
            auto commit = engine->pullCommit();
            auto context = "adapter fixture line " +
                           std::to_string(lineNumber) + " method=" + method +
                           " keys=" + history;
            require(accepted == boolean(f[3]), context + " accepted differs");
            require(engine->preedit() == unhex(f[4]),
                    context + " preedit expected=[" + unhex(f[4]) + "] got=[" +
                        engine->preedit() + "]");
            require(commit == unhex(f[5]), context + " commit expected=[" +
                                               unhex(f[5]) + "] got=[" +
                                               commit + "]");
        } else if (f[0] == "Z") {
            engine->reset();
            history.clear();
        } else if (f[0] == "R")
            engine->requestRestore();
        else if (f[0] == "B") {
            engine->commitPreedit();
            require(engine->pullCommit() == unhex(f[1]),
                    "Focus commit fixture");
        } else
            throw std::runtime_error("Unknown adapter fixture instruction");
    }
    require(lineNumber > 1000, "Empty adapter fixtures");
    std::cout << "Adapter parity: " << lineNumber << " events and options\n";
}
void utilityFixtures() {
    std::ifstream stream(std::string(BAMBOO_FIXTURE_DIR) + "/utility.tsv");
    require(stream.good(), "Cannot open utility fixtures");
    std::string line;
    while (std::getline(stream, line)) {
        auto f = fields(line);
        if (f[0] == "T" || f[0] == "M") {
            auto c = std::stoul(f[1]), effect = std::stoul(f[2]),
                 expected = std::stoul(f[3]);
            require((f[0] == "T" ? AddToneToChar(c, effect)
                                 : AddMarkToChar(c, effect)) == expected,
                    "Tone/mark fixture " + line);
        } else {
            auto c = std::stoul(f[0]);
            require(FindVowelPosition(c) == std::stoi(f[1]) &&
                        FindMarkPosition(c) == std::stoi(f[2]) &&
                        IsVowel(c) == boolean(f[3]) &&
                        IsVietnameseRune(c) == boolean(f[4]) &&
                        IsWordBreakSymbol(c) == boolean(f[5]),
                    "Utility fixture " + line);
        }
    }
    std::ifstream charsets(std::string(BAMBOO_FIXTURE_DIR) + "/charsets.tsv");
    require(charsets.good(), "Cannot open charset fixtures");
    size_t count = 0;
    while (std::getline(charsets, line)) {
        auto f = fields(line);
        require(encodeCharset(unhex(f[0]), unhex(f[1])) == unhex(f[2]),
                "Charset fixture " + unhex(f[0]));
        ++count;
    }
    require(count == charsetNames().size(), "Missing charset fixture");
}
void parserTests() {
    auto rules = ParseRules(U'z', "XoaDauThanh");
    require(rules.size() == 1 && rules[0].Type == ToneTransformation &&
                rules[0].Effect == ToneNone,
            "Tone none parser");
    rules = ParseRules(U'x', "DauNga");
    require(rules.size() == 1 && rules[0].Effect == ToneTilde,
            "Tone tilde parser");
    rules = ParseRules(U'd', "D_Đ");
    require(rules.size() == 2 && rules[0].Effect == MarkDash &&
                rules[0].EffectOn == U'd',
            "Dash parser");
    rules = ParseRules(U'{', "_Ư");
    require(rules.size() == 1 && rules[0].Type == Appending &&
                rules[0].EffectOn == U'Ư',
            "Uppercase appending parser");
    for (auto tail : {"", "__Ư"}) {
        rules = ParseRules(U'w', std::string("UOA_ƯƠĂ") + tail);
        require(rules.size() == (std::string(tail).empty() ? 33 : 34),
                "Combined parser size");
        require(rules[0].Type == MarkTransformation &&
                    rules[0].Effect == MarkHorn && rules[0].EffectOn == U'u',
                "Horn parser");
        require(rules[7].Effect == MarkHorn && rules[7].EffectOn == U'o',
                "Second horn parser");
        require(rules[20].Effect == MarkBreve && rules[20].EffectOn == U'a',
                "Breve parser");
        if (rules.size() == 34)
            require(rules[33].Type == Appending && rules[33].EffectOn == U'ư',
                    "Combined append parser");
    }
    for (auto text : {"__ươ", "__ƯƠ"}) {
        rules = ParseRules(U'[', text);
        auto chars = decode(text);
        require(rules.size() == 1 && rules[0].AppendedRules.size() == 1 &&
                    rules[0].AppendedRules[0].Type == Appending &&
                    rules[0].AppendedRules[0].EffectOn == chars.back(),
                "Multi-character appending parser");
    }
    require(ParseRules(U'w', "UOA_Ư").size() > 0,
            "Truncated custom rule must not index past result");
    require(ParseInputMethod({}, "missing").Rules.empty(),
            "Unknown input method");
}
void adapterTests() {
    auto dictionary = std::make_shared<Dictionary>(
        Dictionary{"chuẩn", "tiếng", "việt", "đài"});
    auto macros = std::make_shared<MacroTable>(
        MacroTable{{"btw", "By The Way"}, {"vn", "Việt Nam"}});
    auto make = [&] {
        return Adapter(ParseInputMethod(inputMethodDefinitions(), "Telex 2"),
                       dictionary, macros);
    };
    auto type = [](Adapter &e, const std::string &s) {
        for (auto c : decode(s))
            e.processKey(c, 0);
    };
    auto engine = make();
    type(engine, "chuaarn");
    require(engine.preedit() == "chuẩn", "Adapter composition");
    require(engine.processKey(U' ', 0), "Space accepted");
    require(engine.pullCommit() == "chuẩn " && engine.pullCommit().empty() &&
                engine.preedit().empty(),
            "Commit is drained once");
    type(engine, "aw");
    engine.processKey(0xff08, 0);
    require(engine.preedit().empty(), "Backspace single composed character");
    engine.reset();
    require(!engine.processKey(U' ', 0), "Empty space passed through");
    type(engine, "chuaarn");
    auto other = make();
    type(other, "ddafi");
    require(engine.preedit() == "chuẩn" && other.preedit() == "đài",
            "Independent contexts");
    engine.requestRestore();
    engine.processKey(U' ', 1);
    require(engine.preedit() == "chuaarn", "Restore raw key strokes");
    engine.reset();
    type(engine, "chuaarn");
    require(!engine.processKey(U'a', 4) && engine.pullCommit() == "chuẩn",
            "Control modifier passes through and commits");
    Options opts;
    opts.macroEnabled = true;
    opts.autoCapitalizeMacro = true;
    engine.setOptions(opts);
    type(engine, "BTW");
    engine.processKey(0xff09, 0);
    require(engine.pullCommit() == "BY THE WAY", "Tab expands uppercase macro");
    type(engine, "btw");
    engine.processKey(U' ', 0);
    require(engine.pullCommit() == "by the way ",
            "Space expands lowercase macro");
    opts.macroEnabled = false;
    opts.autoNonVnRestore = false;
    opts.outputCharset = "TCVN3 (ABC)";
    engine.setOptions(opts);
    type(engine, "aw");
    require(engine.preedit() == encodeCharset(opts.outputCharset, "ă"),
            "Encoded preedit");
    engine.reset();
    opts.outputCharset = "Unicode";
    opts.autoNonVnRestore = true;
    engine.setOptions(opts);
    type(engine, "chuaarn");
    engine.commitPreedit();
    require(engine.pullCommit() == "chuẩn", "Focus-out commit");
    auto custom =
        Adapter(ParseInputMethod({{"Custom", {{"q", "__ươ"}}}}, "Custom"),
                dictionary, macros);
    opts.autoNonVnRestore = false;
    custom.setOptions(opts);
    type(custom, "q");
    require(custom.preedit() == "ư",
            "Custom multi-character virtual appending parity");
    // Reload ownership: existing engines retain immutable snapshots.
    dictionary.reset();
    macros.reset();
    type(other, " ");
    require(other.pullCommit() == "đài ", "Snapshot lifetime");
    int descriptors[2];
    require(pipe(descriptors) == 0, "Dictionary pipe");
    const std::string words = "CHUẨN\r\n\nViệt\nđài";
    require(write(descriptors[1], words.data(), words.size()) ==
                static_cast<ssize_t>(words.size()),
            "Write dictionary");
    close(descriptors[1]);
    auto loaded = readDictionary(descriptors[0]);
    close(descriptors[0]);
    require(loaded->count("chuẩn") && loaded->count("việt") &&
                loaded->count("đài") && loaded->size() == 3,
            "Dictionary casing and final line");
    for (int i = 0; i < 100; ++i) {
        auto temporary = make();
        type(temporary, "nguowfi");
        temporary.reset();
    }
}
void repeatedKeyTests() {
    constexpr size_t count = 256;
    for (const std::string method : {"Telex", "Telex 2", "VNI"}) {
        for (bool restore : {false, true}) {
            const auto context = method + " restore=" + std::to_string(restore);
            auto make = [&] {
                Adapter engine(
                    ParseInputMethod(inputMethodDefinitions(), method), {}, {});
                Options options;
                options.autoNonVnRestore = restore;
                engine.setOptions(options);
                return engine;
            };
            struct RepeatCase {
                std::string prefix;
                char key;
                std::string expected;
            };
            const bool vni = method == "VNI";
            std::vector<RepeatCase> cases = {
                {"", 'k', std::string(count, 'k')},
                {"", 'K', std::string(count, 'K')},
                {"", 'a',
                 vni       ? std::string(count, 'a')
                 : restore ? std::string(count - 1, 'a')
                           : std::string(count / 2 - 1, 'a') + "â"},
                {"a", 's', "a" + std::string(vni ? count : count - 1, 's')},
                {"a", 'w',
                 "a" + std::string(vni ? count
                                   : method == "Telex 2" && !restore
                                       ? count / 2
                                       : count - 1,
                                   'w')}};
            if (vni)
                cases.push_back({"a", '1', "a" + std::string(count - 1, '1')});
            for (const auto &test : cases) {
                const auto context = method +
                                     " restore=" + std::to_string(restore) +
                                     " repeated " + test.key;
                auto engine = make();
                for (char key : test.prefix)
                    engine.processKey(key, 0);
                for (size_t i = 0; i < count; ++i) {
                    require(engine.processKey(test.key, 0),
                            context + " key not accepted");
                    require(engine.pullCommit().empty(),
                            context + " committed early");
                }
                require(engine.preedit() == test.expected,
                        context + " preedit differs");
                auto shortened = decode(test.expected);
                shortened.pop_back();
                require(engine.processKey(0xff08, 0) &&
                            engine.preedit() == encode(shortened),
                        context + " backspace differs");
                require(engine.processKey(U' ', 0) &&
                            engine.pullCommit() == encode(shortened) + " " &&
                            engine.preedit().empty(),
                        context + " commit differs");

                const std::string keys = vni ? "chua63n" : "chuaarn";
                for (char key : keys)
                    engine.processKey(key, 0);
                require(engine.preedit() == "chuẩn",
                        context + " subsequent Vietnamese composition");
                engine.requestRestore();
                engine.processKey(U' ', 1);
                require(engine.preedit() == keys,
                        context + " restore raw strokes");
                engine.reset();
                require(engine.preedit().empty() && engine.pullCommit().empty(),
                        context + " reset differs");
            }
            auto engine = make();
            for (size_t i = 0; i < count; ++i)
                engine.processKey(U'k', 0);
            engine.requestRestore();
            engine.processKey(U' ', 1);
            require(engine.preedit() == std::string(count, 'k'),
                    context + " long restore differs");
            for (size_t i = count; i > 0; --i) {
                require(engine.processKey(0xff08, 0) &&
                            engine.preedit() == std::string(i - 1, 'k'),
                        context + " delete to empty");
            }
            require(!engine.processKey(0xff08, 0),
                    context + " empty backspace passed through");
        }
    }
    Core core(ParseInputMethod(inputMethodDefinitions(), "Telex"));
    const std::string prefix = std::string(count, 'k') + " ";
    core.ProcessString(prefix, EnglishMode);
    core.ProcessString("chuaarn", VietnameseMode);
    require(core.GetProcessedString(VietnameseMode) == "chuẩn" &&
                core.GetProcessedString(PunctuationMode) == "chuẩn" &&
                core.GetProcessedString(VietnameseMode | FullText) ==
                    prefix + "chuẩn" &&
                core.GetProcessedString(EnglishMode | FullText) ==
                    prefix + "chuaarn",
            "Long composition word boundaries");
    core.RestoreLastWord(false);
    require(core.GetProcessedString(VietnameseMode | FullText) ==
                prefix + "chuaarn",
            "Long composition raw restore");
    core.RestoreLastWord(true);
    require(core.GetProcessedString(VietnameseMode | FullText) ==
                prefix + "chuẩn",
            "Long composition Vietnamese restore");
}
} // namespace
int main() {
    try {
        parserTests();
        utilityFixtures();
        coreFixtures();
        adapterFixtures();
        adapterTests();
        repeatedKeyTests();
        std::cout << "Native Bamboo tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
