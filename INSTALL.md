# Build and Install on Ubuntu 26.04

This guide targets Ubuntu 26.04 with Fcitx 5.1.19. The addon requires Fcitx
5.1.13 or newer. Go is not needed. Build from this modified source checkout;
Ubuntu's `fcitx5-bamboo` package contains the original engine.

## 1. Install dependencies

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
  gettext libicu-dev libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev \
  fcitx5-modules-dev fcitx5 fcitx5-config-qt im-config \
  fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 \
  fcitx5-frontend-qt5 fcitx5-frontend-qt6
```

Fcitx packages are in Ubuntu's `universe` repository. If apt cannot locate them,
enable it with `sudo add-apt-repository universe`, then repeat the commands.

## 2. Build and test

From the `vn-keyboard` workspace:

```sh
cd fcitx5-bamboo
cmake --fresh -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build-native --parallel 2
ctest --test-dir build-native --output-on-failure
```

Both `bamboo-native` and `bamboo-fcitx` should pass. The shared addon is
`build-native/src/libbamboo.so`. CMake detects Ubuntu's architecture-specific
library directory; do not copy the library to a guessed directory manually.

A source-only checkout of `fcitx5-bamboo` is sufficient: no Go submodule or sibling
repository is required. `-DBUILD_TESTING=OFF` disables tests and removes the
`fcitx5-modules-dev` requirement for production-only builds.

## 3. Install

If the distribution Bamboo package is installed, remove it first to avoid two
owners for the same addon files. This leaves your per-user configuration intact:

```sh
dpkg-query -W fcitx5-bamboo
# Only if the package is installed:
sudo apt remove fcitx5-bamboo
```

Install the native build:

```sh
sudo cmake --install build-native
```

To inspect the installation without changing the system instead, stage it:

```sh
DESTDIR="$PWD/build-native/stage" cmake --install build-native
```

Keep `build-native/install_manifest.txt`; it records the installed files for
removal. Native builds use the existing addon ID `bamboo` and existing files
under `~/.config/fcitx5/conf/`, including keymaps and per-method macros.

## 4. Enable Bamboo

For Ubuntu's default GNOME desktop or an X11 session:

```sh
im-config -n fcitx5
```

Log out completely and log in again. Launch `fcitx5-configtool`, click **+**,
uncheck **Only Show Current Language** if necessary, search for **Bamboo**, and
add it. Keep a keyboard layout in the input-method list. Select Bamboo using the
tray menu or your Fcitx switching shortcut (normally Ctrl+Space).

In Bamboo's settings, choose **Telex 2**. Test `chuaarn` → `chuẩn` and
`ddafi` → `đài`, then test space and backspace in a text editor.

### Wayland desktops

On GNOME, keep Fcitx's IBus frontend enabled for native Wayland applications.
Use `XMODIFIERS=@im=fcitx` for XWayland and `QT_IM_MODULE=fcitx` for Qt5 apps.
Native GTK apps can use the compositor's text-input support. If a specific GTK
application does not receive input, test launching it with
`GTK_IM_MODULE=fcitx application-name` after installing the GTK frontend.

If GNOME does not start Fcitx after login, add its desktop entry to autostart:

```sh
mkdir -p ~/.config/autostart
cp -n /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/
```

On KDE Plasma Wayland, select **Fcitx 5** in **System Settings → Virtual
Keyboard** and let Plasma launch it. Use desktop-default input-method selection
in `im-config`; follow KDE's guidance rather than applying GNOME's variables
globally.

See the upstream [setup guide](https://fcitx-im.org/wiki/Setup_Fcitx_5) and
[Wayland desktop guide](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland) for
session-specific setup and Chromium/Electron behavior.

## Optional EN/VI top bar and centered popup on GNOME 50

The companion extension displays **EN** or **VI** in the top bar and briefly
shows **English** or **Tiếng Việt** in the center of the focused app's monitor.
It also updates when switching between apps with different input languages.
It keeps your existing Fcitx shortcuts and per-app input state. The extension
targets GNOME Shell 50; other desktops can use Bamboo's normal Fcitx UI.

Install the dependencies in step 1 first. The companion UI also needs
`gnome-extensions` (provided by `gnome-shell`), and its client tests need GJS:

```sh
sudo apt install gnome-shell gjs dbus-daemon
```

Build the bridge and package the extension from `fcitx5-bamboo`. Explicitly set
the installation prefix even when configuring a new build directory:

```sh
cmake -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=ON -DENABLE_GNOME_INTEGRATION=ON
cmake --build build-native --parallel 2
ctest --test-dir build-native --output-on-failure
cmake --build build-native --target gnome-extension
sudo cmake --install build-native
gnome-extensions install --force \
  build-native/integrations/gnome/bamboo-indicator@fcitx5-bamboo.shell-extension.zip
```

This option requires the Fcitx DBus module development headers
(`fcitx5-modules-dev` on Ubuntu), even with testing disabled. Packaging needs
`gnome-extensions`. The bridge uses Fcitx's existing session D-Bus connection;
it has no polling process and does not intercept typing. The option defaults to
OFF and does not add dependencies to ordinary builds.

Log out and back in so GNOME discovers the extension and Fcitx loads the new
module, then enable it:

```sh
gnome-extensions enable bamboo-indicator@fcitx5-bamboo
```

Keep **Keyboard - English (US)** and **Bamboo** in your current Fcitx group.
The panel menu selects those entries and opens Bamboo settings. Other input
methods show their own Fcitx label and name. If the bridge is unavailable, the
extension shows a keyboard icon with disabled menu items, and reconnects when
Fcitx restarts. It does not need AppIndicator or Kimpanel to display this popup.

To show only the centered popup, open **Fcitx Configuration → Global Options**
and turn off **Show Input Method Information when switch input method**
(`Behavior/ShowInputMethodInformation`). Record its previous value. Existing
tray integrations can be hidden through their own settings if they duplicate
the new indicator; the extension does not modify other extensions.

To remove the companion UI:

```sh
gnome-extensions disable bamboo-indicator@fcitx5-bamboo
gnome-extensions uninstall bamboo-indicator@fcitx5-bamboo
```

Restore the switch-information setting to its previous value. The bridge can
be disabled in Fcitx's Addons settings (**Bamboo GNOME Indicator Bridge**).
Disabling the companion UI leaves Bamboo typing available.

Developer validation and the bridge interface are documented in
[`integrations/gnome/README.md`](integrations/gnome/README.md).

## Troubleshooting and removal

Run `fcitx5-diagnose` if Bamboo is missing or typing fails. Confirm that the addon
is enabled, the dictionary exists under `/usr/share/fcitx5/bamboo/`, and the
installed library has no missing dependencies with `ldd`. After installing a
new library, log out and back in so Fcitx loads it afresh. A running process keeps
its previous library loaded until it exits.

To remove a source installation, first inspect `build-native/install_manifest.txt`,
then remove exactly those files:

```sh
sudo xargs -d '\n' -r rm -- < build-native/install_manifest.txt
```

Do not use a manifest from a staging-only install to remove system files. To
return to Ubuntu's original Bamboo engine, run `sudo apt install fcitx5-bamboo`
after removal and log out/in. Your per-user settings remain available.
