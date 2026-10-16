// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
package com.espsmart.voice;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

/** Starts listening after reboot, like Termux:Boot. User must open the app once after install. */
public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context c, Intent i) {
        String a = i == null ? "" : String.valueOf(i.getAction());
        if (android.content.Intent.ACTION_BOOT_COMPLETED.equals(a)
                || "android.intent.action.QUICKBOOT_POWERON".equals(a)) {
            SharedPreferences p = c.getSharedPreferences("espvoice", Context.MODE_PRIVATE);
            if (p.getBoolean("autostart", true) && p.getBoolean("wasRunning", false)) {
                Intent s = new Intent(c, VoiceService.class);
                s.setAction(VoiceService.ACTION_START);
                c.startService(s);
            }
        }
    }
}
