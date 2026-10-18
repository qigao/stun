#!/usr/bin/env bash
set -euo pipefail

package=com.qigao.gcanvas.lifecycle
apk=vendor/gCanvas/tests/android/app/build/outputs/apk/debug/app-debug.apk

test -f "$apk"
adb install -r "$apk"
adb logcat -c
adb shell am start -W -n "$package/.MainActivity"

result=""
for attempt in $(seq 1 90); do
  result="$(adb shell "run-as $package cat files/result.txt 2>/dev/null" | tr -d '\r' || true)"
  if [[ "$result" == "PASS" ]]; then
    break
  fi
  if [[ "$result" == FAIL:* ]]; then
    echo "$result"
    adb logcat -d -s GCANVAS_ANDROID_TEST:V '*:S'
    exit 1
  fi
  sleep 1
done

if [[ "$result" != "PASS" ]]; then
  echo "Timed out waiting for Android gCanvas lifecycle qualification"
  adb logcat -d -s GCANVAS_ANDROID_TEST:V '*:S'
  exit 1
fi

adb logcat -d -s GCANVAS_ANDROID_TEST:V '*:S'
adb shell am force-stop "$package"
