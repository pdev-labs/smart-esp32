// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include "config.h"
#include "command_parser.h"
#include "web_page.h"

// Offline always-listening (only on ESP32-S3 with SR model partition)
#if ENABLE_OFFLINE_VOICE
#include "ESP_I2S.h"
#include "ESP_SR.h"
I2SClass voiceI2s;
static bool voiceReady = false;

// Command IDs map 1:1 to devices: id = device*2 + (on?0:1) -> 0..7
enum {
  SR_FAN_ON = 0, SR_FAN_OFF = 1,
  SR_LIGHT_ON = 2, SR_LIGHT_OFF = 3,
  SR_TV_ON = 4, SR_TV_OFF = 5,
  SR_PUMP_ON = 6, SR_PUMP_OFF = 7,
};
static const sr_cmd_t sr_commands[] = {
  {SR_FAN_ON, "Fan on"}, {SR_FAN_ON, "Turn on the fan"}, {SR_FAN_ON, "Switch on the fan"},
  {SR_FAN_OFF, "Fan off"}, {SR_FAN_OFF, "Turn off the fan"}, {SR_FAN_OFF, "Switch off the fan"},
  {SR_LIGHT_ON, "Light on"}, {SR_LIGHT_ON, "Turn on the light"}, {SR_LIGHT_ON, "Switch on the light"},
  {SR_LIGHT_OFF, "Light off"}, {SR_LIGHT_OFF, "Turn off the light"}, {SR_LIGHT_OFF, "Switch off the light"},
  {SR_TV_ON, "TV on"}, {SR_TV_ON, "Turn on the TV"},
  {SR_TV_OFF, "TV off"}, {SR_TV_OFF, "Turn off the TV"},
  {SR_PUMP_ON, "Pump on"}, {SR_PUMP_ON, "Turn on the pump"}, {SR_PUMP_ON, "Motor on"},
  {SR_PUMP_OFF, "Pump off"}, {SR_PUMP_OFF, "Turn off the pump"}, {SR_PUMP_OFF, "Motor off"},
};
#endif

WebServer server(80);
bool devState[NUM_DEVICES] = {false, false, false, false};

static void applyRelay(int idx) {
  int level = devState[idx] ? HIGH : LOW;
  if (RELAY_ACTIVE_LOW && !LED_TEST_MODE) level = devState[idx] ? LOW : HIGH;
  digitalWrite(DEVICE_PINS[idx], level);
}

static void setDevice(int idx, bool on) {
  if (idx < 0 || idx >= NUM_DEVICES) return;
  devState[idx] = on;
  applyRelay(idx);
  Serial.printf("[switch] %s -> %s\n", DEVICE_NAMES[idx], on ? "ON" : "OFF");
}

static void setAll(bool on) {
  for (int i = 0; i < NUM_DEVICES; i++) setDevice(i, on);
}

#if ENABLE_OFFLINE_VOICE
static void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      Serial.println("[voice] WakeWord 'Hi ESP' detected. Say command...");
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      Serial.printf("[voice] WakeWord ch %d. Say command...\n", command_id);
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("[voice] timeout, back to wake-word.");
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    case SR_EVENT_COMMAND:
      Serial.printf("[voice] cmd %d phrase %d\n", command_id, phrase_id);
      switch (command_id) {
        case SR_FAN_ON: setDevice(0, true); break;
        case SR_FAN_OFF: setDevice(0, false); break;
        case SR_LIGHT_ON: setDevice(1, true); break;
        case SR_LIGHT_OFF: setDevice(1, false); break;
        case SR_TV_ON: setDevice(2, true); break;
        case SR_TV_OFF: setDevice(2, false); break;
        case SR_PUMP_ON: setDevice(3, true); break;
        case SR_PUMP_OFF: setDevice(3, false); break;
        default: break;
      }
      Serial.println(String("[voice] state ") + stateJson());
      ESP_SR.setMode(SR_MODE_COMMAND);  // allow chained commands until timeout
      break;
    default: break;
  }
}

static void initOfflineVoice() {
  voiceI2s.setTimeout(1000);
  // INMP441 standard I2S, left channel (L/R pin to GND)
  voiceI2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, -1, I2S_PIN_DIN);
  if (!voiceI2s.begin(I2S_MODE_STD, I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("[voice] I2S begin FAILED. Check mic wiring.");
    return;
  }
  ESP_SR.onEvent(onSrEvent);
  // "M" = mono mic on mono bus. Use "MN"+STEREO only for stereo bus wiring.
  if (ESP_SR.begin(voiceI2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t),
                   SR_CHANNELS_MONO, SR_MODE_WAKEWORD, "M")) {
    voiceReady = true;
    Serial.println("[voice] always-listening ON. Say 'Hi ESP' then 'Fan on'.");
  } else {
    Serial.println("[voice] ESP_SR begin FAILED. Did you flash with Partition 'ESP SR 16M'?");
  }
}
#endif

static String stateJson() {
  String j = "{";
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (i) j += ",";
    j += "\"" + String(DEVICE_NAMES[i]) + "\":" + (devState[i] ? "1" : "0");
  }
  j += "}";
  return j;
}

// Execute parsed command, returns reply string
static String executeCommand(const String& rawText, bool& toggled) {
  toggled = false;
  ParseResult r = parseVoiceCommand(rawText);
  Serial.printf("[heard] '%s' -> '%s'\n", rawText.c_str(), r.reply.c_str());
  if (!r.ok) return r.reply;

  String low = rawText; low.toLowerCase();
  if (r.all) {
    setAll(r.turnOn);
    return r.reply;
  }
  if (isToggleOnly(low) && r.device >= 0) {
    setDevice(r.device, !devState[r.device]);
    toggled = true;
    return String(devState[r.device] ? "Turning on " : "Turning off ") +
           DEVICE_NAMES[r.device] + ".";
  }
  setDevice(r.device, r.turnOn);
  return r.reply;
}

static String urlDecode(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') out += ' ';
    else if (c == '%' && i + 2 < s.length()) {
      char h1 = s[i + 1], h2 = s[i + 2];
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return 0;
      };
      out += (char)(hex(h1) * 16 + hex(h2));
      i += 2;
    } else out += c;
  }
  return out;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println("\n=== ESP Smart Switch ===");

  for (int i = 0; i < NUM_DEVICES; i++) {
    pinMode(DEVICE_PINS[i], OUTPUT);
    devState[i] = false;
    applyRelay(i);
  }

  WiFi.mode(WIFI_AP_STA);

  // Try each known STA network in order (open or secured, same name allowed)
  WiFiMulti multi;
  for (size_t n = 0; n < WIFI_NET_COUNT; n++) {
    if (strlen(WIFI_NETWORKS[n].ssid) == 0) continue;
    if (strlen(WIFI_NETWORKS[n].pass) == 0) {
      Serial.printf("STA try '%s' (open)...\n", WIFI_NETWORKS[n].ssid);
      multi.addAP(WIFI_NETWORKS[n].ssid);
    } else {
      Serial.printf("STA try '%s' (secured)...\n", WIFI_NETWORKS[n].ssid);
      multi.addAP(WIFI_NETWORKS[n].ssid, WIFI_NETWORKS[n].pass);
    }
  }
  Serial.println("Connecting STA (multi)...");
  unsigned long t0 = millis();
  while (multi.run() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) delay(400);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("STA OK: '%s' IP=%s RSSI=%d\n",
      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.println(">>> Enter this IP in the ESP Voice app <<<");
  } else {
    Serial.println("STA failed, continuing AP-only.");
  }

  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.printf("AP '%s' IP: %s\n", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
  Serial.println("Open http://192.168.4.1/ on phone (connect WiFi to ESP-SMART first)");
  Serial.println("Try serial: esp turn on fan");

  server.on("/", []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", []() {
    server.send(200, "application/json", stateJson());
  });

  server.on("/api/set", []() {
    String dev = server.arg("dev"); dev.toLowerCase();
    String onS = server.arg("on");
    bool on = (onS == "1" || onS == "on" || onS == "true");
    for (int i = 0; i < NUM_DEVICES; i++) {
      if (dev == DEVICE_NAMES[i]) {
        setDevice(i, on);
        server.send(200, "application/json",
          "{\"ok\":1,\"dev\":\"" + String(DEVICE_NAMES[i]) + "\",\"on\":" + (on ? "1" : "0") + "}");
        return;
      }
    }
    server.send(400, "application/json", "{\"ok\":0,\"error\":\"unknown device\"}");
  });

  server.on("/api/cmd", []() {
    String text = urlDecode(server.arg("text"));
    if (text.length() == 0) {
      server.send(400, "application/json", "{\"ok\":0,\"error\":\"empty text\"}");
      return;
    }
    bool toggled = false;
    String reply = executeCommand(text, toggled);
    String resp = "{\"ok\":1,\"heard\":\"" + text + "\",\"reply\":\"" + reply +
                  "\",\"state\":" + stateJson() + "}";
    // NOTE: quotes in text could break JSON; acceptable for simple commands.
    server.send(200, "application/json", resp);
  });

  server.begin();
  Serial.println("HTTP ready.");

#if ENABLE_OFFLINE_VOICE
  initOfflineVoice();
  Serial.println("Voice: say 'Hi ESP' -> 'Fan on' / 'Light off' (no phone/internet needed).");
  Serial.println("Phone page still works as remote + manual buttons.");
#endif
}

void loop() {
  server.handleClient();

  // Serial control: type "esp turn on fan" + Enter
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) {
        bool t = false;
        String reply = executeCommand(line, t);
        Serial.println(reply + " " + stateJson());
        line = "";
      }
    } else {
      line += c;
      if (line.length() > 200) line = "";
    }
  }

  static unsigned long last = 0;
  if (millis() - last > STATUS_PRINT_EVERY_MS) {
    last = millis();
    // heartbeat (comment out if noisy)
    // Serial.println(String("[state] ") + stateJson());
  }
}
