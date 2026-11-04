# Test plan and current evidence

## Automated checks

```sh
npm ci
npm test
python3 -m unittest discover -s test -p 'test_*.py'
node --check data/app.js
pio run                    # requires PlatformIO package registry/toolchain access
pio run -t buildfs          # check web asset packaging (without flashing)
```

`test/test_guide.py` verifies local fonts and all referenced screenshots. `scripts/capture-guide.cjs` runs Chromium against the real frontend to verify publish → visible widget → reload persistence → mapped control, and generates the guide screenshots. `test/test_layout.py` checks the 16 MB OTA partition bounds, local asset sizes and reserved pin conventions. `test/ui-smoke.cjs` uses jsdom to exercise all UI navigation screens, designer widget addition/save, simulated device command and simulated device registration. jsdom alone does **not** exercise a real controller or browser layout; the Chromium browser script does verify real browser rendering and interactions. Neither exercises physical GPIO or electrical outputs.

## Hardware acceptance checklist (not yet performed)

1. Confirm DevKitC-1 board variant, N16R8 flash/PSRAM mode, actual pinout and partition size; boot from both OTA slots.
2. Without relay power: first boot setup AP password, SSID/admin wizard, reconnect, login/cookie/CSRF and setup rejection after provisioning; after boot hold BOOT for five seconds and confirm only Wi-Fi credentials clear.
3. With a **low-voltage** test fixture: output OFF during boot/reset/brownout, active-high/low control, maximum ON timer and interlock, invalid pins and duplicate GPIO rejection, disconnected LAN with local rule still executing.
4. With conditioned inputs: digital changes, analog calibration, rule condition and delay, queue saturation, stale/deleted device behavior.
5. Insert/remove SD during sampling and read history; cycle power during config writes, backup, restore and OTA; verify no unintended output energization.
6. Set RTC and reboot with Wi-Fi disconnected; verify weekly schedule local time/DST, clock-invalid behavior, manual time sync.
7. Run multiple browsers and large configurations, malformed/oversized payloads, Wi-Fi disconnects, login brute force, wrong CSRF, stolen cookie, corrupted SD and failed OTA.
8. Have licensed electrical engineers review isolation, relay ratings, fault cases, enclosure, EMC, input protection and emergency cutoff before using mains.

## Validation status

| Check | Result |
|---|---|
| JavaScript syntax, jsdom smoke, Chromium browser workflow, layout/guide static checks | Passed locally |
| Offline UI assets | No external runtime dependencies |
| PlatformIO compile + LittleFS image | **Passed** for the custom N16R8 board in [GitHub Actions run 41783491385](https://github.com/wrappedman/Arduino-HomeController/actions/runs/41783491385); local sandbox registry remains inaccessible. Build artifacts include binary images and checksums. |
| Board, wiring, relay, RTC, SD, OTA and reliability tests | Not run: no physical ESP32-S3 fixture in the sandbox |

The PRD's final acceptance criteria cannot be claimed until the hardware, security, OTA and power-loss checks above are completed. A successful compile does not establish runtime safety.
