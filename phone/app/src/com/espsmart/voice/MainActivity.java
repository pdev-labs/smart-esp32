// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
package com.espsmart.voice;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/** Settings + log screen. Listening runs in VoiceService (background + boot). */
public class MainActivity extends Activity {
    private EditText ipView;
    private EditText wakeView;
    private CheckBox bootBox;
    private Button toggleBtn;
    private Button testBtn;
    private TextView statusView;
    private TextView logView;
    private ScrollView scroll;

    private final BroadcastReceiver uiRx = new BroadcastReceiver() {
        public void onReceive(Context c, Intent i) {
            if (VoiceService.BC_LOG.equals(i.getAction())) {
                appendLog(i.getStringExtra("line"));
            } else if (VoiceService.BC_STATUS.equals(i.getAction())) {
                statusView.setText(String.valueOf(i.getStringExtra("text")));
            }
        }
    };

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        SharedPreferences p = getSharedPreferences("espvoice", MODE_PRIVATE);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 24, 24, 24);

        TextView title = new TextView(this);
        title.setText("ESP Voice — always listening");
        title.setTextSize(20);
        root.addView(title);

        TextView hint = new TextView(this);
        hint.setText("Phone + ESP32 must be on the SAME WiFi (home router).\nRuns in background + restarts after reboot.\nOn Oppo: lock this app in recent tasks + allow auto-start in Security Center.");
        root.addView(hint);

        ipView = new EditText(this);
        ipView.setText(p.getString("ip", "192.168.1.50"));
        ipView.setHint("ESP32 IP, e.g. 192.168.1.50");
        root.addView(ipView);

        wakeView = new EditText(this);
        wakeView.setText(p.getString("wake", "esp"));
        wakeView.setHint("your wake word, e.g. jarvis");
        root.addView(wakeView);

        bootBox = new CheckBox(this);
        bootBox.setText("Start listening after phone reboot");
        bootBox.setChecked(p.getBoolean("autostart", true));
        root.addView(bootBox);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);

        toggleBtn = new Button(this);
        toggleBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { onToggle(); }
        });
        row.addView(toggleBtn);

        testBtn = new Button(this);
        testBtn.setText("TEST ESP");
        testBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                String ip = ipView.getText().toString().trim();
                save();
                Intent s = new Intent(MainActivity.this, VoiceService.class);
                s.setAction(VoiceService.ACTION_TEST);
                s.putExtra("ip", ip);
                startService(s);
            }
        });
        row.addView(testBtn);
        root.addView(row);

        statusView = new TextView(this);
        statusView.setGravity(Gravity.CENTER);
        root.addView(statusView);

        scroll = new ScrollView(this);
        logView = new TextView(this);
        logView.setTextSize(13);
        scroll.addView(logView);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);
        refreshToggle();
    }

    private void save() {
        String wake = wakeView.getText().toString().trim().toLowerCase();
        if (wake.length() == 0) wake = "esp";
        getSharedPreferences("espvoice", MODE_PRIVATE).edit()
                .putString("ip", ipView.getText().toString().trim())
                .putString("wake", wake)
                .putBoolean("autostart", bootBox.isChecked())
                .apply();
    }

    private void onToggle() {
        if (isServiceRunning()) {
            Intent s = new Intent(this, VoiceService.class);
            s.setAction(VoiceService.ACTION_STOP);
            startService(s);
        } else {
            save();
            Intent s = new Intent(this, VoiceService.class);
            s.setAction(VoiceService.ACTION_START);
            s.putExtra("ip", ipView.getText().toString().trim());
            s.putExtra("wake", wakeView.getText().toString().trim());
            startService(s);
        }
        statusView.postDelayed(new Runnable() {
            public void run() { refreshToggle(); }
        }, 600);
    }

    private boolean isServiceRunning() {
        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) return false;
        for (ActivityManager.RunningServiceInfo s : am.getRunningServices(50)) {
            if (VoiceService.class.getName().equals(s.service.getClassName())) return true;
        }
        return false;
    }

    private void refreshToggle() {
        boolean run = isServiceRunning();
        toggleBtn.setText(run ? "STOP" : "START LISTENING");
        statusView.setText(run ? "running in background" : "idle");
    }

    private void appendLog(String s) {
        if (s == null) return;
        logView.append(s + "\n");
        CharSequence t = logView.getText();
        if (t.length() > 6000) logView.setText(t.subSequence(t.length() - 6000, t.length()));
        scroll.post(new Runnable() {
            public void run() { scroll.fullScroll(View.FOCUS_DOWN); }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        IntentFilter f = new IntentFilter();
        f.addAction(VoiceService.BC_LOG);
        f.addAction(VoiceService.BC_STATUS);
        registerReceiver(uiRx, f);
        refreshToggle();
    }

    @Override
    protected void onPause() {
        try { unregisterReceiver(uiRx); } catch (Exception ignored) {}
        super.onPause();
    }
}
