# SHAP — Smart Home Automation Platform

An offline-first ESP32-S3 smart-home controller and locally hosted web dashboard. **Current status: hardware-unvalidated firmware candidate, not a certified or production-ready mains controller.** The frontend has a clearly labelled interactive simulation; firmware implements a smaller, real GPIO-backed subset. Do not connect mains loads without professionally designed isolation and independent protection.

## راهنمای فارسیِ تصویری

برای شروع **از رایانه‌ای که هیچ ابزار توسعه‌ای ندارد**، [راهنمای کامل نصب از صفر (فارسی)](docs/install-fa.md) را دنبال کنید؛ تهیهٔ پیش‌نیازهای Arduino IDE برای رایانهٔ آفلاین، بازکردن Sketch، انتخاب برد/پورت و تنها یک Upload را توضیح می‌دهد. راهنمای راه‌اندازی همراه با **فونت وزیرمتنِ آفلاین** و تصویرهای رابط در [`data/guide-fa.html`](data/guide-fa.html) است. پس از اجرای دمو یا آپلود Sketch، آن را از مسیر **`/guide-fa.html`** باز کنید یا از لینک «راهنمای فارسی» در نوار کناری استفاده کنید. تصویرها از اجرای مرورگری برنامه (حالت شبیه‌سازی) گرفته شده‌اند؛ عکس از سخت‌افزار واقعی نیستند.

## Try the interactive demo

```sh
python -m http.server 4173 --bind 0.0.0.0 --directory data
# visit http://localhost:4173
```

When `/api/bootstrap` is absent the UI enters **SIMULATED DEMO** mode. It uses localStorage and does not talk to hardware. Explore all ten navigation views, toggle simulated relays, adjust simulated sensor readings, create/edit/delete devices, rules, scenes and schedules, drag/resize dashboard widgets, undo/redo, publish layouts that actually render on Overview, download/restore a demo backup, and preview the five-step setup wizard. No CDN or external runtime scripts/fonts are loaded; the Persian guide bundles Vazirmatn locally. Use the theme button for dark mode. Demo state can be reset by clearing this site's localStorage (`shap_demo_v2`).

## Arduino IDE: one Upload, offline web assets in firmware

- **Board:** ESP32-S3-DevKitC-1 N16R8 with 16 MB QIO flash and 8 MB OPI PSRAM. Confirm the actual board before flashing.
- Open **[`arduino/SHAP/SHAP.ino`](arduino/SHAP/SHAP.ino)** in Arduino IDE 2. Install esp32 by Espressif Systems **2.0.17**, ArduinoJson **6.21.5**, and WebSockets **2.6.1** on the computer beforehand. Select **ESP32S3 Dev Module**, 16 MB Flash, QIO, OPI PSRAM and the actual serial port. See the [full Persian offline/first-time installation guide](docs/install-fa.md) before flashing.
- The Sketch includes `partitions.csv`, firmware sources, and generated `EmbeddedAssets.cpp`: all UI, Vazirmatn fonts, photos and guide pages are inside the *program firmware*. One Arduino IDE **Verify → Upload** installs them together. There is **no filesystem-image upload, PlatformIO requirement, cloud or CDN at runtime**. Configuration lives separately in LittleFS on the same board; a truly blank filesystem partition is formatted once, but damaged existing data is never silently erased.
- Updating web assets as a developer: edit `data/`, then run `python3 scripts/export_arduino.py` to refresh the committed sketch. **End users do not need Python or this generator.**

**Offline preparation is not magic:** an empty computer with no internet still needs an Arduino IDE installer and matching ESP32 toolchain/libraries transferred from a previously prepared computer. Those large OS-specific executables are not bundled in this repository. The controller itself never depends on the internet after installation. Keep `Erase All Flash Before Sketch Upload` disabled when updating a provisioned board, export a backup first, and do not change the partition layout.

Connect to the unique `SmartHome-Setup-XXXX` access point printed in the serial log; its random 16-character password is also printed there. Visit `http://192.168.4.1`. Follow the setup wizard. After reboot, connect your browser to the controller's LAN IP (shown via serial/network configuration). Keep the controller on a trusted isolated Wi-Fi segment: **HTTP credentials and cookies are not encrypted in transit**. After the firmware has fully booted, holding BOOT/GPIO0 for five seconds clears only Wi-Fi credentials and restarts into setup mode; **do not hold BOOT during power-on/reset**, which enters the ESP32 ROM download mode instead; administrator and device configuration remain. Setup AP may also reappear after a prolonged Wi-Fi outage, but authenticated endpoints remain protected.

## Implemented in the firmware preview

| Area | Implemented behavior |
|---|---|
| Boot/storage | Firmware-embedded offline UI; LittleFS config with version/board check, temp file and previous-file recovery; virgin-only filesystem initialization; small secrets in Preferences; safe OFF relay initialization |
| Provisioning | Passworded per-device setup AP, five-step UI, Wi-Fi scan/configuration and administrator creation, controlled reconnect/recovery AP |
| Security | PBKDF2-HMAC-SHA256 (100k iterations, random salt), administrator session in HttpOnly SameSite cookie, 8h idle / 24h absolute expiry, CSRF token, same-site (Origin / Sec-Fetch-Site) check, per-client login throttle, password rotation; up to three concurrent sessions |
| Device control | Max 24 devices: active-high/low relay, digital input, ADC1 analog input with scale/offset, virtual control; firmware GPIO allowlist, duplicate-pin check, optional output interlock and maximum ON timer; edits re-arm only the affected output |
| Configuration storage | LittleFS JSON with recovery file, and a fixed 32 KB in-memory document that is compacted before every mutation so repeated edits and dashboard publishes keep saving |
| Automation | Max 24 event-triggered value rules with optional condition and bounded delay; max 12 scenes; max 24 weekly schedules; 16-slot bounded action queue; 1-second per-rule cooldown |
| Dashboard | Max 48 logical-grid widgets, compatibility check on mapping, config persisted in LittleFS; drag/drop, inspector, resize, undo/redo, overlap-aware layout, publish to real Overview controls/charts/labels |
| Monitoring | Live readings via authenticated broadcast-only WebSocket (port 81); optional SD historical CSV, 256 KiB rotation per sensor, 128-sample RAM fallback; RTC DS3231 or manual browser clock sync |
| Maintenance | Authenticated status/diagnostics, board map, JSON backup/restore (excludes Wi-Fi/admin credentials), experimental browser-upload OTA with SHA-256 integrity check |

## Not implemented / not validated

The PRD describes a **full production system**; this preview does **not** meet its final acceptance criteria. Not yet implemented: static IP configuration, DHT/SHT31, I2C sensor drivers, PWM/RGB/servo/motor drivers, full visual pinout matching a specific board revision, drag-resize handles/touch drag, independent mobile layouts, multiple dashboards, rich historical range queries/CSV export, persistent event archive, configurable sensor threshold alerts, NTP, HTTPS, firmware signatures/secure boot, role management, robust SD archival policies, multi-user connection/load tests, brownout/power-loss qualification and hardware/EMC/electrical certification. The single-upload sketch **compiled directly via Arduino CLI with ESP32 core 2.0.17** and passed the custom partition/size checks in [CI run 41788456170](https://github.com/wrappedman/Arduino-HomeController/actions/runs/41788456170). The GUI Upload and physical GPIO/flash tests have NOT been performed. Physical relay/RTC/SD tests require the actual board and a safe low-voltage fixture. **Do not treat this preview as safe for doors, heaters, pumps, locks or other critical loads.**

See [architecture](docs/architecture.md), [API](docs/api.md), [hardware](docs/hardware.md), [security](docs/security.md), and [tests/status](docs/testing.md).

## Build validation and illustrated browser test

`npm ci && npm test` runs simulated and authenticated REST-client regression tests; `python3 -m unittest discover -s test -p "test_*.py"` verifies exported Arduino Sketch parity and embedded assets. `scripts/capture-guide.cjs` exercises the actual browser UI and builds guide screenshots. CI compiles the exported Sketch only with Arduino CLI/ESP32 core 2.0.17, equivalent to Arduino IDE's compiler path. There is no PlatformIO step. A green compile still **does not** validate actual board hardware.
