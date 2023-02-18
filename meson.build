project('pipe-mon', ['c'])

pipewire_dep = dependency('libpipewire-0.3')
json_c_dep = dependency('json-c')
mqtt_includes = include_directories('mqtt')

executable('pw2mqtt', 'src/main.c',
  'src/node_instance.c', 'src/mqtt.c',
  'mqtt/mqtt.c', 'mqtt/mqtt_pal.c',
  include_directories: mqtt_includes,
  dependencies: [pipewire_dep, json_c_dep])
