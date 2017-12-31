#!/usr/bin/env python

from __future__ import print_function

import gi
gi.require_version('Gst', '1.0')
from gi.repository import GObject, Gst


def bus_message(bus, message):
    structure = message.get_structure()
    if structure and structure.get_name() == "deepspeech":
        text = structure.get_value("text")
        print(text)
    return True


if __name__ == "__main__":
    GObject.threads_init()
    Gst.init(None)
    loop = GObject.MainLoop()

    pipeline = Gst.parse_launch("pulsesrc ! audioconvert ! audiorate ! audioresample ! deepspeech silence-length=20 ! fakesink")
    
    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect ("message", bus_message)
    
    pipeline.set_state(Gst.State.PLAYING)
    
    loop.run()

