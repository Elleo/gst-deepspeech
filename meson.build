project(
  'gst-deepspeech',
  'cpp',
  version: '1.4.0',
  license: 'LGPL-2.0-or-later',
  meson_version: '>= 0.58',
)

# Dependencies

cxx = meson.get_compiler('cpp')

gst_min_version = '1.0.0'
gst_not_found_message = '''
  You need to install or upgrade the GStreamer development
  packages on your system. On debian-based systems these are
  libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
  on RPM-based systems gstreamer1.0-devel, libgstreamer1.0-devel
  or similar. The minimum version required is @0@.
'''.format(gst_min_version)

gstreamer = dependency(
  'gstreamer-1.0',
  version: '>= ' + gst_min_version,
  not_found_message: gst_not_found_message
)
gstreamer_base = dependency(
  'gstreamer-base-1.0',
  version: '>= ' + gst_min_version,
  not_found_message: gst_not_found_message
)
gstreamer_controller = dependency(
  'gstreamer-controller-1.0',
  version: '>= ' + gst_min_version,
  not_found_message: gst_not_found_message
)
gstreamer_audio = dependency(
  'gstreamer-audio-1.0',
  version: '>= ' + gst_min_version,
  not_found_message: gst_not_found_message
)

stt = cxx.find_library('stt')

# Paths

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')

plugins_install_dir = gstreamer.get_variable(
  'pluginsdir',
  pkgconfig_define: ['libdir', libdir],
)

subdir('src')
