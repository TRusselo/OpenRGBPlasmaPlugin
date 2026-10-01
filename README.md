# OpenRGB Plasma Integration

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
- Until KDE ships plasma/powerdevil!691, Plasma only notices the backlight after a PowerDevil restart; the tab offers a button for it.
- Idle dimming and screen-off turn the lights off and back on, like a laptop keyboard.

## Build

    git clone --recursive https://github.com/TRusselo/OpenRGBPlasmaPlugin.git
    cd OpenRGBPlasmaPlugin
    mkdir build && cd build && qmake6 ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)"
    tests/plasma-integration-tests

## License

GPL-2.0-or-later
