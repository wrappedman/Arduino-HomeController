# Architecture and data flow (preview)

```text
Browser (data/index.html + app.css + app.js, all local)
  ├── GET/POST/PUT/DELETE /api/* → WebServer :80 → session/CSRF → API validators
  │                                                ├── config in LittleFS /config.json
  │                                                ├── device registry → GPIO/ADC
  │                                                ├── scene/rule/schedule bounded queue
  │                                                └── status/backup/update
  └── WebSocket :81 → one-time REST ticket → state/event broadcasts only

Hardware loop (runs even with Wi-Fi down):
  sensors (1s) → value changes → rules → 16-entry action queue → device outputs
             └── sample (30s) → 128-entry RAM ring → 32-entry SD queue → rotating CSV
RTC (DS3231 on I²C 8/9) / manual time sync → weekly scheduler
Preferences (NVS): Wi-Fi credentials, admin name, salt/hash, unique setup AP password
LittleFS: structured config and locally served web assets
```

`src/main.cpp` runs one cooperative loop (WebServer, WebSocket, sampling, history, automation, network). No extra FreeRTOS tasks are created. Web operations and SD reads can still stall this loop; this must be load-tested before production. JSON objects use bounded documents and collection limits. All relays return to OFF on startup and after device-registry edits. Existing relay pins are released after removal/change. A configured interlock group turns peers OFF before another relay in that group turns ON. An ON timer imposes an optional software limit; do not substitute it for an independent cutoff.

## Persistence and time

LittleFS saves JSON to `/config.tmp`, flushes/closes, moves the previous file to `/config.bak`, then renames temp to `/config.json`. On boot, it tries the primary then backup if the config is invalid, and restores a valid backup to the primary path. Failed mounts are not auto-formatted. Config schema is currently `1`, bound to `ESP32-S3-DevKitC-1-N16R8`. A failed save returns HTTP 507 and reverts the in-memory mutation. No migrations exist yet. The previous file is only one rollback level, not a journal.

microSD is optional. Files `h_<device-id>.csv` store `epoch,uptime,value`; each file resets after 256 KiB. This is a **size cap**, not a full time-based retention/export policy. When absent, the last 128 samples *across all sensors* are in RAM. No history survives power loss without SD. Event history is a RAM ring of 40 entries and does not survive reboot. Schedules do not run while the clock is invalid; connect a DS3231 or synchronize the clock in Settings after each boot. POSIX TZ strings such as `UTC0` are used for local time.

## Scope boundaries

The analog driver reports ADC1 raw ratio × 100, then applies `scale` and `offset`; this is not a calibrated temperature/current sensor driver. Digital input is sampled, not debounced for pushbuttons. The demo's attractive temperature/energy figures are **simulated** and are never presented as hardware data. Backend software-generated identifiers are random hexadecimal. Driver types other than relay/digital/analog/virtual must not be registered until their real drivers and pin constraints exist.
