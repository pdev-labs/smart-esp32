# Smart ESP32-S3 Voice Switch (4 devices) — always-listening + phone remote

Two mics, same ESP32:
- **Always-listening offline:** INMP441 mic on ESP32, say `Hi ESP` -> `Fan on` (no phone, no internet, 24/7)
- **Phone remote:** Oppo A37fw opens `http://192.168.4.1/` for buttons / typing / tap-to-speak

```
[INMP441 mic] --> [ESP32-S3] --> 4x Relay --> Fan/Light/TV/Pump
      ^               ^
  "Hi ESP..."    [Oppo phone WiFi remote]
```

## Why phone alone can't "always listen"

Oppo A37fw (Android 5.1) kills background pages, Chrome SpeechRecognition stops after ~60s, needs internet, drains battery. So:
- Phone = good remote + push-to-talk mic (already built).
- True 24/7 wake-word = Rs.150 INMP441 wired to ESP32-S3 with ESP-SR (offline, private). This is now enabled in firmware.

## Files

- `smart-esp32.ino` — WiFi + web + relays + ESP-SR wake-word
- `config.h` — devices, pins, I2S mic pins (BCK 17, WS 18, DIN 16)
- `command_parser.h` — phone/serial text parsing, English + Hinglish
- `web_page.h` — phone UI for old Android

## 1. Wiring guide — DC 5V test first, AC mains later (with electrician)

Parts: ESP32-S3 devkit, 4-channel 5V relay module (Songle SRD-05VDC type),
5V 2A adapter (phone charger works), jumper wires, 4x LED + 220Ω (optional
pre-test), small 5V DC load (USB fan / 5V LED strip), multimeter if you have one.

Pin map (fixed in `config.h:7`). Read the white silkscreen text next to each
pin hole — G4 = GPIO4 = IO4, same pin, different boards print it differently.
Never count "4th pin from the end"; layouts vary. Any GND hole works (all joined).
Do NOT wire anything to GPIO 0/3/45/46 (boot strapping) or 19/20 (USB lines).

| Device | ESP32-S3 label to find | Relay pin to find |
|--------|------------------------|-------------------|
| fan    | G4 (or GPIO4 / IO4)    | IN1               |
| light  | G5                     | IN2               |
| tv     | G6                     | IN3               |
| pump   | G7                     | IN4               |
| ground | GND (any)              | GND               |

**Step 0 — LED pre-test (no relay, 5 minutes):**
LED anode (+) via 220Ω resistor to GPIO 4/5/6/7, cathode (−) to any GND.
Say / type `esp turn on fan` → GPIO4 LED lights. Proves firmware + mapping.

**Step 1 — relay module, no load (listen for clicks):**
Keep the JD-VCC jumper ON (factory position). One common ground everywhere.
```
External 5V adapter (+)  ---> relay VCC pin
External 5V adapter (-)  ---> relay GND pin AND ESP32 GND pin
ESP32 USB cable          ---> PC / charger (powers the ESP32 only)
ESP32 GPIO 4,5,6,7       ---> relay IN1,IN2,IN3,IN4
```
Relays are Active LOW (`RELAY_ACTIVE_LOW true`): GPIO LOW = relay ON (click).
`esp all on` → 4 clicks. `esp all off` → 4 clicks back. Check with multimeter:
COM–NO beeps only when ON; COM–NC beeps only when OFF.

**Step 2 — DC 5V load (same supply is fine for the test):**
```
Supply (+) ---> relay COM
relay NO   ---> load (+) e.g. 5V fan red wire
load (-)   ---> supply (-)
```
Trigger `esp turn on fan` → fan spins. One channel at a time first, then all four.
If a relay misbehaves (stuck ON / chatters): remove the JD-VCC jumper and use
isolated wiring instead — JD-VCC pin to external 5V (+), header VCC pin to
ESP32 3V3, header GND to ESP32 GND, IN1-4 to GPIOs. External supply (-) still
meets ESP32 GND at one point.

Rules: never power relay coils from the ESP32 3V3 pin. All-4-on draws ~300mA,
so use the external adapter, not the ESP32 5V pin. Keep relay terminal screws
tight; loose 5V wires just misbehave, but build tidy habits now.

**Step 3 — AC 230V loads (fan/light, ONLY with electrician):**
Same IN/GND/VCC control side. Mains side per channel:
```
Wall LIVE (via fuse/MCB) ---> relay COM
relay NO                 ---> load (fan/light) live wire
load neutral             ---> wall NEUTRAL. Earth the load body if metal.
```
Always switch LIVE, never neutral alone. Fuse on live. 10A-rated module,
plastic enclosure, no breadboard, no exposed terminals, power off while wiring.
If anything here is unfamiliar, stop and get the electrician — 5V mistakes are
harmless, 230V mistakes are not.

## 2. Flash firmware (always-listening build)

Board: ESP32-S3 16MB flash + 8MB PSRAM. Uses custom `partitions.csv` (dual 4MB APP
slots, model partition at 0x910000) because the stock `esp_sr_16` scheme (3MB APP)
no longer fits: sketch is ~3.17MB.

```bash
cd /home/pdev/smart-esp32
FQBN='esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc'

arduino-cli compile -b "$FQBN" .
arduino-cli upload -b "$FQBN" -p /dev/ttyACM0 .

# flash voice model to custom offset (only needed once / when model changes):
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
  write_flash 0x910000 \
  ~/.arduino15/packages/esp32/tools/esp32s3-libs/3.3.12/esp_sr/srmodels.bin
```

WiFi: STA tries each entry in `WIFI_NETWORKS` (`config.h`) in order, AP `ESP-SMART`
always stays up as fallback. Serial prints the STA IP — enter it in the phone app.

## 2b. INMP441 wiring (for always-listening)

```
INMP441 VDD -> ESP32 3V3 (not 5V)
INMP441 GND -> ESP32 GND
INMP441 SD  -> GPIO 16 (DIN)
INMP441 WS  -> GPIO 18
INMP441 SCK -> GPIO 17 (BCK)
INMP441 L/R -> GND (left channel)
```

Place mic away from fan noise, 1-3m range best. Speak loud/clear first time.

Usage: say `Hi ESP` (wait for serial "WakeWord detected"), then within ~10s say one of:
- `Fan on` / `Fan off`
- `Light on` / `Light off`
- `TV on` / `TV off`
- `Pump on` / `Motor off`
You can chain commands without saying Hi ESP again until timeout.

## 3. Use with Oppo A37fw (browser buttons)

1. On ESP32 serial you will see `AP IP: 192.168.4.1`

You have Termux Legacy — good, use it as 24/7 listener so you don't tap mic each time.
File: `phone/termux_listen.sh` (tested `sh -n` OK).

1. On Oppo install matching legacy pair (Android 5.1 needs old APKs):
   - Termux v0.118 + Termux:API v0.50.x (F-Droid archive / GitHub releases)
   - Open Termux:API once, grant Microphone. Disable battery optimization for Termux + Termux:API.
2. In Termux:
```sh
pkg install curl termux-api
termux-wake-lock
```
3. MUST use home WiFi (Google voice needs internet, ESP AP has none):
   - In `config.h` set `WIFI_STA_SSID`/`WIFI_STA_PASS` to home router, reflash
   - Connect Oppo to same home router
   - Read ESP serial STA IP (e.g. `192.168.1.50`), put it in `phone/termux_listen.sh` as `ESP_IP`
4. Copy script to phone (USB / `python3 -m http.server` on PC, `curl` on phone) then:
```sh
chmod +x termux_listen.sh
./termux_listen.sh
# keep phone plugged in, volume up
```
5. Just speak: `esp turn on fan`. Script ignores sentences without `esp`, sends rest to `GET /api/cmd`, speaks reply via `termux-tts-speak`.

Limits: needs internet, 2-3s delay, Google mistypes in noise. For true offline/no-internet 24/7, use INMP441 above — keep Termux as backup remote.

1. On ESP32 serial you will see `AP IP: 192.168.4.1`
2. On Oppo: WiFi settings → connect to `ESP-SMART` password `12345678`
3. Open Chrome on Oppo → go to `http://192.168.4.1/`
4. Tap mic button, allow microphone, say:
   - `esp turn on fan`
   - `esp turn off fan`
   - `esp light on`
   - `esp all off`
   - Hinglish also works: `esp pankha chalu`, `esp light band`
5. No mic / no internet? Just tap ON/OFF buttons or type in box and press Send.

> Oppo A37fw is Android 5.1 with old Chrome. Voice button needs internet + Google voice services. Buttons + typing work even with no internet (just WiFi to ESP32).

## 3b. Always-listening via Termux (advanced)

File: `phone/termux_listen.sh` (tested `sh -n` OK). Needs Termux:API app +
`pkg install curl termux-api`, and Google voice needs internet so the ESP32 must
be on a router network (STA, see `WIFI_NETWORKS` in `config.h`), not AP-only.
Note: no Termux:API release combines Android 5 support with speech-to-text, so
on very old phones prefer the ESP Voice app below.

## 3c. ESP Voice Android app (recommended phone path)

`phone/app/` is a tiny native app (minSdk 21, no dependencies): continuous
Google speech → custom wake word (default `esp`) → `GET /api/cmd` on the ESP32
→ spoken reply. Runs as a foreground service (survives app close, `START_STICKY`)
and auto-starts after reboot if it was running (open once after install).
Build without Gradle: install JDK 17 + Android SDK (`platforms;android-28`,
`build-tools;28.0.3`), then `phone/app/build.sh` → `ESPVoice-v3.apk`.
Signing key lives in `phone/app/keystore/` (git-ignored, never commit it).

## 4. Voice commands supported

- `esp turn on <fan|light|tv|pump>`
- `esp turn off ...`, `esp <name> on/off`, `esp switch on ...`
- `esp all on`, `esp all off`
- Synonyms in `command_parser.h`: fan/pankha, light/batti/bulb, tv/tele, pump/motor
- You can rename devices in `config.h` without touching logic.

Serial test (no phone needed):
```
> esp turn on fan
> esp all off
```

## 5. Next steps (optional)

- [ ] Change device names in `config.h` to your real loads (also update `sr_commands` in `smart-esp32.ino` — offline phrases are English-only)
- [ ] Set `RELAY_ACTIVE_LOW` to match your relay board (mostly true)
- [ ] Add your networks to `WIFI_NETWORKS` in `config.h` so ESP32 joins home WiFi
- [ ] Offline voice is English-only (`Hi ESP` model). Hinglish `pankha` works only via phone typing/speech.

## License

GNU General Public License v3.0 or later — see [LICENSE](LICENSE).
