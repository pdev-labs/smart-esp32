// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
package com.espsmart.voice;

import android.app.Notification;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;

/** Foreground service: always-listening loop that survives app close + reboot. */
public class VoiceService extends Service {
    public static final String ACTION_START = "com.espsmart.voice.START";
    public static final String ACTION_STOP = "com.espsmart.voice.STOP";
    public static final String ACTION_TEST = "com.espsmart.voice.TEST";
    public static final String BC_LOG = "com.espsmart.voice.LOG";
    public static final String BC_STATUS = "com.espsmart.voice.STATUS";

    private static final String TAG = "ESPVoice";
    private static final int NOTIF = 11;

    private SpeechRecognizer sr;
    private TextToSpeech tts;
    private boolean running = false;
    private boolean ttsReady = false;
    private PowerManager.WakeLock wakeLock;
    private WifiManager.WifiLock wifiLock;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private String espIp = "192.168.1.50";
    private String wake = "esp";

    @Override
    public IBinder onBind(Intent i) { return null; }

    @Override
    public void onCreate() {
        super.onCreate();
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "ESPVoice:mic");
        WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "ESPVoice:wifi");
        tts = new TextToSpeech(this, new TextToSpeech.OnInitListener() {
            public void onInit(int st) {
                if (st == TextToSpeech.SUCCESS) {
                    int r = tts.setLanguage(new Locale("en", "IN"));
                    if (r == TextToSpeech.LANG_MISSING_DATA || r == TextToSpeech.LANG_NOT_SUPPORTED) {
                        tts.setLanguage(Locale.ENGLISH);
                    }
                    tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                        public void onStart(String id) {}
                        public void onError(String id) { onSpeakDone(); }
                        public void onDone(String id) { onSpeakDone(); }
                    });
                    ttsReady = true;
                    blog("TTS ready");
                } else {
                    blog("TTS init failed");
                }
            }
        });
    }

    private SharedPreferences prefs() {
        return getSharedPreferences("espvoice", MODE_PRIVATE);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || intent.getAction() == null) {
            // restarted by system after kill: resume only if user left it running
            if (prefs().getBoolean("wasRunning", false)) {
                espIp = prefs().getString("ip", espIp);
                wake = prefs().getString("wake", wake);
                startLoop();
            }
            return START_STICKY;
        }
        String a = intent.getAction();
        if (ACTION_STOP.equals(a)) {
            prefs().edit().putBoolean("wasRunning", false).apply();
            stopLoop();
            stopSelf();
        } else if (ACTION_TEST.equals(a)) {
            String ip = intent.getStringExtra("ip");
            if (ip == null || ip.length() == 0) ip = prefs().getString("ip", espIp);
            final String fip = ip;
            blog("GET http://" + fip + "/api/state ...");
            get("http://" + fip + "/api/state", new Cb() {
                public void done(String body, String err) {
                    if (err != null) blog("ESP error: " + err + " (online? same WiFi?)");
                    else blog("ESP state: " + body);
                }
            });
        } else if (ACTION_START.equals(a)) {
            String ip = intent.getStringExtra("ip");
            String w = intent.getStringExtra("wake");
            if (ip != null && ip.length() > 0) espIp = ip.trim();
            if (w != null && w.length() > 0) wake = w.trim().toLowerCase(Locale.US);
            prefs().edit().putString("ip", espIp).putString("wake", wake)
                    .putBoolean("wasRunning", true).apply();
            startLoop();
        }
        return START_STICKY;
    }

    private void startLoop() {
        startForeground(NOTIF, notif("starting..."));
        try {
            if (!wakeLock.isHeld()) wakeLock.acquire();
            if (!wifiLock.isHeld()) wifiLock.acquire();
        } catch (Exception e) {
            blog("lock warn: " + e);
        }
        running = true;
        blog("Listening. Wake word: '" + wake + "'. Say '" + wake + " turn on fan'");
        bstatus("listening...");
        beginOneShot();
    }

    private void stopLoop() {
        running = false;
        try {
            if (sr != null) { sr.cancel(); sr.destroy(); sr = null; }
        } catch (Exception ignored) {}
        try {
            if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
            if (wifiLock != null && wifiLock.isHeld()) wifiLock.release();
        } catch (Exception ignored) {}
        try { stopForeground(true); } catch (Exception ignored) {}
        bstatus("idle");
        blog("Stopped.");
    }

    private Notification notif(String text) {
        Notification.Builder nb = new Notification.Builder(this)
                .setContentTitle("ESP Voice listening")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_btn_speak_now)
                .setOngoing(true);
        return nb.build();
    }

    private void renotif(String text) {
        try {
            android.app.NotificationManager nm =
                    (android.app.NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            nm.notify(NOTIF, notif(text));
        } catch (Exception ignored) {}
    }

    private void beginOneShot() {
        if (!running) return;
        try {
            if (sr == null) {
                if (!SpeechRecognizer.isRecognitionAvailable(this)) {
                    blog("ERROR: Google voice recognition unavailable. Update Google app.");
                    stopLoop();
                    stopSelf();
                    return;
                }
                sr = SpeechRecognizer.createSpeechRecognizer(this);
                sr.setRecognitionListener(listener);
            }
            Intent i = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            i.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            i.putExtra(RecognizerIntent.EXTRA_LANGUAGE, "en-IN");
            i.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1);
            i.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, false);
            sr.startListening(i);
            bstatus("listening...");
        } catch (Exception e) {
            blog("start error: " + e + " — retrying");
            scheduleRestart(1500);
        }
    }

    private void scheduleRestart(int ms) {
        if (!running) return;
        handler.postDelayed(new Runnable() {
            public void run() { beginOneShot(); }
        }, ms);
    }

    private void recreateSr() {
        try {
            if (sr != null) { sr.cancel(); sr.destroy(); }
        } catch (Exception ignored) {}
        sr = null;
    }

    private final RecognitionListener listener = new RecognitionListener() {
        public void onReadyForSpeech(Bundle p) { bstatus("listening..."); }
        public void onBeginningOfSpeech() { bstatus("hearing you..."); }
        public void onRmsChanged(float v) {}
        public void onBufferReceived(byte[] b) {}
        public void onEndOfSpeech() { bstatus("thinking..."); }
        public void onPartialResults(Bundle p) {}
        public void onEvent(int t, Bundle p) {}

        public void onResults(Bundle r) {
            ArrayList<String> list = r.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            String text = (list != null && !list.isEmpty()) ? list.get(0) : "";
            text = text == null ? "" : text.trim();
            if (text.length() == 0) {
                scheduleRestart(300);
                return;
            }
            blog("heard: " + text);
            renotif("heard: " + text);
            if (text.toLowerCase(Locale.US).contains(wake)) {
                sendCommand(text);
            } else {
                scheduleRestart(300);
            }
        }

        public void onError(int code) {
            Log.d(TAG, "sr error " + code);
            if (!running) return;
            if (code == SpeechRecognizer.ERROR_NO_MATCH || code == SpeechRecognizer.ERROR_SPEECH_TIMEOUT) {
                scheduleRestart(300);
            } else if (code == SpeechRecognizer.ERROR_CLIENT || code == SpeechRecognizer.ERROR_RECOGNIZER_BUSY) {
                recreateSr();
                scheduleRestart(1200);
            } else if (code == SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS) {
                blog("ERROR: microphone permission missing. Reinstall app.");
                stopLoop();
                stopSelf();
            } else if (code == SpeechRecognizer.ERROR_NETWORK || code == SpeechRecognizer.ERROR_NETWORK_TIMEOUT
                    || code == SpeechRecognizer.ERROR_SERVER) {
                blog("network error (" + code + ") — need internet. retrying...");
                renotif("waiting for internet...");
                scheduleRestart(2500);
            } else {
                scheduleRestart(1000);
            }
        }
    };

    private interface Cb { void done(String body, String err); }

    private void get(final String url, final Cb cb) {
        new Thread(new Runnable() {
            public void run() {
                HttpURLConnection c = null;
                try {
                    c = (HttpURLConnection) new URL(url).openConnection();
                    c.setConnectTimeout(6000);
                    c.setReadTimeout(6000);
                    c.setRequestMethod("GET");
                    final int code = c.getResponseCode();
                    InputStream in = (code >= 200 && code < 300) ? c.getInputStream() : c.getErrorStream();
                    ByteArrayOutputStream bos = new ByteArrayOutputStream();
                    byte[] buf = new byte[1024];
                    int n;
                    while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
                    final String body = new String(bos.toByteArray(), "UTF-8");
                    final String err = (code >= 200 && code < 300) ? null : ("HTTP " + code);
                    handler.post(new Runnable() {
                        public void run() { cb.done(body, err); }
                    });
                } catch (final Exception e) {
                    handler.post(new Runnable() {
                        public void run() { cb.done(null, e.toString()); }
                    });
                } finally {
                    if (c != null) c.disconnect();
                }
            }
        }).start();
    }

    private void sendCommand(String text) {
        bstatus("sending to ESP...");
        String url;
        try {
            url = "http://" + espIp + "/api/cmd?text=" + URLEncoder.encode(text, "UTF-8");
        } catch (Exception e) {
            blog("encode error: " + e);
            scheduleRestart(500);
            return;
        }
        get(url, new Cb() {
            public void done(String body, String err) {
                if (!running) return;
                if (err != null) {
                    blog("ESP error: " + err + " (online? same WiFi?)");
                    speak("ESP not reachable");
                    return;
                }
                String reply = extractReply(body);
                blog("ESP: " + reply);
                renotif(reply);
                speak(reply);
            }
        });
    }

    private String extractReply(String json) {
        if (json == null) return "done";
        String key = "\"reply\":\"";
        int i = json.indexOf(key);
        if (i < 0) return json.length() > 120 ? json.substring(0, 120) : json;
        i += key.length();
        int j = json.indexOf('"', i);
        if (j < 0) j = Math.min(i + 120, json.length());
        return json.substring(i, j);
    }

    private void speak(String msg) {
        if (!ttsReady || msg == null || msg.length() == 0) {
            scheduleRestart(400);
            return;
        }
        try {
            HashMap<String, String> p = new HashMap<String, String>();
            p.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, "esp1");
            tts.speak(msg, TextToSpeech.QUEUE_FLUSH, p);
        } catch (Exception e) {
            blog("tts error: " + e);
            scheduleRestart(400);
        }
    }

    private void onSpeakDone() {
        handler.post(new Runnable() {
            public void run() { scheduleRestart(300); }
        });
    }

    private void blog(String s) {
        Log.d(TAG, s);
        sendBroadcast(new Intent(BC_LOG).putExtra("line", s));
    }

    private void bstatus(String s) {
        sendBroadcast(new Intent(BC_STATUS).putExtra("text", s));
    }

    @Override
    public void onDestroy() {
        try { stopLoop(); } catch (Exception ignored) {}
        try {
            if (tts != null) { tts.stop(); tts.shutdown(); }
        } catch (Exception ignored) {}
        super.onDestroy();
    }
}
