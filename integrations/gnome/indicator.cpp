/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <dbus_public.h>
#include <fcitx-utils/dbus/objectvtable.h>
#include <fcitx-utils/event.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/instance.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using State = std::tuple<std::string, std::string, std::string, std::string>;

class Indicator final : public fcitx::AddonInstance,
                        public fcitx::dbus::ObjectVTable<Indicator> {
public:
    explicit Indicator(fcitx::Instance *instance) : instance_(instance) {
        auto *dbus = instance_->addonManager().addon("dbus", true);
        auto *bus = dbus ? dbus->call<fcitx::IDBusModule::bus>() : nullptr;
        if (!bus ||
            !bus->addObjectVTable("/bamboo/indicator",
                                  "org.fcitx.Fcitx.Bamboo.Indicator1", *this)) {
            throw std::runtime_error("Cannot register Bamboo indicator bridge");
        }
        // One deferred callback observes the final state after Fcitx finishes
        // processing focus/group/activation events. No IC pointer is retained.
        update_ = instance_->eventLoop().addDeferEvent([this](auto *) {
            update_->setEnabled(false);
            publish();
            return true;
        });
        update_->setEnabled(false);
        for (auto type : {fcitx::EventType::InputContextInputMethodActivated,
                          fcitx::EventType::InputContextSwitchInputMethod,
                          fcitx::EventType::InputContextFocusIn,
                          fcitx::EventType::InputContextFocusOut,
                          fcitx::EventType::InputContextDestroyed,
                          fcitx::EventType::InputMethodGroupChanged}) {
            handlers_.push_back(instance_->watchEvent(
                type, fcitx::EventWatcherPhase::Default,
                [this](fcitx::Event &) { update_->setOneShot(); }));
        }
    }

    State getState() {
        auto *ic = instance_->mostRecentInputContext();
        const auto *entry = ic ? instance_->inputMethodEntry(ic) : nullptr;
        if (!entry) {
            return {};
        }
        return {entry->uniqueName(), entry->languageCode(),
                instance_->inputMethodLabel(ic), entry->name()};
    }

private:
    void publish() {
        auto state = getState();
        if (state == state_) {
            return;
        }
        state_ = std::move(state);
        std::apply([this](const auto &...value) { stateChanged(value...); },
                   state_);
    }

    fcitx::Instance *instance_;
    State state_;
    std::unique_ptr<fcitx::EventSource> update_;
    std::vector<std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>>>
        handlers_;
    FCITX_OBJECT_VTABLE_METHOD(getState, "GetState", "", "ssss");
    FCITX_OBJECT_VTABLE_SIGNAL(stateChanged, "StateChanged", "ssss");
};

class IndicatorFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new Indicator(manager->instance());
    }
};
} // namespace

FCITX_ADDON_FACTORY(IndicatorFactory)
