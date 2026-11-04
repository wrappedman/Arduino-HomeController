# SHAP — Smart Home Automation Platform

An offline-first ESP32-S3 smart-home controller and locally hosted web dashboard. **Current status: hardware-unvalidated firmware candidate, not a certified or production-ready mains controller.** The frontend has a clearly labelled interactive simulation; firmware implements a smaller, real GPIO-backed subset. Do not connect mains loads without professionally designed isolation and independent protection.

## راهنمای فارسیِ تصویری

راهنمای قدم‌به‌قدم با **فونت وزیرمتنِ آفلاین** و تصویرهای واقعیِ ثبت‌شده از همین رابط در [`data/guide-fa.html`](data/guide-fa.html) قرار دارد. پس از اجرای دمو یا نصب فایل‌سیستم، آن را از مسیر **`/guide-fa.html`** باز کنید یا از لینک «راهنمای فارسی» در نوار کناری استفاده کنید. تصویرها از اجرای مرورگری برنامه (حالت شبیه‌سازی) گرفته شده‌اند؛ عکس از سخت‌افزار واقعی نیستند.

## Try the interactive demo

```sh
python3 -m http.server 4173 --bind 0.0.0.0 --directory data
# visit http://localhost:4173
```

When `/api/bootstrap` is absent the UI enters **SIMULATED DEMO** mode. It uses localStorage and does not talk to hardware. Explore all ten navigation views, toggle simulated relays, adjust simulated sensor readings, create/edit/delete devices, rules, scenes and schedules, drag/resize dashboard widgets, undo/redo, publish layouts that actually render on Overview, download/restore a demo backup, and preview the five-step setup wizard. No CDN or external runtime scripts/fonts are loaded; the Persian guide bundles Vazirmatn locally. Use the theme button for dark mode. Demo state can be reset by clearing this site's localStorage (`shap_demo_v2`).

## Target and build

- **Board:** ESP32-S3-DevKitC-1 N16R8 (16 MB QIO flash, 8 MB OPI PSRAM); `boards/shap-esp32-s3-n16r8.json` explicitly defines its flash/PSRAM profile. Confirm the exact board revision, pinout and memory type before flashing.
- **Framework:** Arduino via PlatformIO, `platformio.ini` contains the board and dual-OTA partition layout.
- **Libraries:** ArduinoJson 6 and WebSocketsServer. Built-in LittleFS, SD, Preferences, Update and Wire.

```sh
python3 -m pip install platformio     # or use an isolated Python venv
pio run
pio run -t upload
pio run -t uploadfs  # REQUIRED on first install: uploads local web assets to LittleFS
pio device monitor
```

**Warning:** `uploadfs` rewrites the LittleFS partition and can erase an existing configuration. Export a backup before uploading new web assets on a provisioned controller; ordinary firmware OTA should not overwrite that partition. Firmware will not silently format a failed LittleFS mount.

Connect to the unique `SmartHome-Setup-XXXX` access point printed in the serial log; its random 16-character password is also printed there. Visit `http://192.168.4.1`. Follow the setup wizard. After reboot, connect your browser to the controller's LAN IP (shown via serial/network configuration). Keep the controller on a trusted isolated Wi-Fi segment: **HTTP credentials and cookies are not encrypted in transit**. After the firmware has fully booted, holding BOOT/GPIO0 for five seconds clears only Wi-Fi credentials and restarts into setup mode; **do not hold BOOT during power-on/reset**, which enters the ESP32 ROM download mode instead; administrator and device configuration remain. Setup AP may also reappear after a prolonged Wi-Fi outage, but authenticated endpoints remain protected.

## Implemented in the firmware preview

| Area | Implemented behavior |
|---|---|
| Boot/storage | LittleFS configuration with version/board check, temp file and previous-file recovery; small secrets in Preferences; safe OFF relay initialization |
| Provisioning | Passworded per-device setup AP, five-step UI, Wi-Fi scan/configuration and administrator creation, controlled reconnect/recovery AP |
| Security | PBKDF2-HMAC-SHA256 (100k iterations, random salt), administrator session in HttpOnly SameSite cookie, 8h idle expiry, CSRF token, Origin check, login throttle, password rotation |
| Device control | Max 24 devices: active-high/low relay, digital input, ADC1 analog input with scale/offset, virtual control; firmware GPIO allowlist, duplicate-pin check, optional output interlock and maximum ON timer |
| Automation | Max 24 event-triggered value rules with optional condition and bounded delay; max 12 scenes; max 24 weekly schedules; 16-slot bounded action queue; 1-second per-rule cooldown |
| Dashboard | Max 48 logical-grid widgets, compatibility check on mapping, config persisted in LittleFS; drag/drop, inspector, resize, undo/redo, overlap-aware layout, publish to real Overview controls/charts/labels |
| Monitoring | Live readings via authenticated broadcast-only WebSocket (port 81); optional SD historical CSV, 256 KiB rotation per sensor, 128-sample RAM fallback; RTC DS3231 or manual browser clock sync |
| Maintenance | Authenticated status/diagnostics, board map, JSON backup/restore (excludes Wi-Fi/admin credentials), experimental browser-upload OTA with SHA-256 integrity check |

## Not implemented / not validated

The PRD describes a **full production system**; this preview does **not** meet its final acceptance criteria. Not yet implemented: static IP configuration, DHT/SHT31, I2C sensor drivers, PWM/RGB/servo/motor drivers, full visual pinout matching a specific board revision, drag-resize handles/touch drag, independent mobile layouts, multiple dashboards, rich historical range queries/CSV export, persistent event archive, configurable sensor threshold alerts, NTP, HTTPS, firmware signatures/secure boot, role management, robust SD archival policies, multi-user connection/load tests, brownout/power-loss qualification and hardware/EMC/electrical certification. Firmware and LittleFS images **compiled successfully in CI** ([board-specific build and checksum verification](https://github.com/wrappedman/Arduino-HomeController/actions/runs/41783491385)); inspect the latest run before flashing. Physical relay/RTC/SD tests require the actual board and a safe low-voltage fixture. **Do not treat this preview as safe for doors, heaters, pumps, locks or other critical loads.**

See [architecture](docs/architecture.md), [API](docs/api.md), [hardware](docs/hardware.md), [security](docs/security.md), and [tests/status](docs/testing.md).

## Build validation and illustrated browser test

`npm ci && npm test` runs both the simulated UI and authenticated firmware-mode REST client regression tests; `python3 -m unittest discover -s test -p "test_*.py"` checks assets/partitions. `scripts/capture-guide.cjs` opens the actual browser UI, exercises registration/publish/reload/device command and produces the screenshots used by the Persian guide. `CHROMIUM_PATH=/path/to/chromium npm run test:api-browser` checks the real REST client mode against a controlled firmware-shaped API fixture (not physical hardware). To regenerate them set `CHROMIUM_PATH` to a locally installed Chromium, serve `data/`, then run `npm run screenshots`. GitHub Actions builds firmware (`pio run`), the LittleFS image (`pio run -t buildfs`), and SHA-256 checksums, then uploads an artifact with firmware, filesystem, bootloader and partition images. [A successful N16R8 build](https://github.com/wrappedman/Arduino-HomeController/actions/runs/41783491385) is available; a green compile is still **not** a hardware safety certificate.
