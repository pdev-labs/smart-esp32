// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
#pragma once

// ---------- Devices ----------
// Rename to match your loads. Keep lowercase, no spaces.
#define NUM_DEVICES 4
// GPIOs chosen to avoid ESP32-S3 strapping pins (0,3,45,46) and USB (19,20)
static const int DEVICE_PINS[NUM_DEVICES] = {4, 5, 6, 7};
static const char* DEVICE_NAMES[NUM_DEVICES] = {"fan", "light", "tv", "pump"};

// Relay boards are usually Active LOW (LOW = ON). Set false if yours is Active HIGH.
#define RELAY_ACTIVE_LOW true

// Set true to invert logic in software for testing with LEDs (LED on = relay ON).
#define LED_TEST_MODE false

// ---------- WiFi ----------
// ESP32 creates this hotspot. Phone connects to it.
#define WIFI_AP_SSID "ESP-SMART"
#define WIFI_AP_PASS "12345678"   // min 8 chars

// STA networks to join (tried in order, first available wins).
// Phone + ESP32 must end up on the SAME network for the app to reach it.
struct WifiNet { const char* ssid; const char* pass; };
static const WifiNet WIFI_NETWORKS[] = {
  {"vivo T4x 5G", ""},                 // open hotspot (no password)
  {"WIFI_NAME", "WIFI_PASSWORD"},      // same-name secured variant (fill in, keep private)
};
#define WIFI_NET_COUNT (sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]))
#define WIFI_CONNECT_TIMEOUT_MS 12000

// ---------- Always-listening offline voice (ESP-SR + I2S mic) ----------
// True always-listening needs a mic wired to ESP32-S3. Phone alone can't do it reliably.
// Buy: INMP441 I2S mic (~Rs.150). Wire: VDD->3V3, GND->GND, SD->DIN, WS->WS, SCK->BCK, L/R->GND
#define ENABLE_OFFLINE_VOICE true
#define I2S_PIN_BCK 17
#define I2S_PIN_WS  18
#define I2S_PIN_DIN 16
#define I2S_SAMPLE_RATE 16000

// ---------- Behaviour ----------
#define SERIAL_BAUD 115200
// How long (ms) to keep "last voice command" shown on serial
#define STATUS_PRINT_EVERY_MS 5000
