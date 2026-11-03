/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _FCITX5_BAMBOO_BAMBOO_H_
#define _FCITX5_BAMBOO_BAMBOO_H_

#include "adapter.h"
#include "bambooconfig.h"
#include <cstdint>
#include <fcitx-config/iniparser.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/signals.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fcitx {

class BambooState;

class BambooEngine final : public InputMethodEngine {
public:
    BambooEngine(Instance *instance);

    void activate(const InputMethodEntry &entry,
                  InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &entry,
                    fcitx::InputContextEvent &event) override;
    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
    void reset(const InputMethodEntry &entry,
               InputContextEvent &event) override;

    const auto &config() const { return config_; }
    const auto &customKeymap() const { return customKeymap_; }

    void reloadConfig() override;
    const Configuration *getConfig() const override { return &config_; }

    const Configuration *getSubConfig(const std::string &path) const override;

    void setConfig(const RawConfig &config) override;

    void setSubConfig(const std::string &path,
                      const RawConfig &config) override;
    std::string subMode(const fcitx::InputMethodEntry &entry,
                        fcitx::InputContext &inputContext) override;

    auto dictionary() const { return dictionary_; }
    auto macroTable() { return macroTableObject_.at(*config_.inputMethod); }

    void refreshEngine();
    void refreshOption();
    void saveConfig() { safeSaveAsIni(config_, "conf/bamboo.conf"); }
    void updateSpellAction(InputContext *ic);
    void updateMacroAction(InputContext *ic);
    void updateInputMethodAction(InputContext *ic);
    void updateCharsetAction(InputContext *ic);

    void populateConfig();

private:
    Instance *instance_;
    BambooConfig config_;
    BambooCustomKeymap customKeymap_;
    std::unordered_map<std::string, BambooMacroTable> macroTables_;
    std::unordered_map<std::string, std::shared_ptr<const bamboo::MacroTable>>
        macroTableObject_;
    FactoryFor<BambooState> factory_;
    std::vector<std::string> imNames_;
    std::unique_ptr<SimpleAction> inputMethodAction_;
    std::vector<std::unique_ptr<SimpleAction>> inputMethodSubAction_;
    std::unique_ptr<Menu> inputMethodMenu_;
    std::unique_ptr<SimpleAction> charsetAction_;
    std::vector<std::unique_ptr<SimpleAction>> charsetSubAction_;
    std::unique_ptr<Menu> charsetMenu_;
    std::unique_ptr<SimpleAction> spellCheckAction_;
    std::unique_ptr<SimpleAction> macroAction_;
    std::vector<ScopedConnection> connections_;
    std::shared_ptr<const bamboo::Dictionary> dictionary_;
};

class BambooFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        registerDomain("fcitx5-bamboo", FCITX_INSTALL_LOCALEDIR);
        return new BambooEngine(manager->instance());
    }
};
} // namespace fcitx

#endif // _FCITX5_BAMBOO_BAMBOO_H_
