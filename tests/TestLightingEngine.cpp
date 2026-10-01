#include "TestLightingEngine.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QTest>

#include "Dimming.h"
#include "FakeLight.h"
#include "LightBuilders.h"
#include "LightingEngine.h"

namespace
{
class Harness
{
public:
    Harness()
        : engine([this](const std::string& key, const LightState& target, std::uint64_t sequence, bool restoreMode) { requests.push_back({key, target, sequence, restoreMode}); })
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
        for(const Request& request : pending)
        {
            lights[request.first]->apply(request.second, request.restoreMode);
            engine.onLightChanged(request.first);
            engine.onWriteFinished(request.first, request.sequence);
        }
    }

    void outsideChange(const std::string& key, const LightState& state)
    {
        lights[key]->state = state;
        engine.onLightChanged(key);
    }

    void acknowledge(std::size_t index)
    {
        engine.onWriteFinished(requests[index].first, requests[index].sequence);
    }

    void republish()
    {
        publish();
    }

    std::optional<LightState> lastRequestFor(const std::string& key) const
    {
        std::optional<LightState> found;
        for(const Request& request : requests)
        {
            if(request.first == key)
            {
                found = request.second;
            }
        }
        return found;
    }

    struct Request
    {
        std::string first;
        LightState second;
        std::uint64_t sequence = 0;
        bool restoreMode = false;
    };

    std::vector<Request> requests;
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
const Rgb Black = makeRgb(0, 0, 0);
const Rgb Green = makeRgb(0, 255, 0);
const Rgb Blue = makeRgb(0, 0, 255);
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
    const auto request = h.requests[0];
    const LightState target = request.second;
    h.requests.clear();

    h.lights["keyboard"]->apply(target, false);
    h.lights["keyboard"]->state = directLight({Red});
    h.engine.onLightChanged("keyboard");
    h.engine.onWriteFinished("keyboard", request.sequence);

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
    const std::uint64_t firstSequence = h.requests[0].sequence;
    h.engine.setLevel(20);
    QCOMPARE(int(h.requests.size()), 2);
    const LightState second = h.requests[1].second;
    const std::uint64_t secondSequence = h.requests[1].sequence;
    h.requests.clear();

    h.lights["keyboard"]->apply(first, false);
    h.engine.onWriteFinished("keyboard", firstSequence);
    h.engine.onLightChanged("keyboard");
    QCOMPARE(h.engine.follows("keyboard"), true);

    h.lights["keyboard"]->apply(second, false);
    h.engine.onWriteFinished("keyboard", secondSequence);
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
    h.engine.onWriteFinished("keyboard", 1);
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

void TestLightingEngine::staleEchoOfOlderWriteIsIgnored()
{
    Harness h;
    h.add("keyboard", directLight({makeRgb(200, 0, 0)}));
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 1);
    const LightState older = h.requests[0].second;
    h.engine.setLevel(25);
    QCOMPARE(int(h.requests.size()), 2);
    h.completeWrites();

    h.lights["keyboard"]->state = older;
    h.engine.onLightChanged("keyboard");

    QCOMPARE(h.engine.base("keyboard"), std::optional<LightState>(directLight({makeRgb(200, 0, 0)})));
    h.engine.setLevel(100);
    h.completeWrites();
    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(200, 0, 0)}));
}

void TestLightingEngine::outOfOrderAckIsIgnored()
{
    Harness h;
    h.add("keyboard", directLight({makeRgb(200, 0, 0)}));
    h.engine.setLevel(50);
    h.engine.setLevel(60);
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 3);

    h.lights["keyboard"]->apply(h.requests[0].second, false);
    h.lights["keyboard"]->apply(h.requests[1].second, false);
    h.acknowledge(0);
    h.lights["keyboard"]->apply(h.requests[2].second, false);
    h.engine.onLightChanged("keyboard");
    h.acknowledge(1);
    h.acknowledge(2);
    h.requests.clear();

    QCOMPARE(h.engine.follows("keyboard"), true);
    h.engine.setLevel(100);
    h.completeWrites();
    QCOMPARE(h.lights["keyboard"]->state, directLight({makeRgb(200, 0, 0)}));
}

void TestLightingEngine::reshapedLightReadsFreshBase()
{
    Harness h;
    h.add("strip", directLight({makeRgb(200, 0, 0)}));
    h.engine.setLevel(50);
    h.completeWrites();

    h.lights["strip"]->state = directLight({makeRgb(10, 20, 30), makeRgb(40, 50, 60)});
    h.republish();

    QCOMPARE(h.engine.base("strip"), std::optional<LightState>(directLight({makeRgb(10, 20, 30), makeRgb(40, 50, 60)})));
}

void TestLightingEngine::newLightGetsAccent()
{
    Harness h;
    h.engine.setAccent(Accent);
    h.add("keyboard", directLight({Orange}));
    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(h.requests[0].second, directLight({Accent}));
}

void TestLightingEngine::newLightWithoutAccentTickIsLeftAlone()
{
    Harness h;
    PluginSettings settings;
    settings.devices["keyboard"] = DeviceOptions{true, false};
    h.engine.setSettings(settings);
    h.engine.setAccent(Accent);
    h.add("keyboard", directLight({Orange}));
    QCOMPARE(int(h.requests.size()), 0);
}

void TestLightingEngine::newBlackLightTakesOthersColor()
{
    Harness h;
    h.add("keyboard", directLight({Red}));
    h.add("mouse", directLight({Black, Black}));

    const std::optional<LightState> mouse = h.lastRequestFor("mouse");
    QVERIFY(mouse.has_value());
    QCOMPARE(*mouse, directLight({Red, Red}));
    QCOMPARE(h.engine.follows("mouse"), true);
}

void TestLightingEngine::newBlackLightIsDimmedToLevel()
{
    Harness h;
    h.engine.setLevel(40);
    h.add("keyboard", directLight({Red}));
    h.add("mouse", directLight({Black}));

    const std::optional<LightState> mouse = h.lastRequestFor("mouse");
    QVERIFY(mouse.has_value());
    QCOMPARE(*mouse, directLight({makeRgb(102, 0, 0)}));
}

void TestLightingEngine::unlitLightFillsOnNextSliderMove()
{
    Harness h;
    h.add("mouse", directLight({Black}));
    h.add("keyboard", directLight({Black}));
    QCOMPARE(int(h.requests.size()), 0);
    h.outsideChange("keyboard", directLight({Red}));

    h.engine.setLevel(50);

    const std::optional<LightState> mouse = h.lastRequestFor("mouse");
    QVERIFY(mouse.has_value());
    QCOMPARE(*mouse, directLight({makeRgb(128, 0, 0)}));
}

void TestLightingEngine::turnedOffLightStaysOff()
{
    Harness h;
    h.add("keyboard", directLight({Red}));
    h.add("mouse", directLight({Red}));
    h.outsideChange("mouse", directLight({Black}));

    h.engine.setLevel(50);

    QVERIFY(!h.lastRequestFor("mouse").has_value());
    QCOMPARE(h.lights["mouse"]->state, directLight({Black}));
}

void TestLightingEngine::untickedBlackLightIsLeftAlone()
{
    Harness h;
    PluginSettings settings;
    settings.devices["mouse"] = DeviceOptions{false, true};
    h.engine.setSettings(settings);
    h.add("keyboard", directLight({Red}));
    h.add("mouse", directLight({Black}));

    h.engine.setLevel(50);

    QVERIFY(!h.lastRequestFor("mouse").has_value());
}

void TestLightingEngine::offModeLightIsLeftAlone()
{
    Harness h;
    h.add("keyboard", directLight({Red}));
    h.add("g703", lightWithMode(offMode(1), {Black, Black}));

    h.engine.setLevel(50);

    QVERIFY(!h.lastRequestFor("g703").has_value());
}

void TestLightingEngine::accentClearsUnlit()
{
    Harness h;
    PluginSettings settings;
    settings.devices["keyboard"] = DeviceOptions{true, false};
    h.engine.setSettings(settings);
    h.add("mouse", directLight({Black}));
    h.add("keyboard", directLight({Black}));
    h.engine.setAccent(Accent);
    h.completeWrites();
    h.outsideChange("keyboard", directLight({Red}));

    h.engine.setLevel(50);

    const std::optional<LightState> mouse = h.lastRequestFor("mouse");
    QVERIFY(mouse.has_value());
    QCOMPARE(*mouse, directLight({scaleColor(Accent, 50)}));
}

void TestLightingEngine::rescannedLightGetsStateBack()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.engine.setLevel(50);
    h.completeWrites();

    h.add("keyboard", directLight({Black}));

    const std::optional<LightState> keyboard = h.lastRequestFor("keyboard");
    QVERIFY(keyboard.has_value());
    QCOMPARE(*keyboard, directLight({makeRgb(100, 50, 0)}));
}

void TestLightingEngine::emptiedListRestoresRememberedState()
{
    Harness h;
    h.add("keyboard", directLight({Orange}));
    h.add("mat", directLight({Red}));
    const LightState off = lightWithMode(offMode(1), {Red});
    h.outsideChange("mat", off);
    h.engine.setLevel(50);
    h.completeWrites();
    h.remove("keyboard");
    h.remove("mat");

    h.add("keyboard", directLight({Black}));
    h.add("mat", directLight({Black}));

    const std::optional<LightState> keyboard = h.lastRequestFor("keyboard");
    const std::optional<LightState> mat = h.lastRequestFor("mat");
    QVERIFY(keyboard.has_value() && mat.has_value());
    QCOMPARE(*keyboard, directLight({makeRgb(100, 50, 0)}));
    QCOMPARE(*mat, off);
}

void TestLightingEngine::replugRestoresOwnColor()
{
    Harness h;
    h.add("keyboard", directLight({Red}));
    h.add("dock", directLight({Green}));
    h.remove("dock");

    h.add("dock", directLight({Black}));

    const std::optional<LightState> dock = h.lastRequestFor("dock");
    QVERIFY(dock.has_value());
    QCOMPARE(*dock, directLight({Green}));
}

void TestLightingEngine::departedLightFollowsLevelChange()
{
    Harness h;
    h.add("keyboard", directLight({Red}));
    h.add("mat", directLight({Red}));
    h.outsideChange("mat", directLight({Blue}));
    h.remove("mat");
    h.engine.setLevel(50);

    h.add("mat", directLight({Black}));

    const std::optional<LightState> mat = h.lastRequestFor("mat");
    QVERIFY(mat.has_value());
    QCOMPARE(*mat, directLight({makeRgb(0, 0, 128)}));
}

void TestLightingEngine::departedLightGetsAccentChange()
{
    Harness h;
    h.add("mat", directLight({Red}));
    h.remove("mat");
    h.engine.setAccent(Accent);

    h.add("mat", directLight({Black}));

    const std::optional<LightState> mat = h.lastRequestFor("mat");
    QVERIFY(mat.has_value());
    QCOMPARE(*mat, directLight({Accent}));
}

void TestLightingEngine::returningLightRestoresItsMode()
{
    Harness h;
    h.add("mat", directLight({Red}));
    h.outsideChange("mat", lightWithMode(offMode(1), {Red}));
    h.remove("mat");

    h.add("mat", directLight({Black}));

    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(h.requests[0].restoreMode, true);
    h.completeWrites();
    h.engine.setLevel(50);
    QCOMPARE(int(h.requests.size()), 0);
}

void TestLightingEngine::levelWritesNeverChangeMode()
{
    Harness h;
    h.add("keyboard", directLight({Red}));

    h.engine.setLevel(50);

    QCOMPARE(int(h.requests.size()), 1);
    QCOMPARE(h.requests[0].restoreMode, false);
}
