# OpenRGB Plasma Integration Plugin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An OpenRGB 1.0 plugin that lets KDE Plasma's Keyboard Backlight slider dim OpenRGB lights and lets them follow Plasma's accent color, with "last actor wins" so other OpenRGB clients (Home Assistant) read back exactly what is shown.

**Architecture:** One Qt plugin (`libOpenRGBPlasmaPlugin.so`). Pure, unit-tested logic (`Dimming`, `LightingEngine`, `PluginSettings`, `DeviceKey`, `StatusText`) sits behind a small `Light` interface; thin adapters connect it to OpenRGB (`OpenRGBLight`), the kernel (`UledsBacklight`), Plasma (`AccentColorSource`, `PowerDevilProbe`), root setup (`SystemSetup`), and the UI (`SettingsTab`). Device writes go through a coalescing `DeviceWriter` on a worker thread.

**Tech Stack:** C++17, Qt 6 (Core, Gui, Widgets, DBus, Test), qmake, OpenRGB `release_1.0` headers (git submodule), Linux `uleds`, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-01-plasma-integration-design.md`

## Global Constraints

- OpenRGB plugin API 5 only (OpenRGB `release_1.0`); metadata `OpenRGBPluginAPIVersion: 5`, IID `org.openrgb.OpenRGBPluginInterface`.
- Linux only. Dependencies: Qt 6 Core/Gui/Widgets/DBus (+Test for tests) and OpenRGB headers. No KDE Frameworks.
- C++17. Code comments: one line or none.
- License GPL-2.0-or-later. Repository `TRusselo/OpenRGBPlasmaPlugin`; plugin name and tab label "Plasma Integration".
- Backlight LED `openrgb::kbd_backlight`, `max_brightness` 100, via `/dev/uleds`.
- Settings key `PlasmaIntegrationPlugin`: `{ "accent_enabled": bool (default false), "devices": { "<key>": { "dim": bool (default true), "accent": bool (default true) } } }`.
- Device key: `name|serial` when the trimmed serial is non-empty and not `none` (any case), else `name|location`.
- At most one write per device every ~30 ms; the latest target wins.
- Setup files: `/etc/modules-load.d/openrgb-kbd-backlight.conf` (`uleds`), `/etc/udev/rules.d/70-openrgb-kbd-backlight.rules` (`KERNEL=="uleds", TAG+="uaccess"`), run through `pkexec`.

## Review Focus

- A device with no zones or LEDs (some controllers report none): rendering, accent and level changes must not crash or write. Tests: `TestDimming::renderHandlesEmptyLight` (Task 2), `TestLightingEngine::emptyLightIsHarmless` (Task 4).
- Device list refreshed while a write for a removed device is pending: the write must be dropped, never applied to a stale controller. Test: `TestDeviceWriter::dropsWritesForRemovedLights` (Task 5).
- Malformed `kdeglobals` (garbage lines, half headers, bad numbers): the reader must fall back per Kameleon's rule, not crash or return garbage. Test: `TestAccentColorSource::garbageIsIgnored` (Task 7).
- A level arriving before OpenRGB has finished detecting devices: devices that appear later must take the current level. Test: `TestLightingEngine::levelBeforeLightsAppliesToNewLights` (Task 4).
- Serials like `none`, `NONE`, blank, padded; settings JSON with wrong types: stable keys and defaults. Tests: `TestDeviceKey::*`, `TestPluginSettings::wrongTypesFallBackToDefaults` (Task 3).

## File Structure

```
OpenRGBPlasmaPlugin/
├── OpenRGBPlasmaPlugin.pro        subdirs: src, tests
├── common.pri                     C++17, include paths shared by src and tests
├── OpenRGB/                       git submodule, pinned to release_1.0
├── LICENSE                        GPL-2.0 (copied from OpenRGB)
├── README.md
├── scripts/
│   ├── dev-run.sh                 runs a release_1.0 OpenRGB build with the plugin, isolated config
│   └── sdk_check.py               reads/sets devices over the SDK for end-to-end checks
├── .github/workflows/
│   ├── build.yml                  build + tests on every push
│   └── release.yml                attach the .so to a GitHub Release on tags
├── src/
│   ├── src.pro
│   ├── PlasmaIntegrationPlugin.json   plugin metadata
│   ├── PlasmaIntegrationPlugin.h/.cpp OpenRGB plugin entry point, wiring
│   ├── LightModel.h               LightState / ZoneState / ModeState, Rgb helpers
│   ├── LightMetaType.h            Q_DECLARE_METATYPE(LightState)
│   ├── Light.h                    abstract Light (read/apply)
│   ├── Dimming.h/.cpp             renderAtLevel, withAccent, scaling
│   ├── DeviceKey.h/.cpp           stable device key
│   ├── PluginSettings.h/.cpp      settings struct <-> JSON
│   ├── LightingEngine.h/.cpp      last-actor-wins rules
│   ├── DeviceWriter.h/.cpp        coalescing writer (worker thread)
│   ├── OpenRGBLight.h/.cpp        Light adapter over RGBControllerInterface
│   ├── UledsBacklight.h/.cpp      /dev/uleds LED
│   ├── AccentColorSource.h/.cpp   Plasma accent color
│   ├── SystemSetup.h/.cpp         uleds access detection + pkexec setup
│   ├── PowerDevilProbe.h/.cpp     does PowerDevil see the backlight
│   ├── StatusText.h/.cpp          status line text
│   └── SettingsTab.h/.cpp         the plugin tab
└── tests/
    ├── tests.pro
    ├── main.cpp                   runs every test class, optional class filter
    ├── LightBuilders.h            test helpers + QTest toString for LightState
    ├── FakeLight.h                in-memory Light
    └── Test*.h/.cpp               one class per unit
```

Build and test commands used throughout (run from the repo root):

```bash
mkdir -p build && (cd build && qmake6 ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)")
build/tests/plasma-integration-tests            # all test classes
build/tests/plasma-integration-tests TestDimming # one class
```

---

### Task 1: Project skeleton that builds a loadable plugin

**Files:**
- Create: `.gitignore`, `LICENSE`, `OpenRGBPlasmaPlugin.pro`, `common.pri`, `src/src.pro`, `src/PlasmaIntegrationPlugin.json`, `src/PlasmaIntegrationPlugin.h`, `src/PlasmaIntegrationPlugin.cpp`, `tests/tests.pro`, `tests/main.cpp`, `tests/TestPluginMetadata.h`, `tests/TestPluginMetadata.cpp`
- Submodule: `OpenRGB` at tag `release_1.0`

**Interfaces:**
- Produces: `PlasmaIntegrationPlugin` (QObject + `OpenRGBPluginInterface`), test runner `plasma-integration-tests [ClassName]`, macros `PLUGIN_VERSION`, `GIT_COMMIT_ID`, `PLUGIN_LIBRARY_PATH` (tests).

- [ ] **Step 1: Add the OpenRGB submodule pinned to release_1.0 and the license**

```bash
cd ~/git/OpenRGBPlasmaPlugin
git submodule add https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB
git -C OpenRGB fetch origin tag release_1.0
git -C OpenRGB checkout release_1.0
grep -n 'OPENRGB_PLUGIN_API_VERSION' OpenRGB/OpenRGBPluginInterface.h
cp OpenRGB/LICENSE LICENSE
printf 'build/\n*.user\n' > .gitignore
```

Expected: the grep prints `#define OPENRGB_PLUGIN_API_VERSION  5`.

- [ ] **Step 2: Write the project files and the failing metadata test**

`OpenRGBPlasmaPlugin.pro`:

```qmake
TEMPLATE = subdirs
SUBDIRS = src tests
tests.depends = src
```

`common.pri`:

```qmake
CONFIG += c++17 warn_on
INCLUDEPATH += $$PWD/src $$PWD/OpenRGB/dependencies/json
```

`src/src.pro` (sources are added by later tasks):

```qmake
include(../common.pri)

TEMPLATE = lib
CONFIG += plugin
TARGET = OpenRGBPlasmaPlugin
QT += core gui widgets dbus

PLUGIN_VERSION = 0.1.0
GIT_COMMIT_ID = $$system(git -C $$PWD/.. rev-parse --short HEAD)

DEFINES += \
    PLUGIN_VERSION=\\\"$$PLUGIN_VERSION\\\" \
    GIT_COMMIT_ID=\\\"$$GIT_COMMIT_ID\\\"

INCLUDEPATH += \
    $$PWD/../OpenRGB \
    $$PWD/../OpenRGB/RGBController \
    $$PWD/../OpenRGB/qt

HEADERS += \
    PlasmaIntegrationPlugin.h

SOURCES += \
    PlasmaIntegrationPlugin.cpp

DISTFILES += \
    PlasmaIntegrationPlugin.json
```

`tests/tests.pro`:

```qmake
include(../common.pri)

TEMPLATE = app
TARGET = plasma-integration-tests
CONFIG += console
CONFIG -= app_bundle
QT += core gui widgets dbus testlib

DEFINES += PLUGIN_LIBRARY_PATH=\\\"$$OUT_PWD/../src/libOpenRGBPlasmaPlugin.so\\\"

HEADERS += \
    TestPluginMetadata.h

SOURCES += \
    main.cpp \
    TestPluginMetadata.cpp
```

`tests/main.cpp`:

```cpp
#include <memory>
#include <vector>

#include <QApplication>
#include <QTest>

#include "TestPluginMetadata.h"

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::vector<char*> args(argv, argv + argc);
    QString filter;
    if(args.size() > 1 && args[1][0] != '-')
    {
        filter = QString::fromLocal8Bit(args[1]);
        args.erase(args.begin() + 1);
    }

    std::vector<std::unique_ptr<QObject>> tests;
    tests.emplace_back(new TestPluginMetadata);

    int status = 0;
    for(const std::unique_ptr<QObject>& test : tests)
    {
        if(filter.isEmpty() || filter == QLatin1String(test->metaObject()->className()))
        {
            status |= QTest::qExec(test.get(), int(args.size()), args.data());
        }
    }
    return status;
}
```

`tests/TestPluginMetadata.h`:

```cpp
#pragma once

#include <QObject>

class TestPluginMetadata : public QObject
{
    Q_OBJECT

private slots:
    void declaresOpenRGBApiFive();
};
```

`tests/TestPluginMetadata.cpp`:

```cpp
#include "TestPluginMetadata.h"

#include <QJsonObject>
#include <QPluginLoader>
#include <QTest>

void TestPluginMetadata::declaresOpenRGBApiFive()
{
    QPluginLoader loader(QStringLiteral(PLUGIN_LIBRARY_PATH));
    const QJsonObject root = loader.metaData();
    const QJsonObject metadata = root.value(QStringLiteral("MetaData")).toObject();

    QCOMPARE(root.value(QStringLiteral("IID")).toString(), QStringLiteral("org.openrgb.OpenRGBPluginInterface"));
    QCOMPARE(metadata.value(QStringLiteral("OpenRGBPluginAPIVersion")).toInt(), 5);
    QCOMPARE(metadata.value(QStringLiteral("Id")).toString(), QStringLiteral("io.github.trusselo.openrgbplasmaplugin"));
    QCOMPARE(metadata.value(QStringLiteral("Name")).toString(), QStringLiteral("Plasma Integration"));
}
```

Create `src/PlasmaIntegrationPlugin.json` empty for now so the build can start:

```json
{}
```

- [ ] **Step 3: Run the test to see it fail**

Run: `mkdir -p build && (cd build && qmake6 ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)"); build/tests/plasma-integration-tests TestPluginMetadata`
Expected: the build of `src` fails (no `PlasmaIntegrationPlugin.h`), or the test FAILs on `IID` with an empty string.

- [ ] **Step 4: Write the plugin stub and metadata**

`src/PlasmaIntegrationPlugin.json`:

```json
{
    "Id": "io.github.trusselo.openrgbplasmaplugin",
    "Name": "Plasma Integration",
    "VendorId": "trusselo",
    "Version": "0.1.0",
    "VersionStr": "0.1.0",
    "Url": "https://github.com/TRusselo/OpenRGBPlasmaPlugin",
    "Commit": "",
    "Description": "Control OpenRGB lights from KDE Plasma's keyboard backlight slider and accent color",
    "OpenRGBPluginAPIVersion": 5
}
```

`src/PlasmaIntegrationPlugin.h`:

```cpp
#pragma once

#include <QObject>

#include "OpenRGBPluginInterface.h"

class PlasmaIntegrationPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "PlasmaIntegrationPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~PlasmaIntegrationPlugin() override = default;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override;
    void Load(OpenRGBPluginAPIInterface* plugin_api) override;
    QWidget* GetWidget() override;
    QMenu* GetTrayMenu() override;
    void Unload() override;
    void OnProfileAboutToLoad() override;
    void OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void ProfileManagerUpdated(unsigned int update_reason) override;
    void ResourceManagerUpdated(unsigned int update_reason) override;
    void SettingsManagerUpdated(unsigned int update_reason) override;

private:
    OpenRGBPluginAPIInterface* api = nullptr;
    QWidget* tab = nullptr;
};
```

`src/PlasmaIntegrationPlugin.cpp`:

```cpp
#include "PlasmaIntegrationPlugin.h"

#include <QLabel>

OpenRGBPluginInfo PlasmaIntegrationPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name = "Plasma Integration";
    info.Description = "Control OpenRGB lights from KDE Plasma's keyboard backlight slider and accent color";
    info.Version = PLUGIN_VERSION;
    info.Commit = GIT_COMMIT_ID;
    info.URL = "https://github.com/TRusselo/OpenRGBPlasmaPlugin";
    info.Location = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Label = "Plasma Integration";
    info.ProtocolVersion = 0;
    return info;
}

unsigned int PlasmaIntegrationPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void PlasmaIntegrationPlugin::Load(OpenRGBPluginAPIInterface* plugin_api)
{
    api = plugin_api;
    tab = new QLabel(QStringLiteral("Plasma Integration"));
}

QWidget* PlasmaIntegrationPlugin::GetWidget()
{
    return tab;
}

QMenu* PlasmaIntegrationPlugin::GetTrayMenu()
{
    return nullptr;
}

void PlasmaIntegrationPlugin::Unload()
{
}

void PlasmaIntegrationPlugin::OnProfileAboutToLoad()
{
}

void PlasmaIntegrationPlugin::OnProfileLoad(nlohmann::json)
{
}

nlohmann::json PlasmaIntegrationPlugin::OnProfileSave()
{
    return nlohmann::json();
}

unsigned char* PlasmaIntegrationPlugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int* pkt_size)
{
    if(pkt_size)
    {
        *pkt_size = 0;
    }
    return nullptr;
}

void PlasmaIntegrationPlugin::ProfileManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::ResourceManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::SettingsManagerUpdated(unsigned int)
{
}
```

- [ ] **Step 5: Run the test to see it pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestPluginMetadata`
Expected: `Totals: 3 passed, 0 failed` (initTestCase, the test, cleanupTestCase) and `build/src/libOpenRGBPlasmaPlugin.so` exists.

- [ ] **Step 6: Commit**

```bash
git add .gitignore .gitmodules OpenRGB LICENSE OpenRGBPlasmaPlugin.pro common.pri src tests
git commit -m "Add plugin skeleton for OpenRGB plugin API 5"
```

---

### Task 2: Light model and dimming

**Files:**
- Create: `src/LightModel.h`, `src/Dimming.h`, `src/Dimming.cpp`, `tests/LightBuilders.h`, `tests/TestDimming.h`, `tests/TestDimming.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Produces:
  - `using Rgb = unsigned int;` (OpenRGB layout `0x00BBGGRR`), `Rgb makeRgb(unsigned r, unsigned g, unsigned b)`, `rgbRed/rgbGreen/rgbBlue(Rgb)`
  - `enum class ColorMode { None = 0, PerLed = 1, ModeSpecific = 2, Random = 3 }`
  - `struct ModeState { int index = -1; ColorMode colorMode; bool hasBrightness; unsigned brightness, brightnessMin, brightnessMax; std::vector<Rgb> colors; }` (`index -1` on a zone = zone follows the device mode)
  - `struct ZoneState { ModeState mode; std::vector<Rgb> leds; }`, `struct LightState { ModeState mode; std::vector<ZoneState> zones; }`, with `==`/`!=`
  - `Rgb scaleColor(Rgb, int level)`, `unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level)`, `LightState renderAtLevel(const LightState&, int level)`, `LightState withAccent(const LightState&, Rgb)`
  - Tests: `LightBuilders.h` with `perLedMode`, `staticMode`, `offMode`, `brightnessMode`, `directLight`, and `toString(const LightState&)`

- [ ] **Step 1: Write the model, builders and the failing tests**

`src/LightModel.h`:

```cpp
#pragma once

#include <vector>

using Rgb = unsigned int;

inline Rgb makeRgb(unsigned int red, unsigned int green, unsigned int blue)
{
    return (blue << 16) | (green << 8) | red;
}

inline unsigned int rgbRed(Rgb color)
{
    return color & 0xFF;
}

inline unsigned int rgbGreen(Rgb color)
{
    return (color >> 8) & 0xFF;
}

inline unsigned int rgbBlue(Rgb color)
{
    return (color >> 16) & 0xFF;
}

enum class ColorMode
{
    None = 0,
    PerLed = 1,
    ModeSpecific = 2,
    Random = 3,
};

struct ModeState
{
    int index = -1;
    ColorMode colorMode = ColorMode::None;
    bool hasBrightness = false;
    unsigned int brightness = 0;
    unsigned int brightnessMin = 0;
    unsigned int brightnessMax = 0;
    std::vector<Rgb> colors;
};

struct ZoneState
{
    ModeState mode;
    std::vector<Rgb> leds;
};

struct LightState
{
    ModeState mode;
    std::vector<ZoneState> zones;
};

inline bool operator==(const ModeState& a, const ModeState& b)
{
    return a.index == b.index && a.colorMode == b.colorMode && a.hasBrightness == b.hasBrightness && a.brightness == b.brightness
        && a.brightnessMin == b.brightnessMin && a.brightnessMax == b.brightnessMax && a.colors == b.colors;
}

inline bool operator!=(const ModeState& a, const ModeState& b)
{
    return !(a == b);
}

inline bool operator==(const ZoneState& a, const ZoneState& b)
{
    return a.mode == b.mode && a.leds == b.leds;
}

inline bool operator!=(const ZoneState& a, const ZoneState& b)
{
    return !(a == b);
}

inline bool operator==(const LightState& a, const LightState& b)
{
    return a.mode == b.mode && a.zones == b.zones;
}

inline bool operator!=(const LightState& a, const LightState& b)
{
    return !(a == b);
}
```

`src/Dimming.h`:

```cpp
#pragma once

#include "LightModel.h"

Rgb scaleColor(Rgb color, int level);
unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level);
LightState renderAtLevel(const LightState& base, int level);
LightState withAccent(const LightState& base, Rgb accent);
```

`src/Dimming.cpp` (stub so the tests compile and fail):

```cpp
#include "Dimming.h"

Rgb scaleColor(Rgb color, int)
{
    return color;
}

unsigned int scaleBrightness(unsigned int brightness, unsigned int, int)
{
    return brightness;
}

LightState renderAtLevel(const LightState& base, int)
{
    return base;
}

LightState withAccent(const LightState& base, Rgb)
{
    return base;
}
```

`tests/LightBuilders.h`:

```cpp
#pragma once

#include <cstring>
#include <utility>
#include <vector>

#include <QString>

#include "LightModel.h"

inline ModeState perLedMode(int index)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::PerLed;
    return mode;
}

inline ModeState staticMode(int index, Rgb color)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::ModeSpecific;
    mode.colors = {color};
    return mode;
}

inline ModeState offMode(int index)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::None;
    return mode;
}

inline ModeState brightnessMode(int index, unsigned int brightness, unsigned int minimum, unsigned int maximum)
{
    ModeState mode;
    mode.index = index;
    mode.colorMode = ColorMode::None;
    mode.hasBrightness = true;
    mode.brightness = brightness;
    mode.brightnessMin = minimum;
    mode.brightnessMax = maximum;
    return mode;
}

inline LightState directLight(std::vector<Rgb> leds)
{
    LightState state;
    state.mode = perLedMode(0);
    ZoneState zone;
    zone.leds = std::move(leds);
    state.zones.push_back(zone);
    return state;
}

inline LightState lightWithMode(const ModeState& mode, std::vector<Rgb> leds)
{
    LightState state;
    state.mode = mode;
    ZoneState zone;
    zone.leds = std::move(leds);
    state.zones.push_back(zone);
    return state;
}

inline char* toString(const LightState& state)
{
    QString text = QStringLiteral("mode %1 colorMode %2 brightness %3").arg(state.mode.index).arg(int(state.mode.colorMode)).arg(state.mode.brightness);
    for(Rgb color : state.mode.colors)
    {
        text += QStringLiteral(" m%1").arg(color, 6, 16, QLatin1Char('0'));
    }
    for(const ZoneState& zone : state.zones)
    {
        text += QStringLiteral(" | zone mode %1").arg(zone.mode.index);
        for(Rgb color : zone.mode.colors)
        {
            text += QStringLiteral(" m%1").arg(color, 6, 16, QLatin1Char('0'));
        }
        for(Rgb color : zone.leds)
        {
            text += QStringLiteral(" %1").arg(color, 6, 16, QLatin1Char('0'));
        }
    }
    return qstrdup(text.toUtf8().constData());
}
```

`tests/TestDimming.h`:

```cpp
#pragma once

#include <QObject>

class TestDimming : public QObject
{
    Q_OBJECT

private slots:
    void scaleColorKeepsColorAtFullLevel();
    void scaleColorIsBlackAtZero();
    void scaleColorRoundsEachChannel();
    void scaleBrightnessMovesTowardsMinimum();
    void scaleBrightnessHandlesInvertedRange();
    void renderScalesPerLedColors();
    void renderScalesModeSpecificColors();
    void renderScalesBrightnessOfColorlessModes();
    void renderLeavesOffModeUnchanged();
    void renderUsesZoneModeWhenSet();
    void renderHandlesEmptyLight();
    void accentPaintsPerLedAndModeColors();
    void accentLeavesColorlessModesUnchanged();
};
```

`tests/TestDimming.cpp`:

```cpp
#include "TestDimming.h"

#include <QTest>

#include "Dimming.h"
#include "LightBuilders.h"

void TestDimming::scaleColorKeepsColorAtFullLevel()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 100), makeRgb(255, 128, 1));
}

void TestDimming::scaleColorIsBlackAtZero()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 0), makeRgb(0, 0, 0));
}

void TestDimming::scaleColorRoundsEachChannel()
{
    QCOMPARE(scaleColor(makeRgb(255, 128, 1), 50), makeRgb(128, 64, 1));
    QCOMPARE(scaleColor(makeRgb(255, 0, 0), 40), makeRgb(102, 0, 0));
}

void TestDimming::scaleBrightnessMovesTowardsMinimum()
{
    QCOMPARE(scaleBrightness(80, 10, 50), 45u);
    QCOMPARE(scaleBrightness(80, 10, 0), 10u);
    QCOMPARE(scaleBrightness(80, 10, 100), 80u);
}

void TestDimming::scaleBrightnessHandlesInvertedRange()
{
    QCOMPARE(scaleBrightness(20, 100, 50), 60u);
    QCOMPARE(scaleBrightness(20, 100, 0), 100u);
}

void TestDimming::renderScalesPerLedColors()
{
    const LightState base = directLight({makeRgb(200, 100, 50), makeRgb(10, 20, 30)});
    QCOMPARE(renderAtLevel(base, 50), directLight({makeRgb(100, 50, 25), makeRgb(5, 10, 15)}));
}

void TestDimming::renderScalesModeSpecificColors()
{
    const LightState base = lightWithMode(staticMode(1, makeRgb(255, 0, 0)), {makeRgb(9, 9, 9)});
    QCOMPARE(renderAtLevel(base, 40), lightWithMode(staticMode(1, makeRgb(102, 0, 0)), {makeRgb(9, 9, 9)}));
}

void TestDimming::renderScalesBrightnessOfColorlessModes()
{
    const LightState base = lightWithMode(brightnessMode(2, 80, 10, 100), {});
    QCOMPARE(renderAtLevel(base, 50), lightWithMode(brightnessMode(2, 45, 10, 100), {}));
}

void TestDimming::renderLeavesOffModeUnchanged()
{
    const LightState base = lightWithMode(offMode(3), {makeRgb(50, 60, 70)});
    QCOMPARE(renderAtLevel(base, 0), base);
}

void TestDimming::renderUsesZoneModeWhenSet()
{
    LightState base;
    base.mode = perLedMode(0);
    ZoneState own;
    own.mode = staticMode(4, makeRgb(0, 0, 200));
    own.leds = {makeRgb(50, 50, 50)};
    ZoneState follower;
    follower.leds = {makeRgb(100, 100, 100)};
    base.zones = {own, follower};

    LightState expected = base;
    expected.zones[0].mode = staticMode(4, makeRgb(0, 0, 100));
    expected.zones[1].leds = {makeRgb(50, 50, 50)};

    QCOMPARE(renderAtLevel(base, 50), expected);
}

void TestDimming::renderHandlesEmptyLight()
{
    const LightState empty;
    QCOMPARE(renderAtLevel(empty, 30), empty);
    QCOMPARE(withAccent(empty, makeRgb(1, 2, 3)), empty);
}

void TestDimming::accentPaintsPerLedAndModeColors()
{
    const Rgb accent = makeRgb(61, 174, 233);
    QCOMPARE(withAccent(directLight({makeRgb(1, 1, 1), makeRgb(2, 2, 2)}), accent), directLight({accent, accent}));
    QCOMPARE(withAccent(lightWithMode(staticMode(1, makeRgb(255, 0, 0)), {makeRgb(9, 9, 9)}), accent),
             lightWithMode(staticMode(1, accent), {makeRgb(9, 9, 9)}));
}

void TestDimming::accentLeavesColorlessModesUnchanged()
{
    const LightState off = lightWithMode(offMode(3), {makeRgb(50, 60, 70)});
    QCOMPARE(withAccent(off, makeRgb(61, 174, 233)), off);
}
```

Register the new files. In `src/src.pro` add `LightModel.h Dimming.h` to `HEADERS` and `Dimming.cpp` to `SOURCES`. In `tests/tests.pro` add:

```qmake
HEADERS += \
    LightBuilders.h \
    TestDimming.h \
    ../src/LightModel.h \
    ../src/Dimming.h

SOURCES += \
    TestDimming.cpp \
    ../src/Dimming.cpp
```

In `tests/main.cpp` add `#include "TestDimming.h"` and `tests.emplace_back(new TestDimming);` after the metadata test.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDimming`
Expected: FAIL in `scaleColorIsBlackAtZero` and the other scaling/render tests (the stubs return their input).

- [ ] **Step 3: Implement dimming**

Replace `src/Dimming.cpp`:

```cpp
#include "Dimming.h"

namespace
{
unsigned int scaleChannel(unsigned int value, int level)
{
    return (value * (unsigned int)level + 50) / 100;
}

void scaleMode(ModeState& mode, int level)
{
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(Rgb& color : mode.colors)
        {
            color = scaleColor(color, level);
        }
    }
    else if((mode.colorMode == ColorMode::None || mode.colorMode == ColorMode::Random) && mode.hasBrightness)
    {
        mode.brightness = scaleBrightness(mode.brightness, mode.brightnessMin, level);
    }
}

void paintMode(ModeState& mode, Rgb accent)
{
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(Rgb& color : mode.colors)
        {
            color = accent;
        }
    }
}
}

Rgb scaleColor(Rgb color, int level)
{
    return makeRgb(scaleChannel(rgbRed(color), level), scaleChannel(rgbGreen(color), level), scaleChannel(rgbBlue(color), level));
}

unsigned int scaleBrightness(unsigned int brightness, unsigned int minimum, int level)
{
    const int delta = int(brightness) - int(minimum);
    const int scaled = (delta * level + (delta >= 0 ? 50 : -50)) / 100;
    return (unsigned int)(int(minimum) + scaled);
}

LightState renderAtLevel(const LightState& base, int level)
{
    LightState target = base;
    scaleMode(target.mode, level);
    for(ZoneState& zone : target.zones)
    {
        const ColorMode effective = (zone.mode.index >= 0) ? zone.mode.colorMode : target.mode.colorMode;
        if(effective == ColorMode::PerLed)
        {
            for(Rgb& led : zone.leds)
            {
                led = scaleColor(led, level);
            }
        }
        if(zone.mode.index >= 0)
        {
            scaleMode(zone.mode, level);
        }
    }
    return target;
}

LightState withAccent(const LightState& base, Rgb accent)
{
    LightState result = base;
    paintMode(result.mode, accent);
    for(ZoneState& zone : result.zones)
    {
        const ColorMode effective = (zone.mode.index >= 0) ? zone.mode.colorMode : result.mode.colorMode;
        if(effective == ColorMode::PerLed)
        {
            for(Rgb& led : zone.leds)
            {
                led = accent;
            }
        }
        if(zone.mode.index >= 0)
        {
            paintMode(zone.mode, accent);
        }
    }
    return result;
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDimming`
Expected: all 13 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add light model and dimming rules"
```

---

### Task 3: Device keys and plugin settings

**Files:**
- Create: `src/DeviceKey.h`, `src/DeviceKey.cpp`, `src/PluginSettings.h`, `src/PluginSettings.cpp`, `tests/TestDeviceKey.h`, `tests/TestDeviceKey.cpp`, `tests/TestPluginSettings.h`, `tests/TestPluginSettings.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Produces:
  - `std::string deviceKey(const std::string& name, const std::string& serial, const std::string& location)`
  - `struct DeviceOptions { bool dim = true; bool accent = true; };`
  - `struct PluginSettings { bool accentEnabled = false; std::map<std::string, DeviceOptions> devices; DeviceOptions optionsFor(const std::string& key) const; static PluginSettings fromJson(const nlohmann::json&); nlohmann::json toJson() const; }`
  - `inline const char* const SettingsKey = "PlasmaIntegrationPlugin";` in `PluginSettings.h`

- [ ] **Step 1: Write the failing tests and stub headers**

`src/DeviceKey.h`:

```cpp
#pragma once

#include <string>

std::string deviceKey(const std::string& name, const std::string& serial, const std::string& location);
```

`src/DeviceKey.cpp` (stub):

```cpp
#include "DeviceKey.h"

std::string deviceKey(const std::string& name, const std::string&, const std::string&)
{
    return name;
}
```

`src/PluginSettings.h`:

```cpp
#pragma once

#include <map>
#include <string>

#include <nlohmann/json.hpp>

inline const char* const SettingsKey = "PlasmaIntegrationPlugin";

struct DeviceOptions
{
    bool dim = true;
    bool accent = true;
};

struct PluginSettings
{
    bool accentEnabled = false;
    std::map<std::string, DeviceOptions> devices;

    DeviceOptions optionsFor(const std::string& key) const;
    static PluginSettings fromJson(const nlohmann::json& json);
    nlohmann::json toJson() const;
};
```

`src/PluginSettings.cpp` (stub):

```cpp
#include "PluginSettings.h"

DeviceOptions PluginSettings::optionsFor(const std::string&) const
{
    return DeviceOptions();
}

PluginSettings PluginSettings::fromJson(const nlohmann::json&)
{
    return PluginSettings();
}

nlohmann::json PluginSettings::toJson() const
{
    return nlohmann::json();
}
```

`tests/TestDeviceKey.h`:

```cpp
#pragma once

#include <QObject>

class TestDeviceKey : public QObject
{
    Q_OBJECT

private slots:
    void usesSerialWhenPresent();
    void trimsSerial();
    void fallsBackToLocationForNone();
    void fallsBackToLocationForBlankSerial();
};
```

`tests/TestDeviceKey.cpp`:

```cpp
#include "TestDeviceKey.h"

#include <QTest>

#include "DeviceKey.h"

void TestDeviceKey::usesSerialWhenPresent()
{
    QCOMPARE(QString::fromStdString(deviceKey("Razer Goliathus", "PM1234", "HID: /dev/hidraw7")), QStringLiteral("Razer Goliathus|PM1234"));
}

void TestDeviceKey::trimsSerial()
{
    QCOMPARE(QString::fromStdString(deviceKey("Razer Goliathus", "  PM1234 \n", "HID: /dev/hidraw7")), QStringLiteral("Razer Goliathus|PM1234"));
}

void TestDeviceKey::fallsBackToLocationForNone()
{
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "none", "I2C: /dev/i2c-3, 0x60")), QStringLiteral("Fan|I2C: /dev/i2c-3, 0x60"));
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "NONE", "I2C: /dev/i2c-3, 0x60")), QStringLiteral("Fan|I2C: /dev/i2c-3, 0x60"));
}

void TestDeviceKey::fallsBackToLocationForBlankSerial()
{
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "", "HID: /dev/hidraw2")), QStringLiteral("Fan|HID: /dev/hidraw2"));
    QCOMPARE(QString::fromStdString(deviceKey("Fan", "   ", "HID: /dev/hidraw2")), QStringLiteral("Fan|HID: /dev/hidraw2"));
}
```

`tests/TestPluginSettings.h`:

```cpp
#pragma once

#include <QObject>

class TestPluginSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenEmpty();
    void readsStoredValues();
    void wrongTypesFallBackToDefaults();
    void roundTripsThroughJson();
};
```

`tests/TestPluginSettings.cpp`:

```cpp
#include "TestPluginSettings.h"

#include <QTest>

#include "PluginSettings.h"

void TestPluginSettings::defaultsWhenEmpty()
{
    const PluginSettings settings = PluginSettings::fromJson(nlohmann::json());
    QCOMPARE(settings.accentEnabled, false);
    QCOMPARE(settings.optionsFor("anything").dim, true);
    QCOMPARE(settings.optionsFor("anything").accent, true);
}

void TestPluginSettings::readsStoredValues()
{
    const nlohmann::json json = {{"accent_enabled", true}, {"devices", {{"Fan|1", {{"dim", false}, {"accent", true}}}}}};
    const PluginSettings settings = PluginSettings::fromJson(json);
    QCOMPARE(settings.accentEnabled, true);
    QCOMPARE(settings.optionsFor("Fan|1").dim, false);
    QCOMPARE(settings.optionsFor("Fan|1").accent, true);
}

void TestPluginSettings::wrongTypesFallBackToDefaults()
{
    const nlohmann::json json = {{"accent_enabled", "yes"}, {"devices", {{"Fan|1", {{"dim", 0}, {"accent", "no"}}}, {"Bad|2", 7}}}};
    const PluginSettings settings = PluginSettings::fromJson(json);
    QCOMPARE(settings.accentEnabled, false);
    QCOMPARE(settings.optionsFor("Fan|1").dim, true);
    QCOMPARE(settings.optionsFor("Fan|1").accent, true);
    QCOMPARE(settings.optionsFor("Bad|2").dim, true);
    QCOMPARE(PluginSettings::fromJson(nlohmann::json::array()).accentEnabled, false);
}

void TestPluginSettings::roundTripsThroughJson()
{
    PluginSettings settings;
    settings.accentEnabled = true;
    settings.devices["Keyboard|ABC"] = DeviceOptions{false, false};
    const PluginSettings copy = PluginSettings::fromJson(settings.toJson());
    QCOMPARE(copy.accentEnabled, true);
    QCOMPARE(copy.optionsFor("Keyboard|ABC").dim, false);
    QCOMPARE(copy.optionsFor("Keyboard|ABC").accent, false);
}
```

Register: `src/src.pro` add `DeviceKey.h PluginSettings.h` to `HEADERS`, `DeviceKey.cpp PluginSettings.cpp` to `SOURCES`. `tests/tests.pro` add `TestDeviceKey.h TestPluginSettings.h ../src/DeviceKey.h ../src/PluginSettings.h` to `HEADERS` and `TestDeviceKey.cpp TestPluginSettings.cpp ../src/DeviceKey.cpp ../src/PluginSettings.cpp` to `SOURCES`. `tests/main.cpp`: include both headers and add `tests.emplace_back(new TestDeviceKey);` and `tests.emplace_back(new TestPluginSettings);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDeviceKey; build/tests/plasma-integration-tests TestPluginSettings`
Expected: FAIL (`deviceKey` returns only the name; settings never store anything).

- [ ] **Step 3: Implement both**

`src/DeviceKey.cpp`:

```cpp
#include "DeviceKey.h"

#include <algorithm>
#include <cctype>

std::string deviceKey(const std::string& name, const std::string& serial, const std::string& location)
{
    const char* const whitespace = " \t\r\n";
    std::string trimmed;
    const std::size_t first = serial.find_first_not_of(whitespace);
    if(first != std::string::npos)
    {
        trimmed = serial.substr(first, serial.find_last_not_of(whitespace) - first + 1);
    }

    std::string lowered = trimmed;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    if(trimmed.empty() || lowered == "none")
    {
        return name + "|" + location;
    }
    return name + "|" + trimmed;
}
```

`src/PluginSettings.cpp`:

```cpp
#include "PluginSettings.h"

namespace
{
void readBool(const nlohmann::json& object, const char* key, bool& value)
{
    if(object.contains(key) && object.at(key).is_boolean())
    {
        value = object.at(key).get<bool>();
    }
}
}

DeviceOptions PluginSettings::optionsFor(const std::string& key) const
{
    const auto found = devices.find(key);
    return found == devices.end() ? DeviceOptions() : found->second;
}

PluginSettings PluginSettings::fromJson(const nlohmann::json& json)
{
    PluginSettings settings;
    if(!json.is_object())
    {
        return settings;
    }

    readBool(json, "accent_enabled", settings.accentEnabled);

    if(json.contains("devices") && json.at("devices").is_object())
    {
        for(auto entry = json.at("devices").begin(); entry != json.at("devices").end(); ++entry)
        {
            if(!entry.value().is_object())
            {
                continue;
            }
            DeviceOptions options;
            readBool(entry.value(), "dim", options.dim);
            readBool(entry.value(), "accent", options.accent);
            settings.devices[entry.key()] = options;
        }
    }
    return settings;
}

nlohmann::json PluginSettings::toJson() const
{
    nlohmann::json json;
    json["accent_enabled"] = accentEnabled;
    json["devices"] = nlohmann::json::object();
    for(const auto& [key, options] : devices)
    {
        json["devices"][key] = {{"dim", options.dim}, {"accent", options.accent}};
    }
    return json;
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDeviceKey && build/tests/plasma-integration-tests TestPluginSettings`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add stable device keys and plugin settings"
```

---

### Task 4: Lighting engine (last actor wins)

**Files:**
- Create: `src/Light.h`, `src/LightingEngine.h`, `src/LightingEngine.cpp`, `tests/FakeLight.h`, `tests/TestLightingEngine.h`, `tests/TestLightingEngine.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Consumes: `LightState`, `renderAtLevel`, `withAccent` (Task 2); `PluginSettings`, `DeviceOptions` (Task 3).
- Produces:
  - `class Light { virtual std::string key() const; virtual std::string name() const; virtual LightState read() const; virtual void apply(const LightState& target); }` (all pure virtual)
  - `class LightingEngine` with `using WriteRequest = std::function<void(const std::string& key, const LightState& target)>;`, `explicit LightingEngine(WriteRequest)`, `void setLights(const std::vector<std::shared_ptr<Light>>&)`, `void setSettings(const PluginSettings&)`, `void setLevel(int)`, `void setAccent(std::optional<Rgb>)`, `void onLightChanged(const std::string& key)`, `void onWriteFinished(const std::string& key, const LightState& applied)`, `int level() const`, `bool follows(const std::string&) const`, `std::optional<LightState> base(const std::string&) const`
  - Tests: `FakeLight(std::string key, LightState state)` with public `state` and `applyCount`

Rules implemented (spec §6): level change → every Dim-ticked light follows and renders `base × level`; outside change → `base := state`, stop following, no write; accent → Accent-ticked lights get the accent as base, follow if Dim-ticked; accent off → nothing; Dim untick → base at full; Dim tick → follow; new light → base = state, follows = Dim, rendered at current level; own writes ignored (in flight, or state equals expected).

- [ ] **Step 1: Write `Light`, the engine header with a stub, the fake and the failing tests**

`src/Light.h`:

```cpp
#pragma once

#include <string>

#include "LightModel.h"

class Light
{
public:
    virtual ~Light() = default;
    virtual std::string key() const = 0;
    virtual std::string name() const = 0;
    virtual LightState read() const = 0;
    virtual void apply(const LightState& target) = 0;
};
```

`src/LightingEngine.h`:

```cpp
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Light.h"
#include "PluginSettings.h"

class LightingEngine
{
public:
    using WriteRequest = std::function<void(const std::string& key, const LightState& target)>;

    explicit LightingEngine(WriteRequest writeRequest);

    void setLights(const std::vector<std::shared_ptr<Light>>& lights);
    void setSettings(const PluginSettings& settings);
    void setLevel(int level);
    void setAccent(std::optional<Rgb> accent);
    void onLightChanged(const std::string& key);
    void onWriteFinished(const std::string& key, const LightState& applied);

    int level() const;
    bool follows(const std::string& key) const;
    std::optional<LightState> base(const std::string& key) const;

private:
    struct Entry
    {
        std::shared_ptr<Light> light;
        LightState base;
        bool follows = false;
        bool writing = false;
        std::optional<LightState> expected;
    };

    void render(const std::string& key, Entry& entry);
    static void adoptOutsideState(Entry& entry, const LightState& state);

    WriteRequest requestWrite;
    std::map<std::string, Entry> entries;
    PluginSettings settings;
    std::optional<Rgb> accent;
    int currentLevel = 100;
};
```

`src/LightingEngine.cpp` (stub):

```cpp
#include "LightingEngine.h"

LightingEngine::LightingEngine(WriteRequest writeRequest)
    : requestWrite(std::move(writeRequest))
{
}

void LightingEngine::setLights(const std::vector<std::shared_ptr<Light>>&)
{
}

void LightingEngine::setSettings(const PluginSettings& newSettings)
{
    settings = newSettings;
}

void LightingEngine::setLevel(int)
{
}

void LightingEngine::setAccent(std::optional<Rgb> newAccent)
{
    accent = newAccent;
}

void LightingEngine::onLightChanged(const std::string&)
{
}

void LightingEngine::onWriteFinished(const std::string&, const LightState&)
{
}

int LightingEngine::level() const
{
    return currentLevel;
}

bool LightingEngine::follows(const std::string&) const
{
    return false;
}

std::optional<LightState> LightingEngine::base(const std::string&) const
{
    return std::nullopt;
}

void LightingEngine::render(const std::string&, Entry&)
{
}

void LightingEngine::adoptOutsideState(Entry&, const LightState&)
{
}
```

`tests/FakeLight.h`:

```cpp
#pragma once

#include <string>
#include <utility>

#include "Light.h"

class FakeLight : public Light
{
public:
    FakeLight(std::string lightKey, LightState initial)
        : keyValue(std::move(lightKey))
        , state(std::move(initial))
    {
    }

    std::string key() const override
    {
        return keyValue;
    }

    std::string name() const override
    {
        return keyValue;
    }

    LightState read() const override
    {
        return state;
    }

    void apply(const LightState& target) override
    {
        state = target;
        applyCount++;
    }

    std::string keyValue;
    LightState state;
    int applyCount = 0;
};
```

`tests/TestLightingEngine.h`:

```cpp
#pragma once

#include <QObject>

class TestLightingEngine : public QObject
{
    Q_OBJECT

private slots:
    void startupAtFullLevelWritesNothing();
    void levelDimsLightsTickedForDim();
    void levelBeforeLightsAppliesToNewLights();
    void levelIsClamped();
    void levelZeroThenUpRestoresExactly();
    void outsideChangeBecomesBaseAndStopsFollowing();
    void levelAfterOutsideChangeDimsNewBase();
    void ownWritesAreIgnored();
    void outsideWriteDuringOwnWriteIsDetected();
    void supersededWriteKeepsWaiting();
    void accentPaintsTickedLightsAtLevel();
    void accentLeavesOffModeAlone();
    void accentOffWritesNothing();
    void untickingDimRestoresFullBase();
    void tickingDimFollowsSlider();
    void tickingAccentAppliesCurrentAccent();
    void removedLightIsForgotten();
    void emptyLightIsHarmless();
};
```

`tests/TestLightingEngine.cpp`:

```cpp
#include "TestLightingEngine.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QTest>

#include "FakeLight.h"
#include "LightBuilders.h"
#include "LightingEngine.h"

namespace
{
class Harness
{
public:
    Harness()
        : engine([this](const std::string& key, const LightState& target) { requests.push_back({key, target}); })
    {
    }

    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    std::shared_ptr<FakeLight> add(const std::string& key, const LightState& state)
    {
        auto light = std::make_shared<FakeLight>(key, state);
        lights[key] = light;
        publish();
        return light;
    }

    void remove(const std::string& key)
    {
        lights.erase(key);
        publish();
    }

    void completeWrites()
    {
        const auto pending = requests;
        requests.clear();
        for(const auto& [key, target] : pending)
        {
            lights[key]->apply(target);
            engine.onLightChanged(key);
            engine.onWriteFinished(key, target);
        }
    }

    void outsideChange(const std::string& key, const LightState& state)
    {
        lights[key]->state = state;
        engine.onLightChanged(key);
    }

    std::vector<std::pair<std::string, LightState>> requests;
    std::map<std::string, std::shared_ptr<FakeLight>> lights;
    LightingEngine engine;

private:
    void publish()
    {
        std::vector<std::shared_ptr<Light>> all;
        for(const auto& [key, light] : lights)
        {
            all.push_back(light);
        }
        engine.setLights(all);
    }
};

const Rgb Orange = makeRgb(200, 100, 0);
const Rgb Red = makeRgb(255, 0, 0);
const Rgb Accent = makeRgb(61, 174, 233);
}

void TestLightingEngine::startupAtFullLevelWritesNothing()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    QCOMPARE(int(h.requests.size()), 0);
    QCOMPARE(h.engine.follows("keyboard"), true);
}

void TestLightingEngine::levelDimsLightsTickedForDim()
{
    Harness h;
    PluginSettings settings;
    settings.devices["mat"] = DeviceOptions{false, true};
    h.engine.setSettings(settings);
    h.add("keyboard", directLight({Orange}));
    h.add("mat", directLight({Orange}));

    h.engine.setLevel(50);

    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(QString::fromStdString(h.requests[0].first), QStringLiteral("keyboard"));
    QCOMPARE(h.requests[0].second, directLight({makeRgb(100, 50, 0)}));
}

void TestLightingEngine::levelBeforeLightsAppliesToNewLights()
{
    Harness h;
    h.engine.setLevel(40);
    h.add("keyboard", directLight({Red}));
    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(h.requests[0].second, directLight({makeRgb(102, 0, 0)}));
}

void TestLightingEngine::levelIsClamped()
{
    Harness h;
    h.engine.setLevel(-5);
    QCOMPARE(h.engine.level(), 0);
    h.engine.setLevel(250);
    QCOMPARE(h.engine.level(), 100);
}

void TestLightingEngine::levelZeroThenUpRestoresExactly()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(0);
    h.completeWrites();
    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(0, 0, 0)}));
    h.engine.setLevel(100);
    h.completeWrites();
    QCOMPARE(h.lights["keyboard"]->state, directLight({Orange}));
}

void TestLightingEngine::outsideChangeBecomesBaseAndStopsFollowing()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();

    h.outsideChange("keyboard", directLight({Red}));

    QCOMPARE(int(h.requests.size()), 0);
    QCOMPARE(h.engine.follows("keyboard"), false);
    QCOMPARE(*h.engine.base("keyboard"), directLight({Red}));
    QCOMPARE(h.lights["keyboard"]->state, directLight({Red}));
}

void TestLightingEngine::levelAfterOutsideChangeDimsNewBase()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();
    h.outsideChange("keyboard", directLight({Red}));

    h.engine.setLevel(60);

    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(h.requests[0].second, directLight({makeRgb(153, 0, 0)}));
    QCOMPARE(h.engine.follows("keyboard"), true);
}

void TestLightingEngine::ownWritesAreIgnored()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();
    h.engine.onLightChanged("keyboard");

    QCOMPARE(h.engine.follows("keyboard"), true);
    QCOMPARE(*h.engine.base("keyboard"), directLight({Orange}));
}

void TestLightingEngine::outsideWriteDuringOwnWriteIsDetected()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    const LightState target = h.requests[0].second;
    h.requests.clear();

    h.lights["keyboard"]->apply(target);
    h.lights["keyboard"]->state = directLight({Red});
    h.engine.onLightChanged("keyboard");
    h.engine.onWriteFinished("keyboard", target);

    QCOMPARE(h.engine.follows("keyboard"), false);
    QCOMPARE(*h.engine.base("keyboard"), directLight({Red}));
}

void TestLightingEngine::supersededWriteKeepsWaiting()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    const LightState first = h.requests[0].second;
    h.engine.setLevel(20);
    const LightState second = h.requests[1].second;
    h.requests.clear();

    h.lights["keyboard"]->apply(first);
    h.engine.onWriteFinished("keyboard", first);
    h.engine.onLightChanged("keyboard");
    QCOMPARE(h.engine.follows("keyboard"), true);

    h.lights["keyboard"]->apply(second);
    h.engine.onWriteFinished("keyboard", second);
    h.engine.onLightChanged("keyboard");
    QCOMPARE(h.engine.follows("keyboard"), true);
    QCOMPARE(*h.engine.base("keyboard"), directLight({Orange}));
}

void TestLightingEngine::accentPaintsTickedLightsAtLevel()
{
    Harness h;
    PluginSettings settings;
    settings.devices["fan"] = DeviceOptions{false, true};
    h.engine.setSettings(settings);
    h.add("keyboard", directLight({Orange}));
    h.add("fan", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();

    h.engine.setAccent(Accent);
    h.completeWrites();

    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(31, 87, 117)}));
    QCOMPARE(h.lights["fan"]->state, directLight({Accent}));
    QCOMPARE(h.engine.follows("keyboard"), true);
    QCOMPARE(h.engine.follows("fan"), false);
}

void TestLightingEngine::accentLeavesOffModeAlone()
{
    Harness h;
    const LightState off = lightWithMode(offMode(3), {Orange});
    h.add("mat", off);
    h.engine.setAccent(Accent);
    QCOMPARE(int(h.requests.size()), 0);
    QCOMPARE(h.lights["mat"]->state, off);
}

void TestLightingEngine::accentOffWritesNothing()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setAccent(std::nullopt);
    QCOMPARE(int(h.requests.size()), 0);
}

void TestLightingEngine::untickingDimRestoresFullBase()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();

    PluginSettings settings;
    settings.devices["keyboard"] = DeviceOptions{false, true};
    h.engine.setSettings(settings);
    h.completeWrites();

    QCOMPARE(h.lights["keyboard"]->state, directLight({Orange}));
    QCOMPARE(h.engine.follows("keyboard"), false);
}

void TestLightingEngine::tickingDimFollowsSlider()
{
    Harness h;
    PluginSettings off;
    off.devices["keyboard"] = DeviceOptions{false, true};
    h.engine.setSettings(off);
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 0);

    h.engine.setSettings(PluginSettings());
    h.completeWrites();

    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(100, 50, 0)}));
    QCOMPARE(h.engine.follows("keyboard"), true);
}

void TestLightingEngine::tickingAccentAppliesCurrentAccent()
{
    Harness h;
    PluginSettings noAccent;
    noAccent.devices["keyboard"] = DeviceOptions{true, false};
    h.engine.setSettings(noAccent);
    h.add("keyboard", directLight({Orange}));
    h.engine.setAccent(Accent);
    QCOMPARE(int(h.requests.size()), 0);

    h.engine.setSettings(PluginSettings());
    h.completeWrites();

    QCOMPARE(h.lights["keyboard"]->state, directLight({Accent}));
}

void TestLightingEngine::removedLightIsForgotten()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.remove("keyboard");
    h.engine.onLightChanged("keyboard");
    h.engine.onWriteFinished("keyboard", directLight({Orange}));
    h.engine.setLevel(10);
    QCOMPARE(int(h.requests.size()), 0);
    QVERIFY(!h.engine.base("keyboard").has_value());
}

void TestLightingEngine::emptyLightIsHarmless()
{
    Harness h;
    h.add("empty", LightState());
    h.engine.setLevel(20);
    h.engine.setAccent(Accent);
    h.engine.onLightChanged("empty");
    QCOMPARE(int(h.requests.size()), 0);
}
```

Register: `src/src.pro` add `Light.h LightingEngine.h` to `HEADERS`, `LightingEngine.cpp` to `SOURCES`. `tests/tests.pro` add `FakeLight.h TestLightingEngine.h ../src/Light.h ../src/LightingEngine.h` to `HEADERS`, `TestLightingEngine.cpp ../src/LightingEngine.cpp` to `SOURCES`. `tests/main.cpp`: include `TestLightingEngine.h`, add `tests.emplace_back(new TestLightingEngine);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestLightingEngine`
Expected: FAIL in most tests (stub engine never writes and has no state).

- [ ] **Step 3: Implement the engine**

Replace `src/LightingEngine.cpp`:

```cpp
#include "LightingEngine.h"

#include <algorithm>

#include "Dimming.h"

LightingEngine::LightingEngine(WriteRequest writeRequest)
    : requestWrite(std::move(writeRequest))
{
}

void LightingEngine::setLights(const std::vector<std::shared_ptr<Light>>& lights)
{
    std::map<std::string, Entry> updated;
    std::vector<std::string> added;
    for(const std::shared_ptr<Light>& light : lights)
    {
        const std::string key = light->key();
        const auto existing = entries.find(key);
        if(existing != entries.end())
        {
            Entry entry = existing->second;
            entry.light = light;
            updated[key] = entry;
            continue;
        }
        Entry entry;
        entry.light = light;
        entry.base = light->read();
        entry.follows = settings.optionsFor(key).dim;
        updated[key] = entry;
        added.push_back(key);
    }
    entries = std::move(updated);
    for(const std::string& key : added)
    {
        render(key, entries[key]);
    }
}

void LightingEngine::setSettings(const PluginSettings& newSettings)
{
    const PluginSettings old = settings;
    settings = newSettings;
    for(auto& [key, entry] : entries)
    {
        const DeviceOptions before = old.optionsFor(key);
        const DeviceOptions after = settings.optionsFor(key);
        bool changed = false;
        if(after.accent && !before.accent && accent)
        {
            entry.base = withAccent(entry.base, *accent);
            changed = true;
        }
        if(after.dim != before.dim || changed)
        {
            entry.follows = after.dim;
            changed = true;
        }
        if(changed)
        {
            render(key, entry);
        }
    }
}

void LightingEngine::setLevel(int level)
{
    currentLevel = std::clamp(level, 0, 100);
    for(auto& [key, entry] : entries)
    {
        if(settings.optionsFor(key).dim)
        {
            entry.follows = true;
            render(key, entry);
        }
    }
}

void LightingEngine::setAccent(std::optional<Rgb> newAccent)
{
    accent = newAccent;
    if(!accent)
    {
        return;
    }
    for(auto& [key, entry] : entries)
    {
        const DeviceOptions options = settings.optionsFor(key);
        if(!options.accent)
        {
            continue;
        }
        entry.base = withAccent(entry.base, *accent);
        entry.follows = options.dim;
        render(key, entry);
    }
}

void LightingEngine::onLightChanged(const std::string& key)
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return;
    }
    Entry& entry = found->second;
    if(entry.writing)
    {
        return;
    }
    const LightState state = entry.light->read();
    if(entry.expected && state == *entry.expected)
    {
        return;
    }
    adoptOutsideState(entry, state);
}

void LightingEngine::onWriteFinished(const std::string& key, const LightState& applied)
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return;
    }
    Entry& entry = found->second;
    if(!entry.expected || applied != *entry.expected)
    {
        return;
    }
    entry.writing = false;
    const LightState state = entry.light->read();
    if(state != *entry.expected)
    {
        adoptOutsideState(entry, state);
    }
}

int LightingEngine::level() const
{
    return currentLevel;
}

bool LightingEngine::follows(const std::string& key) const
{
    const auto found = entries.find(key);
    return found != entries.end() && found->second.follows;
}

std::optional<LightState> LightingEngine::base(const std::string& key) const
{
    const auto found = entries.find(key);
    if(found == entries.end())
    {
        return std::nullopt;
    }
    return found->second.base;
}

void LightingEngine::render(const std::string& key, Entry& entry)
{
    const LightState target = entry.follows ? renderAtLevel(entry.base, currentLevel) : entry.base;
    entry.expected = target;
    if(!entry.writing && entry.light->read() == target)
    {
        return;
    }
    entry.writing = true;
    requestWrite(key, target);
}

void LightingEngine::adoptOutsideState(Entry& entry, const LightState& state)
{
    entry.base = state;
    entry.follows = false;
    entry.expected.reset();
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestLightingEngine`
Expected: all 18 tests PASS. Then run everything: `build/tests/plasma-integration-tests` — all PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add lighting engine with last-actor-wins rules"
```

---

### Task 5: Coalescing device writer

**Files:**
- Create: `src/LightMetaType.h`, `src/DeviceWriter.h`, `src/DeviceWriter.cpp`, `tests/TestDeviceWriter.h`, `tests/TestDeviceWriter.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Consumes: `Light`, `LightState`.
- Produces: `Q_DECLARE_METATYPE(LightState)`; `class DeviceWriter : public QObject` with `explicit DeviceWriter(int intervalMs = 30, QObject* parent = nullptr)`, `void setLights(const std::vector<std::shared_ptr<Light>>&)`, slot `void enqueue(const QString& key, const LightState& target)`, signal `void written(const QString& key, const LightState& applied)`. Callers must `qRegisterMetaType<LightState>("LightState")` once.

- [ ] **Step 1: Write the header, a stub and the failing tests**

`src/LightMetaType.h`:

```cpp
#pragma once

#include <QMetaType>

#include "LightModel.h"

Q_DECLARE_METATYPE(LightState)
```

`src/DeviceWriter.h`:

```cpp
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "Light.h"
#include "LightMetaType.h"

class DeviceWriter : public QObject
{
    Q_OBJECT

public:
    explicit DeviceWriter(int intervalMs = 30, QObject* parent = nullptr);

    void setLights(const std::vector<std::shared_ptr<Light>>& lights);

public slots:
    void enqueue(const QString& key, const LightState& target);

signals:
    void written(const QString& key, const LightState& applied);

private:
    void flush();

    std::map<std::string, std::shared_ptr<Light>> lights;
    std::map<std::string, LightState> pending;
    QTimer timer;
};
```

`src/DeviceWriter.cpp` (stub):

```cpp
#include "DeviceWriter.h"

DeviceWriter::DeviceWriter(int intervalMs, QObject* parent)
    : QObject(parent)
    , timer(this)
{
    timer.setInterval(intervalMs);
}

void DeviceWriter::setLights(const std::vector<std::shared_ptr<Light>>&)
{
}

void DeviceWriter::enqueue(const QString&, const LightState&)
{
}

void DeviceWriter::flush()
{
}
```

`tests/TestDeviceWriter.h`:

```cpp
#pragma once

#include <QObject>

class TestDeviceWriter : public QObject
{
    Q_OBJECT

private slots:
    void coalescesWritesPerLight();
    void writesEachLight();
    void waitsForInterval();
    void dropsWritesForRemovedLights();
};
```

`tests/TestDeviceWriter.cpp`:

```cpp
#include "TestDeviceWriter.h"

#include <memory>

#include <QSignalSpy>
#include <QTest>

#include "DeviceWriter.h"
#include "FakeLight.h"
#include "LightBuilders.h"

void TestDeviceWriter::coalescesWritesPerLight()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(2, 2, 2)}));
    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(3, 3, 3)}));

    QTRY_COMPARE(spy.count(), 1);
    QTest::qWait(60);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(light->applyCount, 1);
    QCOMPARE(light->state, directLight({makeRgb(3, 3, 3)}));
    QCOMPARE(spy.at(0).at(1).value<LightState>(), directLight({makeRgb(3, 3, 3)}));
}

void TestDeviceWriter::writesEachLight()
{
    auto first = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    auto second = std::make_shared<FakeLight>("b", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({first, second});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(4, 4, 4)}));
    writer.enqueue(QStringLiteral("b"), directLight({makeRgb(5, 5, 5)}));

    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(first->state, directLight({makeRgb(4, 4, 4)}));
    QCOMPARE(second->state, directLight({makeRgb(5, 5, 5)}));
}

void TestDeviceWriter::waitsForInterval()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(6, 6, 6)}));

    QCOMPARE(light->applyCount, 0);
    QTRY_COMPARE(light->applyCount, 1);
}

void TestDeviceWriter::dropsWritesForRemovedLights()
{
    auto light = std::make_shared<FakeLight>("a", directLight({makeRgb(1, 1, 1)}));
    DeviceWriter writer(30);
    writer.setLights({light});
    QSignalSpy spy(&writer, &DeviceWriter::written);

    writer.enqueue(QStringLiteral("a"), directLight({makeRgb(7, 7, 7)}));
    writer.setLights({});

    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(light->applyCount, 0);
}
```

Register: `src/src.pro` add `LightMetaType.h DeviceWriter.h` to `HEADERS`, `DeviceWriter.cpp` to `SOURCES`. `tests/tests.pro` add `TestDeviceWriter.h ../src/LightMetaType.h ../src/DeviceWriter.h` to `HEADERS`, `TestDeviceWriter.cpp ../src/DeviceWriter.cpp` to `SOURCES`. `tests/main.cpp`: add `#include "LightMetaType.h"` and `#include "TestDeviceWriter.h"`, call `qRegisterMetaType<LightState>("LightState");` right after constructing `app`, and add `tests.emplace_back(new TestDeviceWriter);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDeviceWriter`
Expected: FAIL in `coalescesWritesPerLight` (`spy.count()` stays 0).

- [ ] **Step 3: Implement the writer**

Replace `src/DeviceWriter.cpp`:

```cpp
#include "DeviceWriter.h"

#include <iterator>

DeviceWriter::DeviceWriter(int intervalMs, QObject* parent)
    : QObject(parent)
    , timer(this)
{
    timer.setSingleShot(true);
    timer.setInterval(intervalMs);
    connect(&timer, &QTimer::timeout, this, &DeviceWriter::flush);
}

void DeviceWriter::setLights(const std::vector<std::shared_ptr<Light>>& newLights)
{
    lights.clear();
    for(const std::shared_ptr<Light>& light : newLights)
    {
        lights[light->key()] = light;
    }
    for(auto entry = pending.begin(); entry != pending.end();)
    {
        entry = lights.count(entry->first) ? std::next(entry) : pending.erase(entry);
    }
}

void DeviceWriter::enqueue(const QString& key, const LightState& target)
{
    pending[key.toStdString()] = target;
    if(!timer.isActive())
    {
        timer.start();
    }
}

void DeviceWriter::flush()
{
    std::map<std::string, LightState> batch;
    batch.swap(pending);
    for(const auto& [key, target] : batch)
    {
        const auto found = lights.find(key);
        if(found == lights.end())
        {
            continue;
        }
        found->second->apply(target);
        emit written(QString::fromStdString(key), target);
    }
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestDeviceWriter`
Expected: all 4 PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add coalescing device writer"
```

---

### Task 6: uleds keyboard backlight

**Files:**
- Create: `src/UledsBacklight.h`, `src/UledsBacklight.cpp`, `tests/TestUledsBacklight.h`, `tests/TestUledsBacklight.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Produces: `class UledsBacklight : public QObject` with `enum class Status { Closed, Ready, Missing, NoPermission, NameTaken, Failed }`, `static constexpr int MaxBrightness = 100`, constructor `(const QString& ledName = "openrgb::kbd_backlight", const QString& devicePath = "/dev/uleds", const QString& ledsDir = "/sys/class/leds", QObject* parent = nullptr)`, `Status open()`, `void close()`, `Status status() const`, `bool ledExists() const`, `static QByteArray registration(const QString& name, int maxBrightness)`, `static std::optional<int> parseLevel(const QByteArray& data)`, signal `levelChanged(int level)`.

Kernel protocol (`include/uapi/linux/uleds.h`): write `struct uleds_user_dev { char name[64]; int max_brightness; }` (68 bytes) to `/dev/uleds`; each later `read()` returns one `int` brightness; closing the fd removes the LED.

- [ ] **Step 1: Write the header, a stub and the failing tests**

`src/UledsBacklight.h`:

```cpp
#pragma once

#include <optional>

#include <QByteArray>
#include <QObject>
#include <QString>

class QSocketNotifier;

class UledsBacklight : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Closed,
        Ready,
        Missing,
        NoPermission,
        NameTaken,
        Failed,
    };

    static constexpr int MaxBrightness = 100;

    explicit UledsBacklight(const QString& ledName = QStringLiteral("openrgb::kbd_backlight"),
                            const QString& devicePath = QStringLiteral("/dev/uleds"),
                            const QString& ledsDir = QStringLiteral("/sys/class/leds"),
                            QObject* parent = nullptr);
    ~UledsBacklight() override;

    Status open();
    void close();
    Status status() const;
    bool ledExists() const;

    static QByteArray registration(const QString& name, int maxBrightness);
    static std::optional<int> parseLevel(const QByteArray& data);

signals:
    void levelChanged(int level);

private:
    void onReadable();

    QString name;
    QString device;
    QString ledsDirectory;
    int fd = -1;
    QSocketNotifier* notifier = nullptr;
    Status current = Status::Closed;
};
```

`src/UledsBacklight.cpp` (stub):

```cpp
#include "UledsBacklight.h"

UledsBacklight::UledsBacklight(const QString& ledName, const QString& devicePath, const QString& ledsDir, QObject* parent)
    : QObject(parent)
    , name(ledName)
    , device(devicePath)
    , ledsDirectory(ledsDir)
{
}

UledsBacklight::~UledsBacklight()
{
}

UledsBacklight::Status UledsBacklight::open()
{
    return Status::Failed;
}

void UledsBacklight::close()
{
}

UledsBacklight::Status UledsBacklight::status() const
{
    return current;
}

bool UledsBacklight::ledExists() const
{
    return false;
}

QByteArray UledsBacklight::registration(const QString&, int)
{
    return QByteArray();
}

std::optional<int> UledsBacklight::parseLevel(const QByteArray&)
{
    return std::nullopt;
}

void UledsBacklight::onReadable()
{
}
```

`tests/TestUledsBacklight.h`:

```cpp
#pragma once

#include <QObject>

class TestUledsBacklight : public QObject
{
    Q_OBJECT

private slots:
    void registrationMatchesKernelLayout();
    void registrationTruncatesLongNames();
    void parsesOneIntPerRead();
    void openReportsMissingDevice();
    void openReportsNoPermission();
    void openRefusesTakenName();
};
```

`tests/TestUledsBacklight.cpp`:

```cpp
#include "TestUledsBacklight.h"

#include <cstring>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "UledsBacklight.h"

void TestUledsBacklight::registrationMatchesKernelLayout()
{
    const QByteArray data = UledsBacklight::registration(QStringLiteral("openrgb::kbd_backlight"), 100);
    QCOMPARE(data.size(), 68);
    QCOMPARE(QByteArray(data.constData()), QByteArray("openrgb::kbd_backlight"));
    QCOMPARE(data.at(22), char(0));
    int maximum = 0;
    std::memcpy(&maximum, data.constData() + 64, sizeof(int));
    QCOMPARE(maximum, 100);
}

void TestUledsBacklight::registrationTruncatesLongNames()
{
    const QByteArray data = UledsBacklight::registration(QString(100, QLatin1Char('x')), 100);
    QCOMPARE(data.size(), 68);
    QCOMPARE(int(std::strlen(data.constData())), 63);
}

void TestUledsBacklight::parsesOneIntPerRead()
{
    int value = 42;
    const QByteArray data(reinterpret_cast<const char*>(&value), sizeof(int));
    QCOMPARE(UledsBacklight::parseLevel(data), std::optional<int>(42));
    QCOMPARE(UledsBacklight::parseLevel(QByteArray("abc")), std::optional<int>());
}

void TestUledsBacklight::openReportsMissingDevice()
{
    QTemporaryDir dir;
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), dir.filePath(QStringLiteral("uleds")), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::Missing);
}

void TestUledsBacklight::openReportsNoPermission()
{
    if(geteuid() == 0)
    {
        QSKIP("root can open any file");
    }
    QTemporaryDir dir;
    QFile device(dir.filePath(QStringLiteral("uleds")));
    QVERIFY(device.open(QIODevice::WriteOnly));
    device.close();
    QVERIFY(device.setPermissions(QFileDevice::Permissions()));
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), device.fileName(), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::NoPermission);
}

void TestUledsBacklight::openRefusesTakenName()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("test::kbd_backlight")));
    UledsBacklight backlight(QStringLiteral("test::kbd_backlight"), dir.filePath(QStringLiteral("uleds")), dir.path());
    QCOMPARE(backlight.open(), UledsBacklight::Status::NameTaken);
}
```

Register: `src/src.pro` add `UledsBacklight.h` / `UledsBacklight.cpp`. `tests/tests.pro` add `TestUledsBacklight.h ../src/UledsBacklight.h` to `HEADERS`, `TestUledsBacklight.cpp ../src/UledsBacklight.cpp` to `SOURCES`. `tests/main.cpp`: include and `tests.emplace_back(new TestUledsBacklight);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestUledsBacklight`
Expected: FAIL (`registration` returns an empty array, `open` returns `Failed`).

- [ ] **Step 3: Implement the backlight**

Replace `src/UledsBacklight.cpp`:

```cpp
#include "UledsBacklight.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

namespace
{
constexpr int NameSize = 64;
}

UledsBacklight::UledsBacklight(const QString& ledName, const QString& devicePath, const QString& ledsDir, QObject* parent)
    : QObject(parent)
    , name(ledName)
    , device(devicePath)
    , ledsDirectory(ledsDir)
{
}

UledsBacklight::~UledsBacklight()
{
    close();
}

UledsBacklight::Status UledsBacklight::open()
{
    if(fd >= 0)
    {
        return current;
    }
    if(ledExists())
    {
        current = Status::NameTaken;
        return current;
    }

    fd = ::open(QFile::encodeName(device).constData(), O_RDWR | O_CLOEXEC);
    if(fd < 0)
    {
        current = (errno == ENOENT) ? Status::Missing : (errno == EACCES || errno == EPERM) ? Status::NoPermission : Status::Failed;
        return current;
    }

    const QByteArray request = registration(name, MaxBrightness);
    if(::write(fd, request.constData(), size_t(request.size())) != ssize_t(request.size()))
    {
        ::close(fd);
        fd = -1;
        current = Status::Failed;
        return current;
    }

    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &UledsBacklight::onReadable);
    current = Status::Ready;
    return current;
}

void UledsBacklight::close()
{
    delete notifier;
    notifier = nullptr;
    if(fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
    current = Status::Closed;
}

UledsBacklight::Status UledsBacklight::status() const
{
    return current;
}

bool UledsBacklight::ledExists() const
{
    return QFileInfo::exists(ledsDirectory + QLatin1Char('/') + name);
}

QByteArray UledsBacklight::registration(const QString& ledName, int maxBrightness)
{
    QByteArray buffer(NameSize + int(sizeof(int)), '\0');
    const QByteArray encoded = ledName.toUtf8().left(NameSize - 1);
    std::memcpy(buffer.data(), encoded.constData(), size_t(encoded.size()));
    std::memcpy(buffer.data() + NameSize, &maxBrightness, sizeof(int));
    return buffer;
}

std::optional<int> UledsBacklight::parseLevel(const QByteArray& data)
{
    if(data.size() != int(sizeof(int)))
    {
        return std::nullopt;
    }
    int value = 0;
    std::memcpy(&value, data.constData(), sizeof(int));
    return value;
}

void UledsBacklight::onReadable()
{
    QByteArray data(int(sizeof(int)), '\0');
    if(::read(fd, data.data(), size_t(data.size())) != ssize_t(sizeof(int)))
    {
        return;
    }
    if(const std::optional<int> level = parseLevel(data))
    {
        emit levelChanged(*level);
    }
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestUledsBacklight`
Expected: all 6 PASS (`openReportsNoPermission` is skipped only when run as root).

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add uleds keyboard backlight"
```

---

### Task 7: Plasma accent color source

**Files:**
- Create: `src/AccentColorSource.h`, `src/AccentColorSource.cpp`, `tests/TestAccentColorSource.h`, `tests/TestAccentColorSource.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Consumes: `Rgb`, `makeRgb` (Task 2).
- Produces: `class AccentColorSource : public QObject` with `explicit AccentColorSource(const QString& kdeglobalsPath = defaultPath(), QObject* parent = nullptr)`, `static QString defaultPath()`, `static std::optional<Rgb> readAccent(const QString& path)`, `static std::optional<Rgb> parseColor(const QString& value)`, `void start()`, `std::optional<Rgb> current() const`, signal `accentChanged(bool available, unsigned int rgb)`.

Rule (Kameleon): `[General] AccentColor`, else `[Colors:View] ForegroundActive`, else white; no file → unavailable. Changes arrive as the session-bus signal `org.kde.kconfig.notify.ConfigChanged` on path `/kdeglobals`, with a file watcher as fallback.

- [ ] **Step 1: Write the header, a stub and the failing tests**

`src/AccentColorSource.h`:

```cpp
#pragma once

#include <optional>

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include "LightModel.h"

class AccentColorSource : public QObject
{
    Q_OBJECT

public:
    explicit AccentColorSource(const QString& kdeglobalsPath = defaultPath(), QObject* parent = nullptr);

    static QString defaultPath();
    static std::optional<Rgb> readAccent(const QString& path);
    static std::optional<Rgb> parseColor(const QString& value);

    void start();
    std::optional<Rgb> current() const;

signals:
    void accentChanged(bool available, unsigned int rgb);

private slots:
    void reload();

private:
    QString path;
    std::optional<Rgb> value;
    bool started = false;
    QFileSystemWatcher watcher;
};
```

`src/AccentColorSource.cpp` (stub):

```cpp
#include "AccentColorSource.h"

#include <QStandardPaths>

AccentColorSource::AccentColorSource(const QString& kdeglobalsPath, QObject* parent)
    : QObject(parent)
    , path(kdeglobalsPath)
{
}

QString AccentColorSource::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdeglobals");
}

std::optional<Rgb> AccentColorSource::readAccent(const QString&)
{
    return std::nullopt;
}

std::optional<Rgb> AccentColorSource::parseColor(const QString&)
{
    return std::nullopt;
}

void AccentColorSource::start()
{
}

std::optional<Rgb> AccentColorSource::current() const
{
    return value;
}

void AccentColorSource::reload()
{
}
```

`tests/TestAccentColorSource.h`:

```cpp
#pragma once

#include <QObject>

class TestAccentColorSource : public QObject
{
    Q_OBJECT

private slots:
    void customAccentWins();
    void schemeColorIsTheFallback();
    void whiteWhenNoKeys();
    void missingFileIsUnavailable();
    void parsesHexAndAlpha();
    void garbageIsIgnored();
    void emitsWhenFileIsReplaced();
};
```

`tests/TestAccentColorSource.cpp`:

```cpp
#include "TestAccentColorSource.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AccentColorSource.h"

namespace
{
QString writeFile(const QTemporaryDir& dir, const QByteArray& content)
{
    const QString temporary = dir.filePath(QStringLiteral("kdeglobals.new"));
    const QString target = dir.filePath(QStringLiteral("kdeglobals"));
    QFile file(temporary);
    if(file.open(QIODevice::WriteOnly))
    {
        file.write(content);
        file.close();
    }
    QFile::remove(target);
    QFile::rename(temporary, target);
    return target;
}
}

void TestAccentColorSource::customAccentWins()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nAccentColor=61,174,233\n\n[Colors:View]\nForegroundActive=1,2,3\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(61, 174, 233)));
}

void TestAccentColorSource::schemeColorIsTheFallback()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nColorScheme=BreezeDark\n\n[Colors:View]\nForegroundActive=1,2,3\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(1, 2, 3)));
}

void TestAccentColorSource::whiteWhenNoKeys()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nColorScheme=BreezeDark\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(255, 255, 255)));
}

void TestAccentColorSource::missingFileIsUnavailable()
{
    QTemporaryDir dir;
    QCOMPARE(AccentColorSource::readAccent(dir.filePath(QStringLiteral("nope"))), std::optional<Rgb>());
}

void TestAccentColorSource::parsesHexAndAlpha()
{
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("#3daee9")), std::optional<Rgb>(makeRgb(61, 174, 233)));
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("61,174,233,255")), std::optional<Rgb>(makeRgb(61, 174, 233)));
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral(" 61, 174 ,233 ")), std::optional<Rgb>(makeRgb(61, 174, 233)));
}

void TestAccentColorSource::garbageIsIgnored()
{
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("red,green,blue")), std::optional<Rgb>());
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("300,1,1")), std::optional<Rgb>());
    QCOMPARE(AccentColorSource::parseColor(QStringLiteral("1,2")), std::optional<Rgb>());

    QTemporaryDir dir;
    const QString path = writeFile(dir, "no group line\n[General\nAccentColor=bogus\n=5\n[Colors:View]\nForegroundActive=10,20,30\n");
    QCOMPARE(AccentColorSource::readAccent(path), std::optional<Rgb>(makeRgb(10, 20, 30)));
}

void TestAccentColorSource::emitsWhenFileIsReplaced()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, "[General]\nAccentColor=1,1,1\n");
    AccentColorSource source(path);
    QSignalSpy spy(&source, &AccentColorSource::accentChanged);

    source.start();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toUInt(), makeRgb(1, 1, 1));

    writeFile(dir, "[General]\nAccentColor=200,10,10\n");
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), true);
    QCOMPARE(spy.at(1).at(1).toUInt(), makeRgb(200, 10, 10));
}
```

Register: `src/src.pro` add `AccentColorSource.h` / `AccentColorSource.cpp`. `tests/tests.pro` add `TestAccentColorSource.h ../src/AccentColorSource.h` to `HEADERS`, `TestAccentColorSource.cpp ../src/AccentColorSource.cpp` to `SOURCES`. `tests/main.cpp`: include and `tests.emplace_back(new TestAccentColorSource);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestAccentColorSource`
Expected: FAIL (stubs return nothing).

- [ ] **Step 3: Implement the source**

Replace `src/AccentColorSource.cpp`:

```cpp
#include "AccentColorSource.h"

#include <QColor>
#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

AccentColorSource::AccentColorSource(const QString& kdeglobalsPath, QObject* parent)
    : QObject(parent)
    , path(kdeglobalsPath)
    , watcher(this)
{
}

QString AccentColorSource::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdeglobals");
}

std::optional<Rgb> AccentColorSource::parseColor(const QString& text)
{
    const QString trimmed = text.trimmed();
    if(trimmed.startsWith(QLatin1Char('#')))
    {
        const QColor color(trimmed);
        if(!color.isValid())
        {
            return std::nullopt;
        }
        return makeRgb((unsigned int)color.red(), (unsigned int)color.green(), (unsigned int)color.blue());
    }

    const QStringList parts = trimmed.split(QLatin1Char(','));
    if(parts.size() != 3 && parts.size() != 4)
    {
        return std::nullopt;
    }
    unsigned int channels[3];
    for(int i = 0; i < 3; i++)
    {
        bool ok = false;
        const int channel = parts[i].trimmed().toInt(&ok);
        if(!ok || channel < 0 || channel > 255)
        {
            return std::nullopt;
        }
        channels[i] = (unsigned int)channel;
    }
    return makeRgb(channels[0], channels[1], channels[2]);
}

std::optional<Rgb> AccentColorSource::readAccent(const QString& filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    QString group;
    std::optional<Rgb> custom;
    std::optional<Rgb> scheme;
    while(!file.atEnd())
    {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if(line.startsWith(QLatin1Char('[')))
        {
            const int end = line.indexOf(QLatin1Char(']'));
            group = (end > 0) ? line.mid(1, end - 1) : QString();
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if(equals <= 0)
        {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString text = line.mid(equals + 1);
        if(group == QLatin1String("General") && key == QLatin1String("AccentColor"))
        {
            custom = parseColor(text);
        }
        else if(group == QLatin1String("Colors:View") && key == QLatin1String("ForegroundActive"))
        {
            scheme = parseColor(text);
        }
    }

    if(custom)
    {
        return custom;
    }
    if(scheme)
    {
        return scheme;
    }
    return makeRgb(255, 255, 255);
}

void AccentColorSource::start()
{
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/kdeglobals"), QStringLiteral("org.kde.kconfig.notify"), QStringLiteral("ConfigChanged"), this, SLOT(reload()));
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, &AccentColorSource::reload);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &AccentColorSource::reload);
    watcher.addPath(QFileInfo(path).absolutePath());
    reload();
}

std::optional<Rgb> AccentColorSource::current() const
{
    return value;
}

void AccentColorSource::reload()
{
    if(QFileInfo::exists(path) && !watcher.files().contains(path))
    {
        watcher.addPath(path);
    }
    const std::optional<Rgb> latest = readAccent(path);
    if(started && latest == value)
    {
        return;
    }
    started = true;
    value = latest;
    emit accentChanged(value.has_value(), value.value_or(0));
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestAccentColorSource`
Expected: all 7 PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add Plasma accent color source"
```

---

### Task 8: Setup detection, PowerDevil probe and status text

**Files:**
- Create: `src/SystemSetup.h`, `src/SystemSetup.cpp`, `src/PowerDevilProbe.h`, `src/PowerDevilProbe.cpp`, `src/StatusText.h`, `src/StatusText.cpp`, `tests/TestSystemSetup.h`, `tests/TestSystemSetup.cpp`, `tests/TestStatusText.h`, `tests/TestStatusText.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Consumes: `UledsBacklight::Status` (Task 6).
- Produces:
  - `class SystemSetup : public QObject` with `enum class State { Ready, NeedsSetup, NoKernelSupport }`, `struct Paths { QString device; QString loadedModule; QString modulesDir; }`, `static Paths systemPaths()`, `static State detect(const Paths&)`, `static QString setupScript()`, `static QString manualCommands()`, slot `void runSetup()`, signal `setupFinished(bool success, const QString& message)`
  - `class PowerDevilProbe : public QObject` with `static bool needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness)`, `bool restartNeeded() const`, slots `void check(bool backlightExists)`, `void restartPowerDevil()`, signal `restartNeededChanged(bool needed)`
  - `struct StatusInput { SystemSetup::State setup; UledsBacklight::Status backlight; bool accentAvailable; bool powerDevilNeedsRestart; };`, `QStringList statusLines(const StatusInput&)`

- [ ] **Step 1: Write headers, stubs and the failing tests**

`src/SystemSetup.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>

class SystemSetup : public QObject
{
    Q_OBJECT

public:
    enum class State
    {
        Ready,
        NeedsSetup,
        NoKernelSupport,
    };

    struct Paths
    {
        QString device;
        QString loadedModule;
        QString modulesDir;
    };

    using QObject::QObject;

    static Paths systemPaths();
    static State detect(const Paths& paths);
    static QString setupScript();
    static QString manualCommands();

public slots:
    void runSetup();

signals:
    void setupFinished(bool success, const QString& message);
};
```

`src/SystemSetup.cpp` (stub):

```cpp
#include "SystemSetup.h"

SystemSetup::Paths SystemSetup::systemPaths()
{
    return Paths();
}

SystemSetup::State SystemSetup::detect(const Paths&)
{
    return State::NoKernelSupport;
}

QString SystemSetup::setupScript()
{
    return QString();
}

QString SystemSetup::manualCommands()
{
    return QString();
}

void SystemSetup::runSetup()
{
}
```

`src/PowerDevilProbe.h`:

```cpp
#pragma once

#include <QObject>

class PowerDevilProbe : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    static bool needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness);
    bool restartNeeded() const;

public slots:
    void check(bool backlightExists);
    void restartPowerDevil();

signals:
    void restartNeededChanged(bool needed);

private:
    bool needed = false;
    bool lastBacklightExists = false;
};
```

`src/PowerDevilProbe.cpp` (stub):

```cpp
#include "PowerDevilProbe.h"

bool PowerDevilProbe::needsRestart(bool, bool, bool, int)
{
    return false;
}

bool PowerDevilProbe::restartNeeded() const
{
    return needed;
}

void PowerDevilProbe::check(bool)
{
}

void PowerDevilProbe::restartPowerDevil()
{
}
```

`src/StatusText.h`:

```cpp
#pragma once

#include <QStringList>

#include "SystemSetup.h"
#include "UledsBacklight.h"

struct StatusInput
{
    SystemSetup::State setup = SystemSetup::State::NeedsSetup;
    UledsBacklight::Status backlight = UledsBacklight::Status::Closed;
    bool accentAvailable = false;
    bool powerDevilNeedsRestart = false;
};

QStringList statusLines(const StatusInput& input);
```

`src/StatusText.cpp` (stub):

```cpp
#include "StatusText.h"

QStringList statusLines(const StatusInput&)
{
    return QStringList();
}
```

`tests/TestSystemSetup.h`:

```cpp
#pragma once

#include <QObject>

class TestSystemSetup : public QObject
{
    Q_OBJECT

private slots:
    void readyWhenDeviceIsAccessible();
    void needsSetupWhenDeviceIsNotAccessible();
    void needsSetupWhenModuleIsAvailable();
    void needsSetupWhenModuleIsBuiltIn();
    void noKernelSupportOtherwise();
    void setupScriptWritesBothFilesAndLoadsModule();
    void powerDevilRestartRule();
};
```

`tests/TestSystemSetup.cpp`:

```cpp
#include "TestSystemSetup.h"

#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "PowerDevilProbe.h"
#include "SystemSetup.h"

namespace
{
SystemSetup::Paths pathsIn(const QTemporaryDir& dir)
{
    return SystemSetup::Paths{dir.filePath(QStringLiteral("uleds")), dir.filePath(QStringLiteral("module/uleds")), dir.filePath(QStringLiteral("modules"))};
}

void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if(file.open(QIODevice::WriteOnly))
    {
        file.write("uleds\n");
    }
}
}

void TestSystemSetup::readyWhenDeviceIsAccessible()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("uleds")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::Ready);
}

void TestSystemSetup::needsSetupWhenDeviceIsNotAccessible()
{
    if(geteuid() == 0)
    {
        QSKIP("root can open any file");
    }
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("uleds")));
    QVERIFY(QFile(dir.filePath(QStringLiteral("uleds"))).setPermissions(QFileDevice::Permissions()));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::needsSetupWhenModuleIsAvailable()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("modules/kernel/drivers/leds/uleds.ko.zst")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::needsSetupWhenModuleIsBuiltIn()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath(QStringLiteral("modules")));
    QFile builtin(dir.filePath(QStringLiteral("modules/modules.builtin")));
    QVERIFY(builtin.open(QIODevice::WriteOnly));
    builtin.write("kernel/drivers/leds/led-class.ko\nkernel/drivers/leds/uleds.ko\n");
    builtin.close();
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NeedsSetup);
}

void TestSystemSetup::noKernelSupportOtherwise()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath(QStringLiteral("modules/kernel/drivers/leds")));
    QCOMPARE(SystemSetup::detect(pathsIn(dir)), SystemSetup::State::NoKernelSupport);
}

void TestSystemSetup::setupScriptWritesBothFilesAndLoadsModule()
{
    const QString script = SystemSetup::setupScript();
    QVERIFY(script.contains(QStringLiteral("echo uleds > /etc/modules-load.d/openrgb-kbd-backlight.conf")));
    QVERIFY(script.contains(QStringLiteral("echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' > /etc/udev/rules.d/70-openrgb-kbd-backlight.rules")));
    QVERIFY(script.contains(QStringLiteral("modprobe uleds")));
    QVERIFY(script.contains(QStringLiteral("udevadm trigger --name-match=uleds")));
    QVERIFY(SystemSetup::manualCommands().contains(QStringLiteral("sudo modprobe uleds")));
}

void TestSystemSetup::powerDevilRestartRule()
{
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, true, 100), false);
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, true, 0), true);
    QCOMPARE(PowerDevilProbe::needsRestart(true, true, false, 0), true);
    QCOMPARE(PowerDevilProbe::needsRestart(false, true, false, 0), false);
    QCOMPARE(PowerDevilProbe::needsRestart(true, false, false, 0), false);
}
```

`tests/TestStatusText.h`:

```cpp
#pragma once

#include <QObject>

class TestStatusText : public QObject
{
    Q_OBJECT

private slots:
    void readyAndAccent();
    void needsSetup();
    void noKernelSupport();
    void nameTaken();
    void accentUnavailableAndPowerDevil();
};
```

`tests/TestStatusText.cpp`:

```cpp
#include "TestStatusText.h"

#include <QTest>

#include "StatusText.h"

void TestStatusText::readyAndAccent()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::Ready, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights.")});
}

void TestStatusText::needsSetup()
{
    const QStringList lines = statusLines({SystemSetup::State::NeedsSetup, UledsBacklight::Status::NoPermission, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Needs setup: click Set up to allow brightness control.")});
}

void TestStatusText::noKernelSupport()
{
    const QStringList lines = statusLines({SystemSetup::State::NoKernelSupport, UledsBacklight::Status::Missing, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("This kernel has no uleds: brightness control is off.")});
}

void TestStatusText::nameTaken()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::NameTaken, true, false});
    QCOMPARE(lines, QStringList{QStringLiteral("Another OpenRGB instance owns the backlight.")});
}

void TestStatusText::accentUnavailableAndPowerDevil()
{
    const QStringList lines = statusLines({SystemSetup::State::Ready, UledsBacklight::Status::Ready, false, true});
    QCOMPARE(lines,
             (QStringList{QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights."),
                          QStringLiteral("PowerDevil has not picked up the backlight yet."),
                          QStringLiteral("Accent color unavailable (not running in Plasma).")}));
}
```

Register: `src/src.pro` add the three headers and three sources. `tests/tests.pro` add `TestSystemSetup.h TestStatusText.h ../src/SystemSetup.h ../src/PowerDevilProbe.h ../src/StatusText.h` to `HEADERS`, `TestSystemSetup.cpp TestStatusText.cpp ../src/SystemSetup.cpp ../src/PowerDevilProbe.cpp ../src/StatusText.cpp` to `SOURCES`. `tests/main.cpp`: include both and add `tests.emplace_back(new TestSystemSetup);`, `tests.emplace_back(new TestStatusText);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestSystemSetup; build/tests/plasma-integration-tests TestStatusText`
Expected: FAIL (stubs).

- [ ] **Step 3: Implement the three units**

Replace `src/SystemSetup.cpp`:

```cpp
#include "SystemSetup.h"

#include <unistd.h>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSysInfo>

namespace
{
bool kernelHasModule(const QString& modulesDir)
{
    QFile builtin(modulesDir + QStringLiteral("/modules.builtin"));
    if(builtin.open(QIODevice::ReadOnly | QIODevice::Text) && builtin.readAll().contains("/uleds.ko"))
    {
        return true;
    }
    QDirIterator modules(modulesDir + QStringLiteral("/kernel/drivers/leds"), {QStringLiteral("uleds.ko*")}, QDir::Files);
    return modules.hasNext();
}
}

SystemSetup::Paths SystemSetup::systemPaths()
{
    return Paths{QStringLiteral("/dev/uleds"), QStringLiteral("/sys/module/uleds"), QStringLiteral("/lib/modules/") + QSysInfo::kernelVersion()};
}

SystemSetup::State SystemSetup::detect(const Paths& paths)
{
    if(QFileInfo::exists(paths.device))
    {
        return ::access(QFile::encodeName(paths.device).constData(), R_OK | W_OK) == 0 ? State::Ready : State::NeedsSetup;
    }
    if(QFileInfo::exists(paths.loadedModule) || kernelHasModule(paths.modulesDir))
    {
        return State::NeedsSetup;
    }
    return State::NoKernelSupport;
}

QString SystemSetup::setupScript()
{
    return QStringLiteral(
        "echo uleds > /etc/modules-load.d/openrgb-kbd-backlight.conf && "
        "echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' > /etc/udev/rules.d/70-openrgb-kbd-backlight.rules && "
        "modprobe uleds && "
        "udevadm control --reload && "
        "udevadm trigger --name-match=uleds && "
        "udevadm settle");
}

QString SystemSetup::manualCommands()
{
    return QStringLiteral(
        "echo uleds | sudo tee /etc/modules-load.d/openrgb-kbd-backlight.conf\n"
        "echo 'KERNEL==\"uleds\", TAG+=\"uaccess\"' | sudo tee /etc/udev/rules.d/70-openrgb-kbd-backlight.rules\n"
        "sudo modprobe uleds\n"
        "sudo udevadm control --reload\n"
        "sudo udevadm trigger --name-match=uleds");
}

void SystemSetup::runSetup()
{
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
        emit setupFinished(success, success ? QString() : tr("Setup did not complete (exit code %1).").arg(exitCode));
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart)
        {
            emit setupFinished(false, tr("pkexec is not available."));
            process->deleteLater();
        }
    });
    process->start(QStringLiteral("pkexec"), {QStringLiteral("/bin/sh"), QStringLiteral("-c"), setupScript()});
}
```

Replace `src/PowerDevilProbe.cpp`:

```cpp
#include "PowerDevilProbe.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QProcess>
#include <QTimer>

namespace
{
const QString Service = QStringLiteral("org.kde.Solid.PowerManagement");

QVariant callPowerDevil(const QString& path, const QString& interface, const QString& method, const QVariantList& arguments)
{
    QDBusMessage call = QDBusMessage::createMethodCall(Service, path, interface, method);
    call.setArguments(arguments);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if(reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
    {
        return QVariant();
    }
    return reply.arguments().first();
}
}

bool PowerDevilProbe::needsRestart(bool backlightExists, bool powerDevilRunning, bool supported, int maxBrightness)
{
    return backlightExists && powerDevilRunning && (!supported || maxBrightness <= 0);
}

bool PowerDevilProbe::restartNeeded() const
{
    return needed;
}

void PowerDevilProbe::check(bool backlightExists)
{
    lastBacklightExists = backlightExists;
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    const bool running = bus && bus->isServiceRegistered(Service).value();
    bool supported = false;
    int maximum = 0;
    if(running)
    {
        supported = callPowerDevil(QStringLiteral("/org/kde/Solid/PowerManagement"), QStringLiteral("org.kde.Solid.PowerManagement"),
                                   QStringLiteral("isActionSupported"), {QStringLiteral("KeyboardBrightnessControl")})
                        .toBool();
        if(supported)
        {
            maximum = callPowerDevil(QStringLiteral("/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"),
                                     QStringLiteral("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"), QStringLiteral("keyboardBrightnessMax"), {})
                          .toInt();
        }
    }
    const bool now = needsRestart(backlightExists, running, supported, maximum);
    if(now != needed)
    {
        needed = now;
        emit restartNeededChanged(needed);
    }
}

void PowerDevilProbe::restartPowerDevil()
{
    QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("restart"), QStringLiteral("plasma-powerdevil")});
    QTimer::singleShot(5000, this, [this] { check(lastBacklightExists); });
}
```

Replace `src/StatusText.cpp`:

```cpp
#include "StatusText.h"

QStringList statusLines(const StatusInput& input)
{
    QStringList lines;
    if(input.setup == SystemSetup::State::NoKernelSupport)
    {
        lines << QStringLiteral("This kernel has no uleds: brightness control is off.");
    }
    else if(input.backlight == UledsBacklight::Status::Ready)
    {
        lines << QStringLiteral("Ready: Plasma's Keyboard Backlight slider controls these lights.");
    }
    else if(input.backlight == UledsBacklight::Status::NameTaken)
    {
        lines << QStringLiteral("Another OpenRGB instance owns the backlight.");
    }
    else if(input.setup == SystemSetup::State::NeedsSetup || input.backlight == UledsBacklight::Status::NoPermission
            || input.backlight == UledsBacklight::Status::Missing)
    {
        lines << QStringLiteral("Needs setup: click Set up to allow brightness control.");
    }
    else
    {
        lines << QStringLiteral("Could not create the backlight (see the OpenRGB log).");
    }

    if(input.powerDevilNeedsRestart)
    {
        lines << QStringLiteral("PowerDevil has not picked up the backlight yet.");
    }
    if(!input.accentAvailable)
    {
        lines << QStringLiteral("Accent color unavailable (not running in Plasma).");
    }
    return lines;
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestSystemSetup && build/tests/plasma-integration-tests TestStatusText`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add setup detection, PowerDevil probe and status text"
```

---

### Task 9: Settings tab

**Files:**
- Create: `src/SettingsTab.h`, `src/SettingsTab.cpp`, `tests/TestSettingsTab.h`, `tests/TestSettingsTab.cpp`
- Modify: `src/src.pro`, `tests/tests.pro`, `tests/main.cpp`

**Interfaces:**
- Consumes: `PluginSettings`, `DeviceOptions` (Task 3).
- Produces: `class SettingsTab : public QWidget` with `struct DeviceRow { std::string key; std::string name; };`, `void setStatusLines(const QStringList&)`, `void setSetupVisible(bool)`, `void setManualCommands(const QString&)` (empty hides), `void setRestartVisible(bool)`, `void setAccentEnabled(bool)`, `void setDevices(const std::vector<DeviceRow>&, const PluginSettings&)`; signals `setupRequested()`, `restartRequested()`, `accentToggled(bool)`, `deviceOptionsChanged(const QString& key, bool dim, bool accent)`, `shown()`. Object names: `statusLabel`, `setupButton`, `manualCommands`, `restartButton`, `accentCheck`, `deviceTable`.

- [ ] **Step 1: Write the header, a stub and the failing tests**

`src/SettingsTab.h`:

```cpp
#pragma once

#include <string>
#include <vector>

#include <QWidget>

#include "PluginSettings.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class SettingsTab : public QWidget
{
    Q_OBJECT

public:
    struct DeviceRow
    {
        std::string key;
        std::string name;
    };

    explicit SettingsTab(QWidget* parent = nullptr);

    void setStatusLines(const QStringList& lines);
    void setSetupVisible(bool visible);
    void setManualCommands(const QString& commands);
    void setRestartVisible(bool visible);
    void setAccentEnabled(bool enabled);
    void setDevices(const std::vector<DeviceRow>& rows, const PluginSettings& settings);

signals:
    void setupRequested();
    void restartRequested();
    void accentToggled(bool enabled);
    void deviceOptionsChanged(const QString& key, bool dim, bool accent);
    void shown();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void onItemChanged(QTableWidgetItem* item);

    QLabel* status = nullptr;
    QPushButton* setupButton = nullptr;
    QLabel* manual = nullptr;
    QPushButton* restartButton = nullptr;
    QCheckBox* accentBox = nullptr;
    QTableWidget* table = nullptr;
    bool updating = false;
};
```

`src/SettingsTab.cpp` (stub):

```cpp
#include "SettingsTab.h"

SettingsTab::SettingsTab(QWidget* parent)
    : QWidget(parent)
{
}

void SettingsTab::setStatusLines(const QStringList&)
{
}

void SettingsTab::setSetupVisible(bool)
{
}

void SettingsTab::setManualCommands(const QString&)
{
}

void SettingsTab::setRestartVisible(bool)
{
}

void SettingsTab::setAccentEnabled(bool)
{
}

void SettingsTab::setDevices(const std::vector<DeviceRow>&, const PluginSettings&)
{
}

void SettingsTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void SettingsTab::onItemChanged(QTableWidgetItem*)
{
}
```

`tests/TestSettingsTab.h`:

```cpp
#pragma once

#include <QObject>

class TestSettingsTab : public QObject
{
    Q_OBJECT

private slots:
    void rowsReflectSettings();
    void togglingDimEmitsOptions();
    void fillingRowsEmitsNothing();
    void accentCheckboxEmits();
    void statusAndButtons();
};
```

`tests/TestSettingsTab.cpp`:

```cpp
#include "TestSettingsTab.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>

#include "SettingsTab.h"

namespace
{
std::vector<SettingsTab::DeviceRow> rows()
{
    return {{"Keyboard|A1", "Razer Blackwidow Elite"}, {"Mat|B2", "Razer Goliathus"}};
}

PluginSettings settingsWithMatUndimmed()
{
    PluginSettings settings;
    settings.devices["Mat|B2"] = DeviceOptions{false, true};
    return settings;
}
}

void TestSettingsTab::rowsReflectSettings()
{
    SettingsTab tab;
    tab.setDevices(rows(), settingsWithMatUndimmed());
    auto* table = tab.findChild<QTableWidget*>(QStringLiteral("deviceTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Razer Blackwidow Elite"));
    QCOMPARE(table->item(0, 1)->checkState(), Qt::Checked);
    QCOMPARE(table->item(1, 1)->checkState(), Qt::Unchecked);
    QCOMPARE(table->item(1, 2)->checkState(), Qt::Checked);
}

void TestSettingsTab::togglingDimEmitsOptions()
{
    SettingsTab tab;
    tab.setDevices(rows(), PluginSettings());
    QSignalSpy spy(&tab, &SettingsTab::deviceOptionsChanged);
    auto* table = tab.findChild<QTableWidget*>(QStringLiteral("deviceTable"));

    table->item(1, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Mat|B2"));
    QCOMPARE(spy.at(0).at(1).toBool(), false);
    QCOMPARE(spy.at(0).at(2).toBool(), true);
}

void TestSettingsTab::fillingRowsEmitsNothing()
{
    SettingsTab tab;
    QSignalSpy spy(&tab, &SettingsTab::deviceOptionsChanged);
    tab.setDevices(rows(), settingsWithMatUndimmed());
    tab.setDevices(rows(), PluginSettings());
    QCOMPARE(spy.count(), 0);
}

void TestSettingsTab::accentCheckboxEmits()
{
    SettingsTab tab;
    QSignalSpy spy(&tab, &SettingsTab::accentToggled);
    tab.setAccentEnabled(true);
    QCOMPARE(spy.count(), 0);
    auto* box = tab.findChild<QCheckBox*>(QStringLiteral("accentCheck"));
    QVERIFY(box->isChecked());
    box->setChecked(false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}

void TestSettingsTab::statusAndButtons()
{
    SettingsTab tab;
    tab.setStatusLines({QStringLiteral("one"), QStringLiteral("two")});
    QCOMPARE(tab.findChild<QLabel*>(QStringLiteral("statusLabel"))->text(), QStringLiteral("one\ntwo"));

    auto* setup = tab.findChild<QPushButton*>(QStringLiteral("setupButton"));
    QVERIFY(setup->isHidden());
    tab.setSetupVisible(true);
    QVERIFY(!setup->isHidden());
    QSignalSpy setupSpy(&tab, &SettingsTab::setupRequested);
    setup->click();
    QCOMPARE(setupSpy.count(), 1);

    auto* restart = tab.findChild<QPushButton*>(QStringLiteral("restartButton"));
    QVERIFY(restart->isHidden());
    tab.setRestartVisible(true);
    QSignalSpy restartSpy(&tab, &SettingsTab::restartRequested);
    restart->click();
    QCOMPARE(restartSpy.count(), 1);

    auto* manual = tab.findChild<QLabel*>(QStringLiteral("manualCommands"));
    QVERIFY(manual->isHidden());
    tab.setManualCommands(QStringLiteral("sudo modprobe uleds"));
    QVERIFY(!manual->isHidden());
    tab.setManualCommands(QString());
    QVERIFY(manual->isHidden());
}
```

Register: `src/src.pro` add `SettingsTab.h` / `SettingsTab.cpp`. `tests/tests.pro` add `TestSettingsTab.h ../src/SettingsTab.h` to `HEADERS`, `TestSettingsTab.cpp ../src/SettingsTab.cpp` to `SOURCES`. `tests/main.cpp`: include and `tests.emplace_back(new TestSettingsTab);`.

- [ ] **Step 2: Run the tests to see them fail**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestSettingsTab`
Expected: FAIL (`findChild` returns null → `QVERIFY(table)` fails).

- [ ] **Step 3: Implement the tab**

Replace `src/SettingsTab.cpp`:

```cpp
#include "SettingsTab.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

SettingsTab::SettingsTab(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    status = new QLabel(this);
    status->setObjectName(QStringLiteral("statusLabel"));
    status->setWordWrap(true);

    setupButton = new QPushButton(tr("Set up"), this);
    setupButton->setObjectName(QStringLiteral("setupButton"));
    setupButton->hide();

    manual = new QLabel(this);
    manual->setObjectName(QStringLiteral("manualCommands"));
    manual->setTextInteractionFlags(Qt::TextSelectableByMouse);
    manual->setWordWrap(true);
    manual->hide();

    restartButton = new QPushButton(tr("Restart Plasma power management"), this);
    restartButton->setObjectName(QStringLiteral("restartButton"));
    restartButton->hide();

    accentBox = new QCheckBox(tr("Follow Plasma's accent color"), this);
    accentBox->setObjectName(QStringLiteral("accentCheck"));

    table = new QTableWidget(0, 3, this);
    table->setObjectName(QStringLiteral("deviceTable"));
    table->setHorizontalHeaderLabels({tr("Device"), tr("Dim"), tr("Accent color")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    layout->addWidget(status);
    layout->addWidget(setupButton);
    layout->addWidget(manual);
    layout->addWidget(restartButton);
    layout->addWidget(accentBox);
    layout->addWidget(table);

    connect(setupButton, &QPushButton::clicked, this, &SettingsTab::setupRequested);
    connect(restartButton, &QPushButton::clicked, this, &SettingsTab::restartRequested);
    connect(accentBox, &QCheckBox::toggled, this, [this](bool enabled) {
        if(!updating)
        {
            emit accentToggled(enabled);
        }
    });
    connect(table, &QTableWidget::itemChanged, this, &SettingsTab::onItemChanged);
}

void SettingsTab::setStatusLines(const QStringList& lines)
{
    status->setText(lines.join(QLatin1Char('\n')));
}

void SettingsTab::setSetupVisible(bool visible)
{
    setupButton->setVisible(visible);
}

void SettingsTab::setManualCommands(const QString& commands)
{
    manual->setText(commands);
    manual->setVisible(!commands.isEmpty());
}

void SettingsTab::setRestartVisible(bool visible)
{
    restartButton->setVisible(visible);
}

void SettingsTab::setAccentEnabled(bool enabled)
{
    updating = true;
    accentBox->setChecked(enabled);
    updating = false;
}

void SettingsTab::setDevices(const std::vector<DeviceRow>& rows, const PluginSettings& settings)
{
    updating = true;
    table->setRowCount(int(rows.size()));
    for(int row = 0; row < int(rows.size()); row++)
    {
        const DeviceOptions options = settings.optionsFor(rows[std::size_t(row)].key);

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(rows[std::size_t(row)].name));
        nameItem->setData(Qt::UserRole, QString::fromStdString(rows[std::size_t(row)].key));
        nameItem->setFlags(Qt::ItemIsEnabled);

        auto* dimItem = new QTableWidgetItem();
        dimItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        dimItem->setCheckState(options.dim ? Qt::Checked : Qt::Unchecked);

        auto* accentItem = new QTableWidgetItem();
        accentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        accentItem->setCheckState(options.accent ? Qt::Checked : Qt::Unchecked);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, dimItem);
        table->setItem(row, 2, accentItem);
    }
    updating = false;
}

void SettingsTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    emit shown();
}

void SettingsTab::onItemChanged(QTableWidgetItem* item)
{
    if(updating || item->column() == 0)
    {
        return;
    }
    const int row = item->row();
    const QString key = table->item(row, 0)->data(Qt::UserRole).toString();
    emit deviceOptionsChanged(key, table->item(row, 1)->checkState() == Qt::Checked, table->item(row, 2)->checkState() == Qt::Checked);
}
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `(cd build && make -j"$(nproc)") && build/tests/plasma-integration-tests TestSettingsTab`
Expected: all 5 PASS. Then `build/tests/plasma-integration-tests` (all classes) PASS.

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "Add settings tab"
```

---

### Task 10: OpenRGB adapter and plugin wiring

**Files:**
- Create: `src/OpenRGBLight.h`, `src/OpenRGBLight.cpp`
- Modify: `src/PlasmaIntegrationPlugin.h`, `src/PlasmaIntegrationPlugin.cpp`, `src/src.pro`

**Interfaces:**
- Consumes: everything from Tasks 2–9; OpenRGB `RGBControllerInterface` (`GetActiveMode`, `GetModeCount`, `GetModeColorMode`, `GetModeFlags`, `GetModeBrightness/Min/Max`, `GetModeColorsCount`, `GetModeColor`, `GetZoneCount`, `GetZoneActiveMode`, `GetZoneModeCount`, `GetZoneMode*`, `GetZoneStartIndex`, `GetZoneLEDsCount`, `GetColor`, `SetModeColor`, `SetModeBrightness`, `UpdateMode`, `SetZoneModeColor`, `SetZoneModeBrightness`, `UpdateZoneMode`, `SetColor`, `UpdateLEDs`, `RegisterUpdateCallback`, `UnregisterUpdateCallback`, `GetName`, `GetSerial`, `GetLocation`, `GetHidden`), `OpenRGBPluginAPIInterface` (`GetRGBControllers`, `GetSettings`, `SetSettings`, `SaveSettings`, `LogEntry`), `RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED` (`ResourceManagerCallback.h`), `RGBCONTROLLER_UPDATE_REASON_*`, `MODE_FLAG_HAS_BRIGHTNESS`, `LL_INFO` (`LogManager.h`).
- Produces: `class OpenRGBLight : public Light` with `explicit OpenRGBLight(RGBControllerInterface*)` and `RGBControllerInterface* controller() const`; the finished plugin.

`OpenRGBLight` cannot be unit-tested without a real `RGBControllerInterface` (OpenRGB's concrete controller pulls in its logging and settings code); it is verified on real hardware in Task 11. This task's gate is: everything builds, all unit tests still pass, and the metadata test still passes.

- [ ] **Step 1: Write the adapter**

`src/OpenRGBLight.h`:

```cpp
#pragma once

#include <string>

#include "Light.h"

class RGBControllerInterface;

class OpenRGBLight : public Light
{
public:
    explicit OpenRGBLight(RGBControllerInterface* controller);

    RGBControllerInterface* controller() const;

    std::string key() const override;
    std::string name() const override;
    LightState read() const override;
    void apply(const LightState& target) override;

private:
    RGBControllerInterface* rgb;
    std::string keyValue;
    std::string nameValue;
};
```

`src/OpenRGBLight.cpp`:

```cpp
#include "OpenRGBLight.h"

#include "DeviceKey.h"
#include "RGBControllerInterface.h"

namespace
{
ModeState readDeviceMode(RGBControllerInterface* controller, int index)
{
    ModeState mode;
    if(index < 0 || index >= int(controller->GetModeCount()))
    {
        return mode;
    }
    const unsigned int modeIndex = (unsigned int)index;
    mode.index = index;
    mode.colorMode = ColorMode(controller->GetModeColorMode(modeIndex));
    mode.hasBrightness = (controller->GetModeFlags(modeIndex) & MODE_FLAG_HAS_BRIGHTNESS) != 0;
    mode.brightness = controller->GetModeBrightness(modeIndex);
    mode.brightnessMin = controller->GetModeBrightnessMin(modeIndex);
    mode.brightnessMax = controller->GetModeBrightnessMax(modeIndex);
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(unsigned int i = 0; i < controller->GetModeColorsCount(modeIndex); i++)
        {
            mode.colors.push_back(controller->GetModeColor(modeIndex, i));
        }
    }
    return mode;
}

ModeState readZoneMode(RGBControllerInterface* controller, unsigned int zone, int index)
{
    ModeState mode;
    if(index < 0 || index >= int(controller->GetZoneModeCount(zone)))
    {
        return mode;
    }
    const unsigned int modeIndex = (unsigned int)index;
    mode.index = index;
    mode.colorMode = ColorMode(controller->GetZoneModeColorMode(zone, modeIndex));
    mode.hasBrightness = (controller->GetZoneModeFlags(zone, modeIndex) & MODE_FLAG_HAS_BRIGHTNESS) != 0;
    mode.brightness = controller->GetZoneModeBrightness(zone, modeIndex);
    mode.brightnessMin = controller->GetZoneModeBrightnessMin(zone, modeIndex);
    mode.brightnessMax = controller->GetZoneModeBrightnessMax(zone, modeIndex);
    if(mode.colorMode == ColorMode::ModeSpecific)
    {
        for(unsigned int i = 0; i < controller->GetZoneModeColorsCount(zone, modeIndex); i++)
        {
            mode.colors.push_back(controller->GetZoneModeColor(zone, modeIndex, i));
        }
    }
    return mode;
}
}

OpenRGBLight::OpenRGBLight(RGBControllerInterface* controller)
    : rgb(controller)
    , keyValue(deviceKey(controller->GetName(), controller->GetSerial(), controller->GetLocation()))
    , nameValue(controller->GetName())
{
}

RGBControllerInterface* OpenRGBLight::controller() const
{
    return rgb;
}

std::string OpenRGBLight::key() const
{
    return keyValue;
}

std::string OpenRGBLight::name() const
{
    return nameValue;
}

LightState OpenRGBLight::read() const
{
    LightState state;
    state.mode = readDeviceMode(rgb, rgb->GetActiveMode());
    for(unsigned int zone = 0; zone < rgb->GetZoneCount(); zone++)
    {
        ZoneState zoneState;
        zoneState.mode = readZoneMode(rgb, zone, rgb->GetZoneActiveMode(zone));
        const unsigned int start = rgb->GetZoneStartIndex(zone);
        const unsigned int count = rgb->GetZoneLEDsCount(zone);
        for(unsigned int led = 0; led < count; led++)
        {
            zoneState.leds.push_back(rgb->GetColor(start + led));
        }
        state.zones.push_back(zoneState);
    }
    return state;
}

void OpenRGBLight::apply(const LightState& target)
{
    const LightState current = read();
    if(target.mode.index != current.mode.index || target.zones.size() != current.zones.size())
    {
        return;
    }

    if(target.mode.index >= 0 && target.mode != current.mode)
    {
        const unsigned int modeIndex = (unsigned int)target.mode.index;
        for(std::size_t i = 0; i < target.mode.colors.size(); i++)
        {
            rgb->SetModeColor(modeIndex, (unsigned int)i, target.mode.colors[i]);
        }
        if(target.mode.hasBrightness)
        {
            rgb->SetModeBrightness(modeIndex, target.mode.brightness);
        }
        rgb->UpdateMode();
    }

    bool ledsChanged = false;
    for(unsigned int zone = 0; zone < target.zones.size(); zone++)
    {
        const ZoneState& want = target.zones[zone];
        const ZoneState& have = current.zones[zone];
        if(want.mode.index >= 0 && want.mode.index == have.mode.index && want.mode != have.mode)
        {
            const unsigned int modeIndex = (unsigned int)want.mode.index;
            for(std::size_t i = 0; i < want.mode.colors.size(); i++)
            {
                rgb->SetZoneModeColor(zone, modeIndex, (unsigned int)i, want.mode.colors[i]);
            }
            if(want.mode.hasBrightness)
            {
                rgb->SetZoneModeBrightness(zone, modeIndex, want.mode.brightness);
            }
            rgb->UpdateZoneMode(int(zone));
        }
        if(want.leds != have.leds && want.leds.size() == have.leds.size())
        {
            const unsigned int start = rgb->GetZoneStartIndex(zone);
            for(std::size_t led = 0; led < want.leds.size(); led++)
            {
                rgb->SetColor(start + (unsigned int)led, want.leds[led]);
            }
            ledsChanged = true;
        }
    }
    if(ledsChanged)
    {
        rgb->UpdateLEDs();
    }
}
```

- [ ] **Step 2: Replace the plugin stub with the wired plugin**

`src/PlasmaIntegrationPlugin.h`:

```cpp
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QThread>

#include "OpenRGBPluginInterface.h"
#include "PluginSettings.h"

class AccentColorSource;
class DeviceWriter;
class LightingEngine;
class OpenRGBLight;
class PowerDevilProbe;
class SettingsTab;
class SystemSetup;
class UledsBacklight;

class PlasmaIntegrationPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "PlasmaIntegrationPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~PlasmaIntegrationPlugin() override;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override;
    void Load(OpenRGBPluginAPIInterface* plugin_api) override;
    QWidget* GetWidget() override;
    QMenu* GetTrayMenu() override;
    void Unload() override;
    void OnProfileAboutToLoad() override;
    void OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void ProfileManagerUpdated(unsigned int update_reason) override;
    void ResourceManagerUpdated(unsigned int update_reason) override;
    void SettingsManagerUpdated(unsigned int update_reason) override;

private:
    static void controllerCallback(void* plugin, unsigned int reason, void* controller);

    void refreshLights();
    void onControllerChanged(void* controller);
    void openBacklight();
    void checkPowerDevilSoon();
    void updateStatus();
    void saveSettings();
    void applyAccent();

    OpenRGBPluginAPIInterface* api = nullptr;
    PluginSettings settings;
    std::unique_ptr<LightingEngine> engine;
    QThread writerThread;
    DeviceWriter* writer = nullptr;
    UledsBacklight* backlight = nullptr;
    AccentColorSource* accentSource = nullptr;
    SystemSetup* setup = nullptr;
    PowerDevilProbe* probe = nullptr;
    SettingsTab* tab = nullptr;
    std::vector<std::shared_ptr<OpenRGBLight>> lights;
    std::map<void*, std::string> keysByController;
};
```

`src/PlasmaIntegrationPlugin.cpp`:

```cpp
#include "PlasmaIntegrationPlugin.h"

#include <QTimer>

#include "AccentColorSource.h"
#include "DeviceWriter.h"
#include "LightingEngine.h"
#include "LogManager.h"
#include "OpenRGBLight.h"
#include "PowerDevilProbe.h"
#include "ResourceManagerCallback.h"
#include "RGBControllerInterface.h"
#include "SettingsTab.h"
#include "StatusText.h"
#include "SystemSetup.h"
#include "UledsBacklight.h"

PlasmaIntegrationPlugin::~PlasmaIntegrationPlugin()
{
    Unload();
}

OpenRGBPluginInfo PlasmaIntegrationPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name = "Plasma Integration";
    info.Description = "Control OpenRGB lights from KDE Plasma's keyboard backlight slider and accent color";
    info.Version = PLUGIN_VERSION;
    info.Commit = GIT_COMMIT_ID;
    info.URL = "https://github.com/TRusselo/OpenRGBPlasmaPlugin";
    info.Location = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Label = "Plasma Integration";
    info.ProtocolVersion = 0;
    return info;
}

unsigned int PlasmaIntegrationPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void PlasmaIntegrationPlugin::Load(OpenRGBPluginAPIInterface* plugin_api)
{
    api = plugin_api;
    qRegisterMetaType<LightState>("LightState");
    settings = PluginSettings::fromJson(api->GetSettings(SettingsKey));

    writer = new DeviceWriter();
    writer->moveToThread(&writerThread);
    connect(&writerThread, &QThread::finished, writer, &QObject::deleteLater);
    writerThread.start();

    engine = std::make_unique<LightingEngine>([this](const std::string& key, const LightState& target) {
        QMetaObject::invokeMethod(writer, "enqueue", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(key)), Q_ARG(LightState, target));
    });
    engine->setSettings(settings);
    connect(writer, &DeviceWriter::written, this, [this](const QString& key, const LightState& applied) {
        if(engine)
        {
            engine->onWriteFinished(key.toStdString(), applied);
        }
    });

    tab = new SettingsTab();
    setup = new SystemSetup(this);
    probe = new PowerDevilProbe(this);
    backlight = new UledsBacklight();
    backlight->setParent(this);
    accentSource = new AccentColorSource(AccentColorSource::defaultPath(), this);

    connect(backlight, &UledsBacklight::levelChanged, this, [this](int level) { engine->setLevel(level); });
    connect(accentSource, &AccentColorSource::accentChanged, this, [this](bool, unsigned int) {
        applyAccent();
        updateStatus();
    });
    connect(tab, &SettingsTab::setupRequested, setup, &SystemSetup::runSetup);
    connect(setup, &SystemSetup::setupFinished, this, [this](bool success, const QString& message) {
        if(success)
        {
            tab->setManualCommands(QString());
            openBacklight();
        }
        else
        {
            tab->setManualCommands(message + QLatin1Char('\n') + SystemSetup::manualCommands());
        }
        updateStatus();
    });
    connect(tab, &SettingsTab::restartRequested, probe, &PowerDevilProbe::restartPowerDevil);
    connect(tab, &SettingsTab::shown, this, [this] { probe->check(backlight->status() == UledsBacklight::Status::Ready); });
    connect(probe, &PowerDevilProbe::restartNeededChanged, this, [this](bool) { updateStatus(); });
    connect(tab, &SettingsTab::accentToggled, this, [this](bool enabled) {
        settings.accentEnabled = enabled;
        saveSettings();
        applyAccent();
    });
    connect(tab, &SettingsTab::deviceOptionsChanged, this, [this](const QString& key, bool dim, bool accent) {
        settings.devices[key.toStdString()] = DeviceOptions{dim, accent};
        saveSettings();
        engine->setSettings(settings);
    });

    tab->setAccentEnabled(settings.accentEnabled);
    refreshLights();
    accentSource->start();
    openBacklight();
    api->LogEntry(__FILE__, __LINE__, LL_INFO, "[PlasmaIntegration] loaded, %zu devices", lights.size());
}

QWidget* PlasmaIntegrationPlugin::GetWidget()
{
    return tab;
}

QMenu* PlasmaIntegrationPlugin::GetTrayMenu()
{
    return nullptr;
}

void PlasmaIntegrationPlugin::Unload()
{
    if(!api)
    {
        return;
    }
    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        controller->UnregisterUpdateCallback(this);
    }
    keysByController.clear();
    if(backlight)
    {
        backlight->close();
    }
    writerThread.quit();
    writerThread.wait();
    writer = nullptr;
    engine.reset();
    lights.clear();
    api = nullptr;
}

void PlasmaIntegrationPlugin::OnProfileAboutToLoad()
{
}

void PlasmaIntegrationPlugin::OnProfileLoad(nlohmann::json)
{
}

nlohmann::json PlasmaIntegrationPlugin::OnProfileSave()
{
    return nlohmann::json();
}

unsigned char* PlasmaIntegrationPlugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int* pkt_size)
{
    if(pkt_size)
    {
        *pkt_size = 0;
    }
    return nullptr;
}

void PlasmaIntegrationPlugin::ProfileManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if(update_reason != RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED || !engine)
    {
        return;
    }
    if(QThread::currentThread() == thread())
    {
        refreshLights();
    }
    else
    {
        QMetaObject::invokeMethod(this, [this] { refreshLights(); }, Qt::BlockingQueuedConnection);
    }
}

void PlasmaIntegrationPlugin::SettingsManagerUpdated(unsigned int)
{
}

void PlasmaIntegrationPlugin::controllerCallback(void* plugin, unsigned int reason, void* controller)
{
    auto* self = static_cast<PlasmaIntegrationPlugin*>(plugin);
    switch(reason)
    {
        case RGBCONTROLLER_UPDATE_REASON_UPDATELEDS:
        case RGBCONTROLLER_UPDATE_REASON_UPDATEMODE:
        case RGBCONTROLLER_UPDATE_REASON_SAVEMODE:
            QMetaObject::invokeMethod(self, [self, controller] { self->onControllerChanged(controller); }, Qt::QueuedConnection);
            break;
        case RGBCONTROLLER_UPDATE_REASON_HIDDEN:
        case RGBCONTROLLER_UPDATE_REASON_UNHIDDEN:
        case RGBCONTROLLER_UPDATE_REASON_CONFIGUREZONE:
        case RGBCONTROLLER_UPDATE_REASON_DEVICE_CHANGED:
            QMetaObject::invokeMethod(self, [self] { self->refreshLights(); }, Qt::QueuedConnection);
            break;
        default:
            break;
    }
}

void PlasmaIntegrationPlugin::refreshLights()
{
    if(!engine)
    {
        return;
    }
    keysByController.clear();
    lights.clear();
    std::vector<std::shared_ptr<Light>> handles;
    std::vector<SettingsTab::DeviceRow> rows;
    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        controller->UnregisterUpdateCallback(this);
        if(controller->GetHidden())
        {
            continue;
        }
        auto light = std::make_shared<OpenRGBLight>(controller);
        controller->RegisterUpdateCallback(&PlasmaIntegrationPlugin::controllerCallback, this);
        keysByController[controller] = light->key();
        lights.push_back(light);
        handles.push_back(light);
        rows.push_back({light->key(), light->name()});
    }
    QMetaObject::invokeMethod(writer, [this, handles] { writer->setLights(handles); }, Qt::BlockingQueuedConnection);
    engine->setLights(handles);
    tab->setDevices(rows, settings);
}

void PlasmaIntegrationPlugin::onControllerChanged(void* controller)
{
    const auto found = keysByController.find(controller);
    if(found != keysByController.end() && engine)
    {
        engine->onLightChanged(found->second);
    }
}

void PlasmaIntegrationPlugin::openBacklight()
{
    if(SystemSetup::detect(SystemSetup::systemPaths()) == SystemSetup::State::Ready)
    {
        backlight->open();
    }
    updateStatus();
    checkPowerDevilSoon();
}

void PlasmaIntegrationPlugin::checkPowerDevilSoon()
{
    QTimer::singleShot(3000, this, [this] { probe->check(backlight->status() == UledsBacklight::Status::Ready); });
}

void PlasmaIntegrationPlugin::updateStatus()
{
    StatusInput input;
    input.setup = SystemSetup::detect(SystemSetup::systemPaths());
    input.backlight = backlight->status();
    input.accentAvailable = accentSource->current().has_value();
    input.powerDevilNeedsRestart = probe->restartNeeded();
    tab->setStatusLines(statusLines(input));
    tab->setSetupVisible(input.setup == SystemSetup::State::NeedsSetup && input.backlight != UledsBacklight::Status::Ready);
    tab->setRestartVisible(input.powerDevilNeedsRestart);
}

void PlasmaIntegrationPlugin::saveSettings()
{
    api->SetSettings(SettingsKey, settings.toJson());
    api->SaveSettings();
}

void PlasmaIntegrationPlugin::applyAccent()
{
    if(engine)
    {
        engine->setAccent(settings.accentEnabled ? accentSource->current() : std::nullopt);
    }
}
```

Register in `src/src.pro`: add `OpenRGBLight.h` to `HEADERS` and `OpenRGBLight.cpp` to `SOURCES` (all other `src` files were added by earlier tasks; verify `HEADERS` lists every header in `src/` and `SOURCES` every `.cpp`).

- [ ] **Step 3: Build and run every test**

Run: `(cd build && make -j"$(nproc)" 2>&1 | grep -E 'error|warning' ; true) && build/tests/plasma-integration-tests`
Expected: no compiler errors; all test classes PASS, including `TestPluginMetadata`.

- [ ] **Step 4: Check the plugin has no unexpected undefined symbols**

Run: `nm -D --undefined-only build/src/libOpenRGBPlasmaPlugin.so | grep ' U ' | grep -v -E '@(Qt_6|GLIBC|GLIBCXX|CXXABI|GCC)' | c++filt`
Expected: no lines (every undefined symbol is versioned Qt or C/C++ runtime; the plugin only reaches OpenRGB through virtual interfaces). If `LogManager::` or `RGBController::` symbols appear, a non-virtual OpenRGB function is being called; replace it with the interface call.

- [ ] **Step 5: Commit**

```bash
git add src
git commit -m "Wire the plugin to OpenRGB, the backlight and Plasma"
```

---

### Task 11: End-to-end check on OpenRGB 1.0

**Files:**
- Create: `scripts/dev-run.sh`, `scripts/sdk_check.py`

**Interfaces:**
- Consumes: the built plugin (`build/src/libOpenRGBPlasmaPlugin.so`).
- Produces: repeatable dev commands; a verified plugin.

This task needs the person at the PC for the password prompt, the slider, and looking at the lights. It stops the installed OpenRGB while it runs, so Home Assistant loses control until the last step.

- [ ] **Step 1: Build OpenRGB release_1.0 in its own folder**

```bash
git -C ~/git/OpenRGB fetch origin tag release_1.0
git -C ~/git/OpenRGB worktree add ~/git/OpenRGB-1.0 release_1.0
mkdir -p ~/git/OpenRGB-1.0/build
(cd ~/git/OpenRGB-1.0/build && qmake6 ../OpenRGB.pro PREFIX=/usr && make -j"$(nproc)")
~/git/OpenRGB-1.0/build/openrgb --version | head -3
```

Expected: version `1.0`. (A fresh worktree avoids the stale LTO files that live in `~/git/OpenRGB`'s own tree.)

- [ ] **Step 2: Write the dev scripts**

`scripts/dev-run.sh`:

```bash
#!/bin/bash
# Runs a release_1.0 OpenRGB with this plugin in an isolated config folder.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
config="$HOME/.config/OpenRGB-plasma-dev"
mkdir -p "$config/plugins"
cp "$repo/build/src/libOpenRGBPlasmaPlugin.so" "$config/plugins/"
exec "${OPENRGB_BIN:-$HOME/git/OpenRGB-1.0/build/openrgb}" --config "$config" --startminimized --server --server-host 127.0.0.1 --server-port 6743 -v --loglevel 4
```

`scripts/sdk_check.py`:

```python
"""Read or change one OpenRGB device over the SDK: sdk_check.py NAME [--color RRGGBB] [--mode MODE]."""

import argparse

from openrgb import OpenRGBClient
from openrgb.utils import RGBColor

parser = argparse.ArgumentParser()
parser.add_argument("name")
parser.add_argument("--color")
parser.add_argument("--mode")
parser.add_argument("--modes", action="store_true")
parser.add_argument("--port", type=int, default=6743)
args = parser.parse_args()

client = OpenRGBClient("127.0.0.1", args.port, "sdk-check")
device = next(d for d in client.devices if d.name == args.name)
if args.mode:
    device.set_mode(args.mode)
if args.color:
    device.set_color(RGBColor.fromHEX(args.color), True)
reader = OpenRGBClient("127.0.0.1", args.port, "sdk-check-read")
device = next(d for d in reader.devices if d.name == args.name)
color = device.colors[0]
print(f"{device.name}: mode={device.modes[device.active_mode].name} color0={color.red},{color.green},{color.blue}")
if args.modes:
    for mode in device.modes:
        print(f"  {mode.name}: flags={mode.flags} brightness {mode.brightness_min}..{mode.brightness_max} = {mode.brightness}")
```

```bash
chmod +x scripts/dev-run.sh
python3 -m venv ~/.local/share/orgb-plasma-venv
~/.local/share/orgb-plasma-venv/bin/pip install "git+https://github.com/TRusselo/openrgb-python@set-mode-tracks-requested-mode"
```

- [ ] **Step 3: Start the dev OpenRGB with the plugin**

```bash
for p in $(pgrep -x openrgb); do kill "$p"; done
setsid -f scripts/dev-run.sh > /tmp/openrgb-plasma-dev.log 2>&1
sleep 15
grep -E 'PlasmaIntegration|PluginManager' /tmp/openrgb-plasma-dev.log | head
```

Expected: a `[PlasmaIntegration] loaded, N devices` line. The person confirms a "Plasma Integration" tab with one row per device, both columns ticked, accent switch off.

- [ ] **Step 4: One-time setup**

The person clicks **Set up** and enters their password. Then:

```bash
cat /etc/modules-load.d/openrgb-kbd-backlight.conf /etc/udev/rules.d/70-openrgb-kbd-backlight.rules
getfacl -p /dev/uleds | grep '^user:'
ls /sys/class/leds/ | grep openrgb
```

Then record what level the backlight started at (spec §11) and whether any light changed when it appeared:

```bash
cat /sys/class/leds/openrgb::kbd_backlight/brightness
busctl --system call org.freedesktop.UPower /org/freedesktop/UPower/KbdBacklight org.freedesktop.UPower.KbdBacklight GetBrightness
```

Expected: `uleds`, the rule line, `user:<name>:rw-`, and `openrgb::kbd_backlight`; note the two brightness values in `docs/verification.md`. The tab shows "Ready: …" and (stock PowerDevil 6.7.5) "PowerDevil has not picked up the backlight yet." with the restart button.

- [ ] **Step 5: PowerDevil**

The person clicks **Restart Plasma power management**. Then:

```bash
busctl --user call org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl keyboardBrightnessMax
```

Expected: `i 100`; within ~5 s the restart line and button disappear; the Brightness & Color widget shows a Keyboard Backlight row.

- [ ] **Step 6: Last actor wins, checked over the SDK**

```bash
venv=~/.local/share/orgb-plasma-venv/bin/python
$venv scripts/sdk_check.py "Razer Goliathus" --mode Direct --color ff0000
busctl --system call org.freedesktop.UPower /org/freedesktop/UPower/KbdBacklight org.freedesktop.UPower.KbdBacklight SetBrightness i 60; sleep 1
$venv scripts/sdk_check.py "Razer Goliathus"
$venv scripts/sdk_check.py "Razer Goliathus" --color 00ff00
$venv scripts/sdk_check.py "Razer Goliathus"
busctl --system call org.freedesktop.UPower /org/freedesktop/UPower/KbdBacklight org.freedesktop.UPower.KbdBacklight SetBrightness i 100; sleep 1
$venv scripts/sdk_check.py "Razer Goliathus"
```

Expected, in order: `255,0,0`; `153,0,0` (slider 60 dims it); `0,255,0` (outside change shows exactly); `0,255,0` (still exact); `0,255,0` (slider 100). The person confirms the mat matches each step. If `SetBrightness` is refused, set the widget's Keyboard Backlight slider to 60 and 100 instead.

Record the real brightness ranges of modes with a brightness setting (spec §11):

```bash
for name in "Razer Blackwidow Elite" "Razer Goliathus" "Razer Goliathus Extended" "Z390 AORUS ELITE-CF"; do $venv scripts/sdk_check.py "$name" --modes; done
```

- [ ] **Step 7: Accent color, Off mode and the change signal**

```bash
old=$(kreadconfig6 --file kdeglobals --group General --key AccentColor)
dbus-monitor --session "type='signal',interface='org.kde.kconfig.notify'" > /tmp/kconfig-signals.log 2>&1 &
echo $! > /tmp/kconfig-monitor.pid
```

The person ticks **Follow Plasma's accent color** in the tab. Then:

```bash
kwriteconfig6 --file kdeglobals --group General --key AccentColor --notify 0,128,255; sleep 2
$venv scripts/sdk_check.py "Razer Goliathus"
$venv scripts/sdk_check.py "Razer Goliathus Extended" --mode Off
kwriteconfig6 --file kdeglobals --group General --key AccentColor --notify 255,128,0; sleep 2
$venv scripts/sdk_check.py "Razer Goliathus Extended"
grep -c 'member=ConfigChanged' /tmp/kconfig-signals.log
kill "$(cat /tmp/kconfig-monitor.pid)"
if [ -n "$old" ]; then kwriteconfig6 --file kdeglobals --group General --key AccentColor --notify "$old"; else kwriteconfig6 --file kdeglobals --group General --key AccentColor --delete --notify; fi
```

Expected: Goliathus `0,128,255`; Goliathus Extended stays `mode=Off`; `ConfigChanged` count ≥ 2 (confirms spec §11's signal). If the count is 0, the file watcher handled it; note that in the spec.

- [ ] **Step 8: Per-device Dim and idle**

The person unticks **Dim** for "Razer Blackwidow Elite", moves the Plasma slider down to about 20%, and confirms the keyboard stays bright while the mats dim. Then re-ticks it and confirms the keyboard dims. Then waits for the screen to dim on idle (or sets a 1-minute dim timeout in Power Management settings) and confirms the lights go dark and return on mouse move.

- [ ] **Step 8b: Review scenarios (added after the final review)**

With the dev OpenRGB still running and the slider at about 40%:
1. Click **Rescan Devices** in OpenRGB with the accent switch on: every light comes back dimmed and in the accent color.
2. Unplug and replug one USB RGB device: it reappears in the tab and takes the current level.
3. Start a second OpenRGB window (`~/git/OpenRGB-1.0/build/openrgb --config ~/.config/OpenRGB-plasma-dev`): its Plasma Integration tab says another instance owns the backlight and its controls are disabled; changing the accent color does not make the lights jump.
4. Drag the slider quickly up and down for a few seconds, then set it to 100: every light returns to its full color.

- [ ] **Step 9: Shutdown and restore**

```bash
for p in $(pgrep -x openrgb); do kill "$p"; done; sleep 2
ls /sys/class/leds/ | grep openrgb || echo "backlight removed"
setsid -f /usr/bin/openrgb --startminimized --server --server-host 192.168.1.13 --server-port 6742 > /dev/null 2>&1
```

Expected: "backlight removed"; the installed OpenRGB is running again for Home Assistant.

- [ ] **Step 10: Record results and commit**

Add a short `docs/verification.md` listing each step's observed result (and the `ConfigChanged` finding), then:

```bash
git add scripts docs/verification.md
git commit -m "Add dev scripts and record the end-to-end check"
```

---

### Task 12: CI, releases and README

**Files:**
- Create: `.github/workflows/build.yml`, `.github/workflows/release.yml`, `README.md`

**Interfaces:**
- Consumes: the qmake build and test runner.
- Produces: CI on every push; a GitHub Release with `libOpenRGBPlasmaPlugin.so` for each `v*` tag.

- [ ] **Step 1: Write the workflows**

`.github/workflows/build.yml`:

```yaml
name: build

on:
  push:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: true
      - name: Install Qt
        run: sudo apt-get update && sudo apt-get install -y qt6-base-dev qt6-base-dev-tools libgl-dev build-essential
      - name: Build
        run: mkdir build && cd build && /usr/lib/qt6/bin/qmake ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)"
      - name: Test
        run: build/tests/plasma-integration-tests
      - uses: actions/upload-artifact@v4
        with:
          name: libOpenRGBPlasmaPlugin
          path: build/src/libOpenRGBPlasmaPlugin.so
```

`.github/workflows/release.yml`:

```yaml
name: release

on:
  push:
    tags:
      - "v*"

permissions:
  contents: write

jobs:
  release:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: true
      - name: Install Qt
        run: sudo apt-get update && sudo apt-get install -y qt6-base-dev qt6-base-dev-tools libgl-dev build-essential
      - name: Build
        run: mkdir build && cd build && /usr/lib/qt6/bin/qmake ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)"
      - name: Test
        run: build/tests/plasma-integration-tests
      - uses: softprops/action-gh-release@v2
        with:
          files: build/src/libOpenRGBPlasmaPlugin.so
```

Built against Ubuntu 24.04's Qt 6.4, the plugin loads in newer Qt 6 releases (Qt keeps backward binary compatibility within 6.x).

- [ ] **Step 2: Write the README**

`README.md`:

```markdown
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
```

- [ ] **Step 3: Check the workflow files parse and the README commands work locally**

Run: `python3 -c "import yaml,sys; [yaml.safe_load(open(f)) for f in sys.argv[1:]]; print('ok')" .github/workflows/build.yml .github/workflows/release.yml && rm -rf /tmp/plasma-readme-build && git clone --recursive -q . /tmp/plasma-readme-build && (cd /tmp/plasma-readme-build && mkdir build && cd build && qmake6 ../OpenRGBPlasmaPlugin.pro && make -j"$(nproc)" >/dev/null && tests/plasma-integration-tests >/dev/null && echo build-ok)`
Expected: `ok` and `build-ok`.

- [ ] **Step 4: Commit**

```bash
git add .github README.md
git commit -m "Add CI, release workflow and README"
```

- [ ] **Step 5: Publish (needs the user's go-ahead)**

Ask the user before creating the public repository. With approval:

```bash
gh repo create TRusselo/OpenRGBPlasmaPlugin --public --source . --remote origin --push
gh run watch --exit-status
```

Expected: the `build` workflow passes on GitHub.

---

### Task 13: Rollout on the development PC (user-driven)

**Files:** none in the repo.

This switches the user's machine from OpenRGB git2035 to 1.0. Every step needs the user's go-ahead; Home Assistant must keep working.

- [ ] **Step 1: Point the HA custom component at the patched openrgb-python**

In `/mnt/unraid/appdata/home-assistant/custom_components/openrgb/manifest.json`, change the requirement to `"openrgb-python @ git+https://github.com/TRusselo/openrgb-python@set-mode-tracks-requested-mode"`, restart the Home Assistant container, and confirm the integration loads.

- [ ] **Step 2: Install OpenRGB 1.0**

User runs: `sudo pacman -S openrgb` (replaces `openrgb-git`; Arch `extra/openrgb 1.0-2`). Restart OpenRGB with the usual autostart command.

- [ ] **Step 3: Confirm Home Assistant control on 1.0**

Toggle a light off and on in HA and change its color; the user confirms the device follows. Also run the race test from `techsupport/patch/openrgb-sdk-write-ordering/race_test.py` against `192.168.1.13 6742` with the patched library: expect 20/20.

- [ ] **Step 4: Install the plugin and run Set up**

OpenRGB → Settings → Plugins → Install Plugin → `libOpenRGBPlasmaPlugin.so` (from the release or `build/src`), restart OpenRGB, click **Set up**, then the PowerDevil restart button if shown.

- [ ] **Step 5: Later, return to released libraries**

When an openrgb-python release contains PR #96 and HA has bumped to it, restore the custom component's requirement to the released version (or remove the custom component once HA core includes PR #179046).
