#!/bin/sh
# Génère la configuration Mosquitto à partir des variables d'environnement.
set -eu

CONF=/mosquitto/config/mosquitto.conf
mkdir -p /mosquitto/config /mosquitto/data /mosquitto/log

{
  printf 'listener %s\n' "${MQTT_PORT:-1883}"
  printf 'persistence true\n'
  printf 'persistence_location /mosquitto/data/\n'
  printf 'log_dest stdout\n'
  printf 'log_type all\n'
  printf 'connection_messages true\n'
  if [ -n "${MQTT_USER:-}" ] && [ -n "${MQTT_PASSWORD:-}" ]; then
    printf 'allow_anonymous false\n'
    printf 'password_file /mosquitto/config/passwd\n'
    mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASSWORD"
  else
    printf 'allow_anonymous true\n'
  fi
} > "$CONF"

echo "[aquafeeder] Mosquitto configuré sur le port ${MQTT_PORT:-1883}"
exec /usr/sbin/mosquitto -c "$CONF"
