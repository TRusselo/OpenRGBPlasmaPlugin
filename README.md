<p align="center"><img src="docs/images/icon.jpg" width="160" alt="OpenRGB Plasma Integration icon"></p>

# OpenRGB Plasma Integration

> **Status: waiting on upstream fixes.** The plugin works, but a few fixes it relies on have not been released yet:
>
> - **Home Assistant users:** openrgb-python 0.3.7 (used by HA's OpenRGB integration) fails to connect to any OpenRGB that has a plugin other than Effects loaded, including this one. Fix: [jath03/openrgb-python#95](https://github.com/jath03/openrgb-python/pull/95). Until it ships, loading this plugin cuts Home Assistant off.
> - **Home Assistant on OpenRGB 1.0:** colour and mode changes from HA are often dropped. Reported as [OpenRGB #5924](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/5924); client-side fix in [jath03/openrgb-python#96](https://github.com/jath03/openrgb-python/pull/96).
> - **Plasma:** PowerDevil only notices the backlight after a restart (the tab has a button for it). Fixed in [plasma/powerdevil!691](https://invent.kde.org/plasma/powerdevil/-/merge_requests/691), merged for Plasma 6.8.

An OpenRGB 1.0 plugin that lets KDE Plasma control your RGB lights:

- **Brightness**: Plasma's Brightness & Color widget gets a Keyboard Backlight slider that dims every OpenRGB device. Keyboard brightness keys and Plasma's idle dimming work too.
- **Accent color**: optionally, your lights follow Plasma's accent color.
- **Last actor wins**: if Home Assistant or the OpenRGB window changes a light, it shows exactly that until you move the slider again.

## Requirements

- OpenRGB 1.0 (plugin API 5), Linux
- A kernel with `uleds` (most distro kernels)
- KDE Plasma for the accent color and the slider

## Install

1. Download `libOpenRGBPlasmaPlugin.so` from the latest release.
2. OpenRGB → Settings → Plugins → Install Plugin, pick the file, restart OpenRGB.
3. Open the **Plasma Integration** tab and click **Set up** (one password prompt). It allows OpenRGB to create the keyboard backlight; nothing runs as root afterwards.

To do the setup by hand instead:

    echo uleds | sudo tee /etc/modules-load.d/openrgb-kbd-backlight.conf
    echo 'KERNEL=="uleds", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-openrgb-kbd-backlight.rules
    sudo modprobe uleds
    sudo udevadm control --reload
    sudo udevadm trigger --name-match=uleds

Then restart OpenRGB: the plugin only creates the backlight when it loads or after its own **Set up** succeeds.

## Known limitations

- The Plasma slider does not move when something else changes brightness.
- Before Plasma 6.8 (plasma/powerdevil!691), Plasma only notices the backlight after a PowerDevil restart; the tab offers a button for it.
- Idle dimming and screen-off turn the lights off and back on, like a laptop keyboard.

## Build

    git clone --recursive https://github.com/TRusselo/OpenRGBPlasmaPlugin.git
    cd OpenRGBPlasmaPlugin
    mkdir build && cd build && qmake6 ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)"
    tests/plasma-integration-tests

## License

GPL-2.0-or-later
