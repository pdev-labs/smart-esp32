// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
#pragma once
#include <Arduino.h>
#include "config.h"

// Synonyms per device: English + Hinglish. Add yours here.
static const char* DEVICE_ALIASES[NUM_DEVICES][6] = {
  {"fan", "pankha", "fan1", "fan 1", NULL, NULL},          // 0 fan
  {"light", "batti", "bulb", "tubelight", "light1", NULL}, // 1 light
  {"tv", "tele", "television", NULL, NULL, NULL},           // 2 tv
  {"pump", "motor", "pani", NULL, NULL, NULL},              // 3 pump
};

struct ParseResult {
  bool ok = false;
  bool all = false;
  int device = -1;     // 0..NUM_DEVICES-1
  bool turnOn = true;
  String heard;
  String reply;
};

static String toLowerStr(String s) {
  s.toLowerCase();
  return s;
}

// tolerant contains: matches alias as whole word-ish substring
static int matchDevice(const String& cmd) {
  for (int d = 0; d < NUM_DEVICES; d++) {
    for (int a = 0; a < 6; a++) {
      const char* alias = DEVICE_ALIASES[d][a];
      if (!alias) break;
      if (cmd.indexOf(alias) >= 0) return d;
    }
    // also match configured name directly
    if (cmd.indexOf(DEVICE_NAMES[d]) >= 0) return d;
  }
  return -1;
}

static bool wantsOn(const String& cmd) {
  // check OFF first (contains "off" also matches... careful: "on" is substring of nothing problematic)
  // Hinglish: chalu/on/khol vs band/off/bund
  if (cmd.indexOf("turn off") >= 0 || cmd.indexOf("switch off") >= 0 ||
      cmd.indexOf(" turn off") >= 0 || cmd.indexOf(" off") >= 0 ||
      cmd.indexOf("band") >= 0 || cmd.indexOf("bund") >= 0 ||
      cmd.indexOf("stop") >= 0 || cmd.indexOf("close") >= 0) {
    // but "turn on" also contains " on" — disambiguate: explicit on-phrases win if both?
    if (cmd.indexOf("turn on") >= 0 || cmd.indexOf("switch on") >= 0 ||
        cmd.indexOf("chalu") >= 0 || cmd.indexOf("start") >= 0) {
      // if both appear, last occurrence decides
      int iOn = cmd.lastIndexOf("on");
      int iOff = cmd.lastIndexOf("off");
      int iBand = cmd.lastIndexOf("band");
      int mOff = iOff > iBand ? iOff : iBand;
      return iOn > mOff;
    }
    return false;
  }
  if (cmd.indexOf("turn on") >= 0 || cmd.indexOf("switch on") >= 0 ||
      cmd.indexOf(" on") >= 0 || cmd.indexOf("chalu") >= 0 ||
      cmd.indexOf("start") >= 0 || cmd.indexOf("open") >= 0) {
    return true;
  }
  return true; // default e.g. "esp fan" -> treat as toggle handled by caller? keep ON
}

// Returns true if command looks like a toggle request without explicit on/off
static bool isToggleOnly(const String& cmd) {
  if (cmd.indexOf("toggle") >= 0 || cmd.indexOf("switch") >= 0) {
    if (cmd.indexOf("on") < 0 && cmd.indexOf("off") < 0 &&
        cmd.indexOf("chalu") < 0 && cmd.indexOf("band") < 0) return true;
  }
  return false;
}

static ParseResult parseVoiceCommand(String raw) {
  ParseResult r;
  r.heard = raw;
  String cmd = toLowerStr(raw);
  cmd.trim();
  // strip common wake words / politeness
  cmd.replace("hey esp", " ");
  cmd.replace("hello esp", " ");
  cmd.replace("hi esp", " ");
  cmd.replace("hey ", " ");
  // keep "esp" removal optional — commands work with or without it
  cmd.replace("esp", " ");
  cmd.replace("please", " ");
  cmd.replace("  ", " ");
  cmd.trim();

  if (cmd.length() == 0) {
    r.reply = "I did not hear anything. Try 'esp turn on fan'.";
    return r;
  }

  bool all = (cmd.indexOf("all") >= 0) || (cmd.indexOf("sab") >= 0) ||
             (cmd.indexOf("sare") >= 0) || (cmd.indexOf("everything") >= 0);
  bool on = wantsOn(cmd);

  if (all) {
    r.ok = true;
    r.all = true;
    r.turnOn = on;
    r.reply = on ? "Turning all devices on." : "Turning all devices off.";
    return r;
  }

  int dev = matchDevice(cmd);
  if (dev < 0) {
    r.reply = String("Sorry, I could not find a device in '") + raw +
              "'. Try fan, light, tv, pump.";
    return r;
  }

  r.ok = true;
  r.device = dev;
  r.turnOn = on;
  if (isToggleOnly(cmd)) {
    r.reply = String("Toggling ") + DEVICE_NAMES[dev] + ".";
    // mark toggle via device=-2 convention? caller checks isToggleOnly separately.
  } else {
    r.reply = String(on ? "Turning on " : "Turning off ") + DEVICE_NAMES[dev] + ".";
  }
  return r;
}
