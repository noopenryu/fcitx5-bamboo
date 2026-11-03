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
