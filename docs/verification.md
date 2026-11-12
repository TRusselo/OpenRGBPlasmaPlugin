# End-to-end check — 2026-10-01

Plan Task 11, run on the development PC against OpenRGB `release_1.0` (81bbe18) built from source, with the plugin in an isolated config folder (`scripts/dev-run.sh`).

Environment: CachyOS, kernel 7.2.8, Plasma 6.7.5 (Wayland), PowerDevil 6.7.5, UPower 1.91.4. Hardware: Razer BlackWidow Elite, Naga Epic Chroma, Mouse Dock Chroma, Goliathus and Goliathus Extended; Gigabyte Z390 AORUS ELITE ARGB; Logitech G703.

Device state was read back over the SDK with `scripts/sdk_check.py`; the person at the PC confirmed what the hardware showed.

## Results

| Step | Check | Result |
|------|-------|--------|
| 3 | Plugin loads in OpenRGB 1.0 | Pass — "Loaded plugin Plasma Integration", 6–7 devices |
| 4 | Tab, one-time setup through pkexec | Pass — rows ticked, accent off, **Set up** wrote both files and loaded `uleds` |
| 5 | PowerDevil sees the backlight | Pass — `keyboardBrightnessMax` = 100 after the restart button |
| 6 | Last actor wins | Pass — `255,0,0` → `153,0,0` at 60 → `0,255,0` held through 100 |
| 7 | Accent color, Off mode, change signal | Pass — device followed two accent changes, an Off device stayed off, `ConfigChanged` seen 3 times (the file watcher is a fallback only) |
| 8 | Per-device Dim, idle | Pass — Dim-unticked keyboard stayed full at 20 while others dimmed; PowerDevil's dim-to-0 and restore path took the lights dark and back |
| 8b.1 | Rescan | Pass after fixes below — colours, level and an Off mode all survive |
| 8b.2 | USB replug | Pass — replugged dock reappears in the tab and follows the slider |
| 8b.3 | Second OpenRGB window | Pass — logs "another instance owns the backlight, staying passive"; lights unchanged |
| 8b.4 | Fast slider drags | Pass — after 40 rapid level changes and back to 100, all 7 devices match their earlier full colours exactly |

Brightness ranges seen (spec §11): Razer modes 0..255, one Gigabyte mode 0..100.

## Rollout on OpenRGB 1.1 (plan Task 13)

Run 2026-11-11 on the same PC, now with the `openrgb` 1.1 package, Plasma 6.8.0 and Home Assistant 2026.11.0 (openrgb-python 0.3.8). The plugin was built from 07ba692 against the release_1.0 headers; the plugin-facing headers are unchanged in release_1.1. 139/139 tests pass.

| Check | Result |
|-------|--------|
| Installed through the plugins folder, enabled in Settings → Plugins | Pass — "[PlasmaIntegration] loaded, 7 devices" |
| PowerDevil sees the backlight | Pass after the tab's restart button — UPower reports 100 of 100 |
| Plasma slider dims the devices | Pass |
| Accent color | Pass |
| Home Assistant stays connected with the plugin loaded | Pass — entry loaded, no errors, states update |
| Last actor wins | Pass — keyboard set red from HA, slider dimmed it and HA showed the dimmed brightness |

Found during the rollout, outside the plugin: openrgb-python 0.3.8 raises `StopIteration` while connecting when the Effects plugin is loaded but is not plugin 0, because OpenRGB 1.0 and 1.1 send every plugin-specific reply with device id 0. The Effects plugin was removed on this PC.

## Bugs found and fixed during the check

- **Black devices stayed dark.** OpenRGB cannot read most hardware back, so plugged-in and rescanned devices start black. A device black since it appeared now takes the blend of the lit devices' colours (2182140).
- **Lights went dark at every start.** The kernel reports brightness 0 when a uleds LED registers, and UPower's combined keyboard object keeps its own cached value that PowerDevil reads. The plugin discards the registration value and sets the shared level to 100% when it is the only keyboard backlight, or joins the existing level otherwise (4810bd7).
- **Rescans wiped device state.** OpenRGB 1.0 empties its device list before re-adding devices, so the plugin forgot every device (an Off mode came back as the accent). Departed devices are now remembered and restored (3f390ee), including their mode (4f81628).

## Not checked by hand

- Ticking and unticking **Dim** with the mouse (settings were set in the config file; the engine behaviour is unit-tested).
- A real idle timeout (PowerDevil's own brightness call was used to take the same path).

## Unrelated OpenRGB issues seen

Reproduced with the plugin removed. The Razer code involved is unchanged between git2035 and 1.0:

- Naga Epic Chroma (wired, 1532:003e) does not light in any mode.
- BlackWidow Elite ignores OpenRGB's mode brightness setting (colours work, and the plugin's dimming works because it scales colours).
