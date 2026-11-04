/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <dbus_public.h>
#include <deque>
#include <fcitx-utils/dbus/bus.h>
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonloader.h>
#include <fcitx/addonmanager.h>
#include <fcitx/focusgroup.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <testfrontend_public.h>
#include <utility>

namespace {
// The distribution keyboard engine is linked into the fcitx5 executable, not
// exported as a loadable addon. Supply a pass-through keyboard in this isolated
// instance; the real DBus module, bridge, and Bamboo engine are loaded
// normally.
class Keyboard final : public fcitx::InputMethodEngine {
public:
    std::vector<fcitx::InputMethodEntry> listInputMethods() override {
        std::vector<fcitx::InputMethodEntry> entries;
        entries.emplace_back("keyboard-us", "English", "en", "keyboard");
        entries.back().setLabel("EN").setIcon("input-keyboard");
        return entries;
    }
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &) override {
    }
};

class KeyboardFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *) override {
        return new Keyboard;
    }
};

void require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class Test {
public:
    explicit Test(fcitx::Instance &instance)
        : instance_(instance), client_(fcitx::dbus::BusType::Session),
          firstFocusGroup_("first-display", instance.inputContextManager()),
          secondFocusGroup_("second-display", instance.inputContextManager()) {
        client_.attachEventLoop(&instance_.eventLoop());
    }

    int result = 1;

    void start() {
        try {
            using namespace fcitx;
            require(instance_.addonManager().addon("bambooindicator"),
                    "Bridge did not load eagerly");
            frontend_ = instance_.addonManager().addon("testfrontend");
            require(frontend_, "Missing test frontend");
            auto *dbus = instance_.addonManager().addon("dbus");
            require(dbus, "Missing DBus module");
            destination_ = dbus->call<IDBusModule::bus>()->uniqueName();
            subscription_ = client_.addMatch(
                dbus::MatchRule(dbus::MessageType::Signal, destination_, "",
                                "/bamboo/indicator",
                                "org.fcitx.Fcitx.Bamboo.Indicator1",
                                "StateChanged"),
                [this](dbus::Message &message) {
                    std::string id, language, label, name;
                    message >> id >> language >> label >> name;
                    if (id == lastSignal_) {
                        duplicate_ = true;
                    }
                    lastSignal_ = id;
                    ++signals_;
                    return true;
                });
            auto group = instance_.inputMethodManager().currentGroup();
            group.inputMethodList().clear();
            group.inputMethodList().emplace_back("keyboard-us");
            group.inputMethodList().emplace_back("bamboo");
            group.setDefaultInputMethod("bamboo");
            instance_.inputMethodManager().setGroup(std::move(group));
            first_ =
                frontend_->call<ITestFrontend::createInputContext>("first");
            second_ =
                frontend_->call<ITestFrontend::createInputContext>("second");
            auto *first = context(first_);
            first->setFocusGroup(&firstFocusGroup_);
            context(second_)->setFocusGroup(&secondFocusGroup_);
            first->setCapabilityFlags(CapabilityFlag::Preedit);
            first->focusIn();
            instance_.setCurrentInputMethod(first, "keyboard-us", true);
            require(!instance_.addonManager().addon("bamboo"),
                    "English indicator eagerly loaded Bamboo");

            steps_.emplace_back([] {}, "keyboard-us");
            steps_.emplace_back([this] { select(first_, "bamboo"); }, "bamboo");
            steps_.emplace_back(
                [this] {
                    using namespace fcitx;
                    frontend_->call<ITestFrontend::sendKeyEvent>(
                        first_, Key("a"), false);
                    frontend_->call<ITestFrontend::sendKeyEvent>(
                        first_, Key("w"), false);
                    require(context(first_)
                                    ->inputPanel()
                                    .clientPreedit()
                                    .toString() == "ă",
                            "Composition before switching differs");
                    frontend_->call<ITestFrontend::pushCommitExpectation>("ă");
                    select(first_, "keyboard-us");
                    require(
                        context(first_)->inputPanel().clientPreedit().empty(),
                        "Switch failed to commit preedit");
                },
                "keyboard-us");
            steps_.emplace_back([this] { select(first_, "bamboo"); }, "bamboo");
            steps_.emplace_back(
                [this] {
                    context(first_)->focusOut();
                    context(second_)->focusIn();
                    select(second_, "keyboard-us");
                },
                "keyboard-us");
            steps_.emplace_back(
                [this] {
                    context(second_)->focusOut();
                    context(first_)->focusIn();
                },
                "bamboo");
            steps_.emplace_back(
                [this] {
                    // Separate focus groups can keep both contexts focused.
                    context(second_)->focusIn();
                    require(context(first_)->hasFocus() &&
                                context(second_)->hasFocus(),
                            "Both focus groups must retain focus");
                },
                "keyboard-us");
            steps_.emplace_back(
                [this] {
                    context(second_)->focusOut();
                    require(instance_.mostRecentInputContext() ==
                                context(first_),
                            "Focus out did not restore the older context");
                },
                "bamboo");
            steps_.emplace_back(
                [this] {
                    // GetState reads the current context directly; the panel
                    // needs the matching signal even without another focus-in.
                    require(
                        lastSignal_ == "bamboo",
                        "Focus out left the published indicator in English");
                },
                "bamboo");
            steps_.emplace_back(
                [this] {
                    // A background context must not change the panel language.
                    select(second_, "keyboard-us");
                },
                "bamboo");
            steps_.emplace_back(
                [this] {
                    for (int i = 0; i < 10000; ++i) {
                        select(first_, i % 2 ? "bamboo" : "keyboard-us");
                    }
                },
                "bamboo");
            steps_.emplace_back(
                [this] {
                    require(signals_ >= 5 && signals_ < 20,
                            "State signals missing or event burst was not "
                            "coalesced");
                    require(!duplicate_,
                            "Repeated state emitted duplicate signal");
                    frontend_->call<fcitx::ITestFrontend::destroyInputContext>(
                        first_);
                    context(second_)->focusIn();
                },
                "keyboard-us");
            steps_.emplace_back([this] { select(second_, "bamboo"); },
                                "bamboo");
            steps_.emplace_back(
                [this] {
                    auto &manager = instance_.inputMethodManager();
                    manager.addEmptyGroup("English only");
                    fcitx::InputMethodGroup group("English only");
                    group.inputMethodList().emplace_back("keyboard-us");
                    group.setDefaultLayout("us");
                    manager.setGroup(std::move(group));
                    manager.setCurrentGroup("English only");
                },
                "keyboard-us");
            steps_.emplace_back(
                [this] {
                    frontend_->call<fcitx::ITestFrontend::destroyInputContext>(
                        second_);
                },
                "");
            next();
        } catch (const std::exception &error) {
            fail(error);
        }
    }

private:
    fcitx::InputContext *context(fcitx::ICUUID uuid) {
        auto *ic = instance_.inputContextManager().findByUUID(uuid);
        require(ic, "Input context disappeared");
        return ic;
    }

    void select(fcitx::ICUUID uuid, const std::string &name) {
        instance_.setCurrentInputMethod(context(uuid), name, true);
    }

    void fail(const std::exception &error) {
        std::cerr << error.what() << '\n';
        instance_.exit();
    }

    void next() {
        try {
            if (steps_.empty()) {
                result = 0;
                std::cout
                    << "Bridge DBus, focus, composition, context destruction, "
                       "deduplication, and 10000 switches passed\n";
                instance_.exit();
                return;
            }
            auto [action, expected] = std::move(steps_.front());
            steps_.pop_front();
            action();
            auto message = client_.createMethodCall(
                destination_.c_str(), "/bamboo/indicator",
                "org.fcitx.Fcitx.Bamboo.Indicator1", "GetState");
            pending_ = message.callAsync(
                5000000, [this, expected](fcitx::dbus::Message &reply) {
                    try {
                        if (reply.isError()) {
                            throw std::runtime_error(reply.errorMessage());
                        }
                        std::string id, language, label, name;
                        reply >> id >> language >> label >> name;
                        require(id == expected,
                                "Expected " + expected + ", got " + id);
                        require(expected.empty() ||
                                    (!language.empty() && !label.empty() &&
                                     !name.empty()),
                                "Incomplete bridge state");
                        if (id == "bamboo") {
                            require(language == "vi" && label == "VI",
                                    "Bamboo language metadata differs");
                        }
                        instance_.eventDispatcher().schedule(
                            [this] { next(); });
                    } catch (const std::exception &error) {
                        fail(error);
                    }
                    return true;
                });
        } catch (const std::exception &error) {
            fail(error);
        }
    }

    fcitx::Instance &instance_;
    fcitx::dbus::Bus client_;
    fcitx::FocusGroup firstFocusGroup_, secondFocusGroup_;
    fcitx::AddonInstance *frontend_ = nullptr;
    fcitx::ICUUID first_, second_;
    std::string destination_, lastSignal_;
    size_t signals_ = 0;
    bool duplicate_ = false;
    std::unique_ptr<fcitx::dbus::Slot> subscription_, pending_;
    std::deque<std::pair<std::function<void()>, std::string>> steps_;
};
} // namespace

int main() {
    fcitx::setupTestingEnvironmentPath(
        BAMBOO_BINARY_DIR, {"src", "integrations/gnome", FCITX_ADDON_DIR},
        {"tests/data"});
    char name[] = "test-bamboo-indicator";
    char disable[] = "--disable=all";
    char enable[] =
        "--enable=bamboo,bambooindicator,dbus,keyboard,testfrontend";
    char *argv[] = {name, disable, enable};
    KeyboardFactory keyboardFactory;
    fcitx::StaticAddonRegistry registry{{"keyboard", &keyboardFactory}};
    fcitx::Instance instance(3, argv);
    instance.addonManager().registerDefaultLoader(&registry);
    Test test(instance);
    instance.eventDispatcher().schedule([&test] { test.start(); });
    instance.exec();
    return test.result;
}
