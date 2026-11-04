# Local API reference (preview)

Base URL: `http://<controller-ip>/api`. JSON for all normal request bodies and responses; errors are `{ "error": { "code": "...", "message": "..." } }`. HTTP 400/401/403/404/409/413/422/429/507 are used as applicable. No CORS is enabled. Other than `bootstrap`, `setup`, `auth/login` and `auth/session`, endpoints require an administrator cookie. Mutations also require `X-CSRF-Token` from `/auth/session` or login and a same-site request (`Origin` matching the host, or `Sec-Fetch-Site` of `same-origin`/`same-site`/`none`). On login the controller sets `shap_session` (HttpOnly; SameSite Strict; eight-hour idle expiry, 24-hour absolute cap). Three concurrent sessions are supported. Failed logins are throttled per client IP (five per minute). Do not use this HTTP interface outside an isolated LAN.

| Endpoint | Methods | Purpose |
|---|---|---|
| `/bootstrap` | GET | setup flag, firmware version and HTTP warning |
| `/setup` | POST | one-time Wi-Fi/admin/home provisioning in setup mode |
| `/auth/login`, `/auth/session`, `/auth/logout`, `/auth/password` | POST, GET, POST, POST | login/session/logout/password rotation |
| `/system/status`, `/system/info`, `/system/diagnostics` | GET | controller details (no secrets) |
| `/system/time` | POST | `{ "epoch": 1730000000 }` sets clock and optional DS3231 |
| `/network/status` | GET | Wi-Fi status and SSID (no password) |
| `/network/scan` | GET | asynchronous nearby SSID scan; available during setup or after login |
| `/network/config` | POST | administrator updates SSID/password, then controller restarts |
| `/devices`, `/automations`, `/scenes`, `/schedules` | GET, POST | list/create bounded records |
| `/<collection>/<id>` | GET, PUT, DELETE | detail/replace/remove record |
| `/devices/<id>/command` | POST | `{ "command": "on" | "off" | "toggle" }` on relay/virtual |
| `/scenes/activate` | POST | `{ "id": "..." }` queues scene actions |
| `/hardware/board`, `/hardware/gpio` | GET | firmware board allowlist and current assignments |
| `/hardware/test` | POST | `{ "deviceId": "...", "command": "on" }` for a registered output |
| `/dashboard` | GET, PUT | one dashboard with validated grid widgets |
| `/sensors`, `/sensors/<id>/history` | GET | sensor registry and up to 48 recent SD/RAM samples |
| `/events` | GET | last 40 volatile events |
| `/storage/status`, `/storage/backup`, `/storage/restore` | GET, GET, POST | capacity, credential-free JSON export and validated import |
| `/firmware/version` | GET | version + signature-support flag (currently false) |
| `/firmware/update` | POST multipart | experimental OTA with mandatory `X-Firmware-SHA256` and CSRF header |
| `/ws-ticket` | POST | 30-second single-use ticket for `ws://<ip>:81/` |

WebSocket client first sends `auth:<ticket>`. Authorized broadcasts are `{"type":"device","id":"...","value":1}`, `{"type":"event","data":{...}}`, or `{"type":"connected"}`. No commands are accepted over WebSocket.

Device example: `{ "name":"Living room relay", "type":"relay", "pin":4, "activeLow":true, "enabled":true, "maxOnSeconds":120, "interlock":"lights" }`. Other types: `digital` (optional `pullup`, `invert`), `analog` (optional `scale`, `offset`, `unit`), `virtual` (no pin). Device ID is assigned by firmware. Rules use `triggerDeviceId`, `triggerOp` (`equals`/`above`/`below`), `triggerValue`, optional `conditionDeviceId`/`conditionOp`/`conditionValue`, `actionDeviceId`, `action` (`on`/`off`/`toggle`), `delaySeconds`, `enabled`. Scenes contain `actions: [{"deviceId":"...","command":"off"}]`. Schedules contain `time: "HH:MM"`, `days` bitmask (Sun=1 … Sat=64), `targetId`, `action`, `enabled`.

**Known API gaps:** no static IP support, no per-sensor calibration/test endpoint beyond registration and sampling, no arbitrary time-range history query, no signed firmware. The synchronous WebServer buffers a request body before app-level JSON size checks: use a trusted isolated LAN until this is hardened.
