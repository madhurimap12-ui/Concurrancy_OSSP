# Dashboard API

Base URL: `http://127.0.0.1:8080`

## Authentication
- `POST /api/login` — local demo login; returns administrator or student role and sets a session cookie.
- `POST /api/logout` — clears the local session.
- `GET /api/me` — returns current session role.

## Read-only APIs
- `GET /api/status` — metrics, Linux memory/CPU telemetry, event tail and resource catalog. Requires dashboard login.
- `GET /api/resources` — list resources; used by the student portal as a read-only catalog.

## Administrator APIs
- `POST /api/resources` — create resource and optionally upload PDF/document content.
- `PUT /api/resources/<id>` — update metadata/file.
- `DELETE /api/resources/<id>` — delete resource/file.
- `POST /api/start` — ensure C server is running; optional `{ "workers": 8 }`.
- `POST /api/stop` — cleanly stop C server.
- `POST /api/fault` — request controlled worker fault/recovery using SIGUSR1.
- `POST /api/performance` — run concurrent `/health` requests and report measured throughput.

The C server also exposes `GET /health`, `GET /api/stats`, `GET /`, and `GET /resource/<filename>`.
