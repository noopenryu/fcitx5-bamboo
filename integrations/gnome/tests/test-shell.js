// SPDX-License-Identifier: LGPL-2.1-or-later
// Run only in an isolated GNOME Shell using gnome-shell-test-tool --headless.
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

export const METRICS = {};

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

async function screenshot(name) {
    const directory = GLib.getenv('BAMBOO_SCREENSHOT_DIR');
    if (!directory)
        return;
    const stream = Gio.File.new_for_path(`${directory}/${name}.png`)
        .replace(null, false, Gio.FileCreateFlags.NONE, null);
    await new Shell.Screenshot().screenshot(false, stream);
    stream.close(null);
}

export async function run() {
    await Scripting.sleep(1000);
    const uuid = 'bamboo-indicator@fcitx5-bamboo';
    const extension = Main.extensionManager.lookup(uuid);
    assert(extension?.stateObj?._indicator, 'Extension failed to enable');
    const app = extension.stateObj;
    const en = ['keyboard-us', 'en', 'us', 'English'];
    const vi = ['bamboo', 'vi', 'VI', 'Bamboo'];
    // Deterministic UI tests independent of a desktop's Fcitx installation.
    app._client.destroy();
    app._update(en, true);
    assert(app._label.text === 'EN' && !app._popup.visible,
        'Initial synchronization must be silent');
    app._update(vi, false);
    await Scripting.sleep(100);
    assert(app._label.text === 'VI' && app._popup.visible, 'Vietnamese popup missing');
    assert(app._popupName.text === 'Tiếng Việt', 'Wrong popup title');
    const content = app._popup.get_first_child();
    const [x, y] = content.get_transformed_position();
    const [width, height] = content.get_transformed_size();
    const monitor = Main.layoutManager.primaryMonitor;
    print(`Popup ${x},${y} ${width}x${height}; monitor ${monitor.x},${monitor.y} ${monitor.width}x${monitor.height}`);
    await screenshot('vietnamese');
    assert(Math.abs(x + width / 2 - monitor.x - monitor.width / 2) < 2 &&
        Math.abs(y + height / 2 - monitor.y - monitor.height / 2) < 2,
    'Popup is not centered');
    const timer = app._timer._source;
    app._update(vi, false);
    assert(app._timer._source === timer, 'Duplicate language restarted the popup');
    app._update(en, false);
    await Scripting.sleep(100);
    await screenshot('english');
    assert(app._popupLabel.text === 'EN', 'Rapid switch left stale popup text');
    await Scripting.sleep(1200);
    assert(!app._popup.visible && app._timer._source === null,
        'Popup failed to dismiss and release its timer');
    const actors = Main.uiGroup.get_n_children();
    for (let i = 0; i < 100; i++) {
        app.disable();
        assert(!app._popup && !app._indicator && !app._timer && !app._client,
            'Disable retained owned resources');
        app.enable();
        app._client.destroy();
        app._update(en, true);
        app._update(vi, false);
        assert(Main.uiGroup.get_n_children() === actors, 'Enable accumulated actors');
    }
    app._unavailable();
    assert(!app._popup.visible && app._icon.visible && !app._english.sensitive,
        'Unavailable backend left an active language display');
    const settings = new Gio.Settings({schema_id: 'org.gnome.desktop.interface'});
    const scheme = settings.get_string('color-scheme');
    for (const theme of ['prefer-light', 'prefer-dark']) {
        settings.set_string('color-scheme', theme);
        await Scripting.sleep(400);
        app._update(en, true);
        app._update(vi, false);
        await Scripting.sleep(100);
        assert(app._popup.visible && app._label.text === 'VI',
            `Popup failed in ${theme}`);
        assert(app._popup.get_first_child().get_theme_node()
            .get_background_color().alpha > 0,
            `Popup lost its background in ${theme}`);
        const [themeWidth, themeHeight] = app._popup.get_first_child().get_transformed_size();
        assert(themeWidth >= 144 && themeHeight >= 100,
            `Popup dimensions collapsed in ${theme}`);
        await screenshot(`vietnamese-${theme}`);
    }
    settings.set_string('color-scheme', scheme);
    const stSettings = St.Settings.get();
    stSettings.inhibit_animations();
    try {
        assert(!stSettings.enable_animations, 'Animations were not disabled');
        app._update(en, true);
        app._update(vi, false);
        await Scripting.sleep(1000);
        assert(!app._popup.visible && app._timer._source === null &&
            !app._popup.get_transition('opacity'),
        'Reduced animations left a popup, timer, or animation');
    } finally {
        stSettings.uninhibit_animations();
    }
    app.disable();
    app.enable();
    print('GNOME panel, centered popup, dismissal, themes, reduced animations, and 100 lifecycle cycles passed');
}
