# pw2mqtt

Envía actualizaciones del estado de entradas/salidas de pipewire por mqtt.

Actualmente sólo se tienen en cuenta los tipos `PipeWire:Interface:Node`.

Mensaje de ejemplo:

Tópico: `stat/desktop/wire`
Mesaje:

```json
{
  "alsa:pcm:2:front:2:playback": {
  "state": "suspended",
  "media.class": "Audio/Sink",
  "name": "alsa_output.pci-0000_31_00.4.analog-stereo"
}
```

## Compilando

```
$ meson build
$ cd build
$ ninja
```

## Configuración

La puede configurar pw2mqtt con variables de entorno:

- `REPORT_NAME` - (requerido) Nombre a incluir en el tópico, ejemplo: "desktop".
- `MQTT_HOST` - Opcional, "localhost" por defecto.
- `MQTT_PORT` - Opcional, "1883" por defecto.
- `MQTT_USERNAME` - Opcional, sin usuario por defecto.
- `MQTT_PASSWORD` - Opcional, sin contraseña por defecto.

## TODO

Esto es un proyecto en progreso, algunas de las cosas pendientes son:

- Manejar otros tipos?
- Pausar conección/reporte en ciertos contextos (por ejemplo según la red).
- Enviar más información?
- Tests?
