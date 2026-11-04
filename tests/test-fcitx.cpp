/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <iostream>
#include <stdexcept>
#include <testfrontend_public.h>
#include <utility>

namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    using namespace fcitx;
    // Isolate configuration, data, and addon search paths from the desktop.
    setupTestingEnvironmentPath(BAMBOO_BINARY_DIR, {"src", FCITX_ADDON_DIR},
                                {"tests/data"});
    char arg0[] = "test-bamboo";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=bamboo,testfrontend";
    char *argv[] = {arg0, arg1, arg2};
    Instance instance(3, argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    int result = 1;
    instance.eventDispatcher().schedule([&]() {
        try {
            auto group = instance.inputMethodManager().currentGroup();
            group.inputMethodList().clear();
            group.inputMethodList().emplace_back("bamboo");
            group.setDefaultInputMethod("bamboo");
            instance.inputMethodManager().setGroup(std::move(group));
            auto *frontend = instance.addonManager().addon("testfrontend");
            require(frontend, "Test frontend did not load");
            require(instance.addonManager().addon("bamboo", true),
                    "Native Bamboo addon did not load");
            auto uuid = frontend->call<ITestFrontend::createInputContext>(
                "bamboo-native-test");
            auto *ic = instance.inputContextManager().findByUUID(uuid);
            require(ic, "Input context missing");
            ic->setCapabilityFlags(CapabilityFlag::Preedit);
            ic->focusIn();
            instance.setCurrentInputMethod(ic, "bamboo", true);
            require(instance.inputMethod(ic) == "bamboo",
                    "Bamboo not selected");
            for (char c : std::string("chuaarn")) {
                require(frontend->call<ITestFrontend::sendKeyEvent>(
                            uuid, Key(std::string(1, c)), false),
                        "Composition key was not accepted");
            }
            require(ic->inputPanel().clientPreedit().toString() == "chuẩn",
                    "Client preedit differs");
            frontend->call<ITestFrontend::pushCommitExpectation>("chuẩn ");
            require(frontend->call<ITestFrontend::sendKeyEvent>(
                        uuid, Key("space"), false),
                    "Commit space was not accepted");
            require(ic->inputPanel().clientPreedit().empty(),
                    "Preedit not cleared on commit");
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("a"), false);
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("w"), false);
            require(ic->inputPanel().clientPreedit().toString() == "ă",
                    "Second composition differs");
            frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("BackSpace"),
                                                        false);
            require(ic->inputPanel().clientPreedit().empty(),
                    "Backspace did not clear composed character");
            // A held key produces repeated presses (and, on some frontends,
            // interleaved releases). Every press must reach the composition.
            for (const auto &[key, expected] :
                 {std::make_pair("k", std::string(256, 'k')),
                  std::make_pair("a", std::string(255, 'a'))}) {
                for (int i = 0; i < 256; ++i) {
                    require(frontend->call<ITestFrontend::sendKeyEvent>(
                                uuid, Key(key), false),
                            "Repeated press was not accepted");
                    const auto beforeRelease =
                        ic->inputPanel().clientPreedit().toString();
                    frontend->call<ITestFrontend::sendKeyEvent>(uuid, Key(key),
                                                                true);
                    require(ic->inputPanel().clientPreedit().toString() ==
                                beforeRelease,
                            "Release changed repeated composition");
                }
                require(ic->inputPanel().clientPreedit().toString() == expected,
                        "Repeated preedit differs");
                frontend->call<ITestFrontend::pushCommitExpectation>(expected +
                                                                     " ");
                require(frontend->call<ITestFrontend::sendKeyEvent>(
                            uuid, Key("space"), false),
                        "Repeated commit was not accepted");
                require(ic->inputPanel().clientPreedit().empty(),
                        "Repeated preedit not cleared on commit");
            }
            instance.reloadAddonConfig("bamboo");
            frontend->call<ITestFrontend::destroyInputContext>(uuid);
            result = 0;
            std::cout << "Fcitx native addon load, composition, commit, "
                         "deletion, and reload passed\n";
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
        }
        instance.exit();
    });
    instance.exec();
    return result;
}
