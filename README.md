# GStreamer DeepSpeech Plugin

[DeepSpeech](https://github.com/mozilla/DeepSpeech) is a speech recognition project created by [Mozilla](https://www.mozilla.org).

This project provides a GStreamer element which can be placed into an audio pipeline, it will then report any recognised speech via bus messages. It automatically segments audio based on configurable silence thresholds making it suitable for continuous dictation.

Here’s a couple of example pipelines using gst-launch.

To perform speech recognition on a file, printing all bus messages to the terminal:

```shell
gst-launch-1.0 -m filesrc location=/path/to/file.ogg ! decodebin ! audioconvert ! audiorate ! audioresample ! deepspeech ! fakesink
```

To perform speech recognition on audio recorded from the default system microphone, with changes to the silence thresholds:

```shell
gst-launch-1.0 -m pulsesrc ! audioconvert ! audiorate ! audioresample ! deepspeech silence-threshold=0.3 silence-length=20 ! fakesink
```

# Installation

If you have downloaded DeepSpeech native client and not installed it in some standard location, you can use the `--with-deepspeech`
`./configure` option to point to where it is located, for example:

```
./configure --prefix=/home/fran/local --with-deepspeech=/home/fran/source/DeepSpeech/native_client
```

If you want to link to your own version of Tensorflow instead of the one bundled with DeepSpeech then you can use the `--enable-tensorflow`
option to the `./configure` script.


