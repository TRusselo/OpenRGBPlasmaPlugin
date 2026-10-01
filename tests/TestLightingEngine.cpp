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
    QCOMPARE(h.engine.base("keyboard"), std::optional<LightState>(directLight({Red})));
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
    QCOMPARE(h.engine.base("keyboard"), std::optional<LightState>(directLight({Orange})));
}

void TestLightingEngine::outsideWriteDuringOwnWriteIsDetected()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 1);
    const LightState target = h.requests[0].second;
    h.requests.clear();

    h.lights["keyboard"]->apply(target);
    h.lights["keyboard"]->state = directLight({Red});
    h.engine.onLightChanged("keyboard");
    h.engine.onWriteFinished("keyboard", target);

    QCOMPARE(h.engine.follows("keyboard"), false);
    QCOMPARE(h.engine.base("keyboard"), std::optional<LightState>(directLight({Red})));
}

void TestLightingEngine::supersededWriteKeepsWaiting()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 1);
    const LightState first = h.requests[0].second;
    h.engine.setLevel(20);
    QCOMPARE(int(h.requests.size()), 2);
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
    QCOMPARE(h.engine.base("keyboard"), std::optional<LightState>(directLight({Orange})));
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
    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(100, 50, 0)}));

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
