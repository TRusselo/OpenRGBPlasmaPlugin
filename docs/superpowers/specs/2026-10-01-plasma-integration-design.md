# OpenRGB Plasma Integration plugin — design

Date: 2026-10-01
Status: approved in conversation, awaiting review of this written spec

## 1. Goal

Let KDE Plasma control the lights managed by OpenRGB the way it controls a keyboard backlight:

- **Brightness** from Plasma's Brightness & Color widget ("Keyboard Backlight" slider), keyboard brightness keys, and PowerDevil's own actions (idle dim, screen off, wake, profiles). One slider covers every backlight, including a laptop's real one.
- **Color** that follows Plasma's accent color, switchable on or off.
- **Everything reports the truth**: changes made from Plasma go through OpenRGB, so other OpenRGB clients (Home Assistant) read the real result.

It ships as an OpenRGB plugin in its own repository (`TRusselo/OpenRGBPlasmaPlugin`, shown in OpenRGB as "Plasma Integration"). No changes to OpenRGB or KDE are required, apart from the PowerDevil fix noted in §9.

## 2. Non-goals

- Color controls inside the Plasma widget (that is KDE Kameleon's area and has no external backend API).
- Moving the Plasma slider when something else changes brightness (the backlight is one-way, §9).
- Windows, macOS, Flatpak OpenRGB (no access to `/dev/uleds`), OpenRGB plugin API 4 (git builds before 1.0).
- A separate KDE service, daemon, or root process.

## 3. Background (verified on 2026-09-30/10-01)

- **uleds**: `CONFIG_LEDS_USER` lets a userspace process create an LED class device by writing `{char name[64]; int max_brightness;}` to `/dev/uleds`; every brightness written to the LED is read back from the same fd. Closing the fd removes the LED. `/dev/uleds` is `root:root 0600` and only exists after `modprobe uleds` (no auto-load alias).
- **UPower 1.91.4** picks up a `*::kbd_backlight` LED immediately, emits `DeviceAdded`/`DeviceRemoved`, and drives all keyboard backlights from one combined object.
- **PowerDevil 6.7.5** only checks for a keyboard backlight at startup (fixed by MR plasma/powerdevil!691, not merged yet). It sets the keyboard backlight to 0 on idle dim and screen off and restores it on activity and after suspend. Per-profile keyboard brightness exists but is off by default.
- **Kameleon** reads the accent color from `kdeglobals`: `[General] AccentColor`, falling back to `[Colors:View] ForegroundActive`, then white.
- **OpenRGB plugins** are a single `.so`. Users install them with Settings → Plugins → Install Plugin, which copies the file to `~/.config/OpenRGB/plugins/`. Distro packages use `/usr/lib/openrgb/plugins`. Plugins load only in GUI instances of OpenRGB.
- **OpenRGB 1.0 plugin API 5**: `OpenRGBPluginAPIInterface` gives `GetRGBControllers()`, `GetSettings()`/`SetSettings()`/`SaveSettings()`, `LogEntry()`; `RGBControllerInterface` exposes modes, per-zone modes, colors, `UpdateLEDs`/`UpdateZoneLEDs`/`UpdateMode`/`UpdateZoneMode`, and `RegisterUpdateCallback(callback(arg, reason, controller))`.

## 4. Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| OpenRGB version | 1.0, plugin API 5 only | What users run; API 4 is incompatible (different IID and `Load()`). |
| Brightness path | Fake keyboard backlight `openrgb::kbd_backlight` (0–100) via uleds | Proven in the spike; works with UPower, so PowerDevil and other desktops get it for free. |
| Color path | Plugin follows Plasma's accent color itself | Kameleon has no way to plug in an external backend. |
| Device scope | Every device, with per-device **Dim** and **Accent color** checkboxes, both on by default | Users like customization (case, fans, etc.). |
| Conflict rule | **Last actor wins** (§6) | One rule, no special case at 0, other clients read back exactly what they set. |
| Root access | One-time setup through `pkexec`, offered by the plugin | Plugins are installed as plain files, so no package can be assumed. |
| Dependencies | Qt (Core, Gui, Widgets, DBus) and OpenRGB headers only; no KDE Frameworks | Loads anywhere OpenRGB loads; brightness still works outside Plasma. |
| License | GPL-2.0-or-later | Built against OpenRGB's GPL-2.0 headers. |

## 5. Components

| Component | Responsibility | Talks to |
| --- | --- | --- |
| `PlasmaIntegrationPlugin` | Implements `OpenRGBPluginInterface`: plugin info (API 5, label "Plasma Integration", top-level tab), creates and tears down the other parts in `Load()`/`Unload()`, returns the settings tab from `GetWidget()`. | OpenRGB |
| `UledsBacklight` | Registers `openrgb::kbd_backlight` (max 100) on `/dev/uleds`, watches the fd with a `QSocketNotifier`, emits `levelChanged(int)`. Refuses to register if that LED name already exists (another OpenRGB instance) and reports it. Closing removes the LED. | kernel |
| `AccentColorSource` | Reads the accent color with Kameleon's rule from `$XDG_CONFIG_HOME/kdeglobals`, emits `accentChanged(QColor)` when it changes. Listens for Plasma's KConfig change notification on the session bus, with a file watcher as fallback. Reports "unavailable" outside Plasma. | session D-Bus, `kdeglobals` |
| `LightingEngine` | Owns per-device state and the rules in §6. Registers an update callback on every controller, ignores its own writes, decides what to write, and hands writes to the writer. Pure logic behind a small controller abstraction so it can be unit-tested with fakes. | OpenRGB controllers |
| `DeviceWriter` | Applies writes on a worker thread, coalescing per device so at most one write per device goes out every ~30 ms (the latest wins). | OpenRGB controllers |
| `SystemSetup` | Detects whether `/dev/uleds` is usable (missing module, no permission, kernel without uleds) and runs the one-time setup (§7). | `pkexec` |
| `PowerDevilProbe` | Asks PowerDevil over D-Bus whether it sees the backlight (`isActionSupported("KeyboardBrightnessControl")`, `keyboardBrightnessMax`). If our LED exists but PowerDevil reports no support or max 0, the tab offers "Restart Plasma power management" (`systemctl --user restart plasma-powerdevil`). Re-checks when the LED is created and when the tab is shown. | session D-Bus |
| `SettingsTab` | Status line, Set up button, PowerDevil restart button (when needed), accent on/off switch, device table with Dim and Accent color checkboxes. Saves through OpenRGB settings. | user |

Settings are stored under OpenRGB settings key `PlasmaIntegrationPlugin`:

```json
{ "accent_enabled": false,
  "devices": { "<device key>": { "dim": true, "accent": true } } }
```

The accent switch starts **off**, so installing the plugin never recolors anything; switching it on applies the accent (§6 event 3), and while it is on the accent is applied again each time OpenRGB starts.

**Device key**: `name|serial` when the serial is non-empty and not `none`; otherwise `name|location`. (USB paths change between boots, so the serial comes first.) Devices without a saved entry use both checkboxes on.

## 6. Behavior rules

State kept by the engine:

- `level` (0–100): last value the OS wrote to the backlight. Starts at 100 and changes nothing until the OS writes a value.
- Per device: `base` (the look last set by anyone other than the plugin's dimming: LED colors, mode colors, or mode brightness, per zone), `follows` (whether the device currently follows the slider), and the `expected` state of the plugin's last write.

Events:

1. **Level change** (slider, brightness keys, PowerDevil idle/wake/profile): `level := v`. Every device ticked **Dim** gets `follows := true` and is rendered at `level`.
2. **Outside change** (update callback whose state does not match `expected`; e.g. HA, the OpenRGB window, a profile load): `base :=` the device's current state, `follows := false`. Nothing is written. The device shows exactly what was set until the next level change.
3. **Accent change** (accent changed, or accent switched on): every device ticked **Accent color** gets the accent as its `base` color, then is rendered at `level` if ticked **Dim** (`follows := true`) or at full otherwise. The mode is never changed: a device in **Off** stays off.
4. **Accent switched off**: nothing is written.
5. **Dim unticked** on a device: render its `base` at full, `follows := false`. **Dim ticked**: `follows := true`, render at `level`.
6. **Device added** (startup, hotplug, rescan): `base :=` current state, with the accent applied when the accent switch is on and the device is ticked **Accent color**; `follows :=` its Dim setting; rendered at `level`. A known device whose zone layout changed (zone count or LEDs per zone, e.g. a resized ARGB zone) is treated as added, so its base is read fresh. A known device that comes back from a rescan with the same layout keeps its base and is rendered again, because OpenRGB re-creates it black.
6a. **Unlit device** (every colour slot black since the device appeared; OpenRGB cannot read Razer and most other hardware back, so plugged-in and rescanned devices start that way): on device add and on each level change, every unlit device ticked **Dim** takes the blend of the colours of the devices that have one (each device counted once; the average hue at the inputs' typical brightness) and is rendered at `level`. With nothing lit yet it stays black and is tried again on the next add or level change. Black set later by anyone (HA, the OpenRGB window, a profile) is an outside change and is kept. Devices in a mode with no colours (e.g. **Off**) are never unlit.
7. **Device removed**: state dropped, settings kept.

**Rendering** a device at a level, per zone (using the zone's own active mode if the device supports per-zone modes, else the device mode):

| Mode color type | What is scaled |
| --- | --- |
| `MODE_COLORS_PER_LED` (Direct, Custom) | each LED color = base color × level/100 |
| `MODE_COLORS_MODE_SPECIFIC` (Static, Breathing, …) | each mode color = base mode color × level/100 |
| `MODE_COLORS_NONE`/`RANDOM` with `MODE_FLAG_HAS_BRIGHTNESS` | mode brightness = min + (base brightness − min) × level/100 |
| anything else (Off, effects without brightness) | nothing |

Scaling is per channel, rounded. Level 0 renders black (or minimum brightness) but never switches the mode, so the next level restores the exact look.

**Own-write detection**: each write carries a sequence number and only the newest one's acknowledgement counts. The engine remembers its last 8 targets per device; a callback whose state equals any of them (including late echoes when OpenRGB runs as a client of a server) is ignored. An outside client writing exactly the same values is indistinguishable and harmless.

**Threading**: controller callbacks arrive on OpenRGB threads and are queued to the engine's thread (`Qt::QueuedConnection`) before any state is touched.

## 7. One-time setup

Shown when `/dev/uleds` is missing or not accessible and the kernel has the module. "Set up" runs, through `pkexec /bin/sh -c '…'`:

```sh
echo uleds > /etc/modules-load.d/openrgb-kbd-backlight.conf
echo 'KERNEL=="uleds", TAG+="uaccess"' > /etc/udev/rules.d/70-openrgb-kbd-backlight.rules
modprobe uleds
udevadm control --reload
udevadm trigger --name-match=uleds
```

Then the plugin retries opening `/dev/uleds`. If `pkexec` is missing or the prompt is cancelled, the tab shows these commands to run by hand. A future AUR package can install the same two files under `/usr/lib`, making the button unnecessary.

If the backlight LED already exists when the plugin loads, another OpenRGB instance owns it (e.g. a second window started as a client of the first). That instance stays **passive**: no device callbacks, no accent, no writes, tab controls disabled, with a status line saying so.

Status line states: **Ready**, **Needs setup**, **This kernel has no uleds** (brightness disabled, accent still works), **Another OpenRGB instance owns the backlight**, **Accent color unavailable (not running in Plasma)**, **PowerDevil has not picked up the backlight** (with the restart button).

## 8. Testing

- **Unit tests (Qt Test)** for `LightingEngine` against fake controllers, no hardware: every event in §6, scaling for each mode type and per-zone modes, own-write detection, rounding at 0 and 100, device keys and settings defaults.
- **`AccentColorSource` tests** with temporary `kdeglobals` files (custom accent, scheme fallback, missing file).
- **On the development PC** (OpenRGB 1.0 with the plugin):
  - D-Bus checks like the PowerDevil A–D table: LED appears, PowerDevil sees max 100, slider moves reach the plugin, LED disappears when OpenRGB quits.
  - SDK readback (openrgb-python with the #96 fix) confirming other clients see exactly what was rendered, and that an outside change is left alone until the slider moves.
  - Visual checks: slider, brightness keys, idle dim / screen off / wake, accent change, Off mode stays off.
- **CI (GitHub Actions)**: build against OpenRGB `release_1.0` and run the unit tests on every push.

## 9. Build, release, rollout

- **Build**: qmake project like `OpenRGBDevelopers/OpenRGBSamplePlugin`, OpenRGB as a git submodule pinned to `release_1.0`, C++17, Linux only. Code comments one line or none.
- **Release**: on each git tag, GitHub Actions builds `libOpenRGBPlasmaPlugin.so` and attaches it to a GitHub Release. Users install it with OpenRGB → Settings → Plugins → Install Plugin. A GitLab mirror and an AUR package can follow.
- **Rollout on the development PC** (needs OpenRGB 1.0, which breaks HA control until openrgb-python ships #96):
  1. Point the HA OpenRGB custom component's requirement at the patched openrgb-python (TRusselo fork, branch `set-mode-tracks-requested-mode`), restart HA, confirm control works against a 1.0 test build.
  2. Install OpenRGB 1.0, then the plugin, run Set up.
  3. Switch the requirement back to the released openrgb-python once a release contains #96.

## 10. Known limitations

- The Plasma slider does not move when HA or OpenRGB changes brightness: uleds cannot report changes back to the kernel/UPower.
- Until PowerDevil !691 ships, a backlight created after login (OpenRGB autostarts after Plasma) needs one PowerDevil restart per login; the tab offers it.
- Idle dim and screen off turn the lights off and back on. This follows PowerDevil's keyboard backlight behavior and cannot be told apart from the slider.
- Needs a kernel with `CONFIG_LEDS_USER`.

## 11. To verify during implementation

- What sets the backlight's level when it first appears (a value of 10 was seen once after a PowerDevil restart; possibly UPower restoring a saved level). The plugin treats any OS value as a level change, so if the OS restores a low level at login the lights follow it.
- The exact session D-Bus signal Plasma emits when `kdeglobals` changes (KConfigWatcher), and whether the file-watcher fallback is needed.
- `pkexec` prompting from inside OpenRGB (polkit agent available in the Plasma session).
- Whether controller write calls are safe from a worker thread in OpenRGB 1.0, or must run on OpenRGB's thread.
- Real brightness ranges of modes with `MODE_FLAG_HAS_BRIGHTNESS` on the test hardware.
