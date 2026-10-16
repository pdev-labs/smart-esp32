#!/data/data/com.termux/files/usr/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
# Always-listening bridge for Oppo A37fw (Android 5.1, Termux Legacy + Termux:API)
# Listens via Android speech (needs internet), sends text to ESP32-S3.
#
# Install (on phone, in Termux):
#   pkg install curl termux-api
#   # + install Termux:API legacy APK v0.47 (already downloaded to Downloads/TermuxAPI_v0.47.apk) and grant Microphone permission
#
# Run:
#   termux-wake-lock
#   chmod +x termux_listen.sh
#   ./termux_listen.sh
# Stop: Ctrl+C, then: termux-wake-unlock
#
# IMPORTANT WiFi note:
#   Google speech-to-text needs INTERNET. ESP AP "ESP-SMART" has no internet.
#   So join everything to your HOME ROUTER:
#     1) in ESP config.h set WIFI_STA_SSID / WIFI_STA_PASS to home router, reflash
#     2) connect Oppo to same home router (internet works)
#     3) set ESP_IP below to the STA IP shown in ESP serial (e.g. 192.168.1.50)
#   If you stay on ESP AP (192.168.4.1) with no internet, speech will fail —
#   use buttons/typing on the web page instead, or install offline Vosk (heavy).

ESP_IP="192.168.1.50"
# Fallback: if home router not set, use AP IP:
# ESP_IP="192.168.4.1"

ESP="http://$ESP_IP"
SLEEP_OK=1
SLEEP_EMPTY=2

echo "=== ESP always-listening (Termux) ==="
echo "ESP: $ESP"
echo "Keep phone plugged in. Say: esp turn on fan"
echo ""

urlenc() {
  # minimal encoder: spaces + a few chars. Commands are a-z only so this is enough.
  echo "$1" | sed 's/ /%20/g; s/#/%23/g; s/&/%26/g; s/?/%3F/g'
}

i=0
while true; do
  # termux-speech-to-text prints recognized text, one line per result.
  # It uses Android Google voice typing (online). Empty output = nothing heard / no net.
  TEXT=$(termux-speech-to-text 2>/dev/null | head -n 1)
  # trim
  TEXT=$(echo "$TEXT" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

  if [ -z "$TEXT" ]; then
    i=$((i+1))
    if [ $i -eq 3 ]; then
      echo "[...] waiting... (no speech / no internet? check WiFi + Google voice typing)"
      i=0
    fi
    sleep $SLEEP_EMPTY
    continue
  fi

  echo "[heard] $TEXT"

  # only act when wake word "esp" (or hey esp) present — saves battery + false triggers
  echo "$TEXT" | grep -i -q "esp"
  if [ $? -ne 0 ]; then
    echo "  (ignored, no 'esp' wake word)"
    sleep $SLEEP_OK
    continue
  fi

  ENC=$(urlenc "$TEXT")
  RESP=$(curl -s --max-time 6 "$ESP/api/cmd?text=$ENC")
  echo "  [esp] $RESP"

  # speak confirmation back on phone speaker (needs Google TTS, offline voice ok)
  REPLY=$(echo "$RESP" | sed 's/.*"reply":"\([^"]*\)".*/\1/')
  if [ -n "$REPLY" ] && [ "$REPLY" != "$RESP" ]; then
    termux-tts-speak "$REPLY" 2>/dev/null
  fi

  sleep $SLEEP_OK
done
