#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
# Manual build of ESP Voice APK (no Gradle). Needs JDK + Android SDK (see README).
set -e
SDK=/home/pdev/.local/android/sdk
BT=$SDK/build-tools/28.0.3
AJAR=$SDK/platforms/android-28/android.jar
export JAVA_HOME=/home/pdev/.local/android/jdk-17
export PATH=$JAVA_HOME/bin:$PATH
HERE=$(dirname "$0")
cd "$HERE"
rm -rf out && mkdir -p out/classes out/dex

echo "[1/6] aapt2 link (manifest)"
$BT/aapt2 link -o out/app-base.apk \
  --manifest AndroidManifest.xml \
  -I "$AJAR" \
  --min-sdk-version 21 --target-sdk-version 22 \
  --version-code 3 --version-name 1.2

echo "[2/6] javac"
find src -name '*.java' > out/srcs.txt
javac -source 8 -target 8 -bootclasspath "$AJAR" -cp "$AJAR" -d out/classes @out/srcs.txt 2>&1 | grep -v "bootstrap\|deprecat" || true

echo "[3/6] d8 (dex, min-api 21)"
$BT/d8 --min-api 21 --lib "$AJAR" --output out/dex $(find out/classes -name '*.class')

echo "[4/6] add classes.dex"
cd out/dex && zip -q -uj ../app-base.apk classes*.dex && cd ../..

echo "[5/6] zipalign"
$BT/zipalign -f 4 out/app-base.apk out/app-aligned.apk

echo "[6/6] sign (debug key)"
if [ ! -f keystore/debug.keystore ]; then
  keytool -genkeypair -keystore keystore/debug.keystore -storepass android -alias androiddebugkey \
    -keypass android -keyalg RSA -keysize 2048 -validity 10950 \
    -dname "CN=ESP Voice Debug, OU=dev, O=espsmart, L=home, ST=home, C=IN"
fi
$BT/apksigner sign --ks keystore/debug.keystore --ks-pass pass:android --key-pass pass:android --out ESPVoice-v3.apk out/app-aligned.apk
$BT/apksigner verify --print-certs ESPVoice-v3.apk | head -n 6
ls -l ESPVoice-v3.apk
unzip -p ESPVoice-v3.apk classes.dex | head -c 8 | od -c | head -n 1
echo "BUILD OK: $HERE/ESPVoice-v3.apk"
