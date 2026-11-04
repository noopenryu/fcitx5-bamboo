// SPDX-License-Identifier: LGPL-2.1-or-later
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import Pango from 'gi://Pango';
import St from 'gi://St';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Layout from 'resource:///org/gnome/shell/ui/layout.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';

import {IndicatorClient} from './client.js';
import {PopupTimer, presentState} from './state.js';

export default class BambooIndicator extends Extension {
    enable() {
        this._lastLanguage = null;
        this._indicator = new PanelMenu.Button(0.0, 'Keyboard language');
        const box = new St.BoxLayout();
        this._label = new St.Label({
            style_class: 'bamboo-panel-label',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._label.clutter_text.ellipsize = Pango.EllipsizeMode.END;
        this._icon = new St.Icon({
            icon_name: 'input-keyboard-symbolic',
            style_class: 'system-status-icon',
        });
        box.add_child(this._label);
        box.add_child(this._icon);
        this._indicator.add_child(box);
        this._english = new PopupMenu.PopupMenuItem('English');
        this._vietnamese = new PopupMenu.PopupMenuItem('Tiếng Việt');
        this._settings = new PopupMenu.PopupMenuItem('Bamboo Settings');
        this._indicator.menu.addMenuItem(this._english);
        this._indicator.menu.addMenuItem(this._vietnamese);
        this._indicator.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._indicator.menu.addMenuItem(this._settings);
        this._english.connect('activate', () => this._client.select('keyboard-us'));
        this._vietnamese.connect('activate', () => this._client.select('bamboo'));
        this._settings.connect('activate', () => this._client.configure());
        Main.panel.addToStatusArea(this.uuid, this._indicator);

        this._popup = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            reactive: false,
            visible: false,
        });
        this._monitor = new Layout.MonitorConstraint({primary: true});
        this._popup.add_constraint(this._monitor);
        const content = new St.BoxLayout({
            style_class: 'osd-window bamboo-language-popup',
            orientation: Clutter.Orientation.VERTICAL,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._popupLabel = new St.Label({
            style_class: 'bamboo-popup-label',
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._popupName = new St.Label({x_align: Clutter.ActorAlign.CENTER});
        content.add_child(this._popupLabel);
        content.add_child(this._popupName);
        this._popup.add_child(content);
        // All popup actors are non-reactive; never take a modal grab or focus.
        Main.layoutManager.addTopChrome(this._popup, {
            affectsStruts: false,
            trackFullscreen: false,
        });
        this._timer = new PopupTimer((delay, callback) =>
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, delay, () => {
                callback();
                return GLib.SOURCE_REMOVE;
            }), id => GLib.Source.remove(id), () => this._hidePopup(false));
        this._monitorsChanged = Main.layoutManager.connect('monitors-changed',
            () => this._positionPopup());
        this._sessionChanged = Main.sessionMode.connect('updated', () => {
            this._hidePopup();
            if (!Main.sessionMode.isLocked)
                this._client.sync();
        });
        this._client = new IndicatorClient(
            (state, silent) => this._update(state, silent),
            () => this._unavailable(), error => console.error(error));
        this._unavailable();
        this._client.start();
    }

    _update(state, silent) {
        const info = presentState(state);
        if (!info) {
            this._unavailable();
            return;
        }
        const changed = this._lastLanguage !== null && this._lastLanguage !== info.language;
        this._lastLanguage = info.language;
        this._label.text = info.label;
        this._label.show();
        this._icon.hide();
        this._indicator.accessible_name = `Keyboard language: ${info.name}`;
        this._english.setSensitive(true);
        this._vietnamese.setSensitive(true);
        this._settings.setSensitive(true);
        this._english.setOrnament(info.id === 'keyboard-us'
            ? PopupMenu.Ornament.DOT : PopupMenu.Ornament.NONE);
        this._vietnamese.setOrnament(info.id === 'bamboo'
            ? PopupMenu.Ornament.DOT : PopupMenu.Ornament.NONE);
        if (changed && !silent && !Main.sessionMode.isLocked) {
            this._popup.remove_all_transitions();
            this._popupLabel.text = info.label;
            this._popupName.text = info.name;
            this._popup.accessible_name = info.name;
            this._positionPopup();
            this._popup.opacity = 255;
            this._popup.show();
            this._timer.restart();
        }
    }

    _positionPopup() {
        const index = global.display.focus_window?.get_monitor() ?? -1;
        if (index >= 0 && index < Main.layoutManager.monitors.length) {
            this._monitor.primary = false;
            this._monitor.index = index;
        } else {
            this._monitor.primary = true;
        }
    }

    _hidePopup(immediate = true) {
        this._timer?.clear();
        this._popup.remove_all_transitions();
        if (immediate) {
            this._popup.hide();
        } else {
            this._popup.ease({
                opacity: 0,
                duration: 120,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._popup.hide(),
            });
        }
    }

    _unavailable() {
        this._lastLanguage = null;
        this._hidePopup();
        this._label.hide();
        this._icon.show();
        this._indicator.accessible_name = 'Fcitx keyboard unavailable';
        for (const item of [this._english, this._vietnamese, this._settings]) {
            item.setSensitive(false);
            item.setOrnament(PopupMenu.Ornament.NONE);
        }
    }

    disable() {
        this._client?.destroy();
        this._client = null;
        this._timer?.clear();
        this._timer = null;
        if (this._sessionChanged)
            Main.sessionMode.disconnect(this._sessionChanged);
        this._sessionChanged = 0;
        if (this._monitorsChanged)
            Main.layoutManager.disconnect(this._monitorsChanged);
        this._monitorsChanged = 0;
        this._popup?.remove_all_transitions();
        this._popup?.destroy();
        this._popup = null;
        this._monitor = null;
        this._indicator?.destroy();
        this._indicator = null;
        this._label = null;
        this._icon = null;
        this._english = null;
        this._vietnamese = null;
        this._settings = null;
        this._popupLabel = null;
        this._popupName = null;
        this._lastLanguage = null;
    }
}
