cdata = configuration_data()
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE', meson.project_name())

config_h = configure_file(
  output: 'config.h',
  configuration: cdata,
)

libgstdeepspeech = shared_library(
  'gstdeepspeech',
  sources: [
    'gstdeepspeech.cc',
    config_h,
  ],
  cpp_args: [
    '-DHAVE_CONFIG_H',
  ],
  dependencies: [
    gstreamer,
    gstreamer_base,
    gstreamer_controller,
    gstreamer_audio,
    stt,
  ],
  install: true,
  install_dir: plugins_install_dir,
)
