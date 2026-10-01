"""Read or change one OpenRGB device over the SDK: sdk_check.py NAME [--color RRGGBB] [--mode MODE]."""

import argparse

import openrgb.orgb as orgb
from openrgb import OpenRGBClient, plugins
from openrgb.utils import RGBColor


class UnknownPlugin:
    def __init__(self, plugin, comms):
        self.name = plugin.name

    def update(self):
        pass


# openrgb-python 0.3.7 raises KeyError for any plugin but Effects (jath03/openrgb-python#95).
create_plugin = orgb.create_plugin
orgb.create_plugin = lambda plugin, comms: (create_plugin if plugin.name in plugins.PLUGIN_NAMES else UnknownPlugin)(plugin, comms)

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
