# Detailed Project Working — CampusPulse Edition

## 1. Login and roles
The dashboard starts with a role-aware login page. Administrator access exposes control/CRUD/benchmark/fault tools. Student access exposes a read-only learning view, resource browsing, server observability and an explicit button to open the student portal. The student button is not a server-control button.

## 2. Student request path
Browser → TCP socket → C server → bounded synchronized request queue → worker → cache/file/mmap → HTTP response.

## 3. Multiple users / concurrency
Many browser clients can connect at the same time. `accept()` accepts connections, the bounded queue stores work, and the POSIX worker pool consumes queued requests concurrently. The queue is protected by a mutex and condition variable, implementing the producer/consumer pattern.

## 4. Dashboard path
Browser → Python dashboard/control server → runtime JSON/log files → C server telemetry. The dashboard is an observability/control layer; it does not replace the C server.

## 5. Memory and CPU observability
Memory comes from Linux `/proc/meminfo` and uses `MemTotal - MemAvailable` for used memory. CPU utilization is derived from Linux aggregate counters in `/proc/stat`. The dashboard also reads C-server PID/RSS/thread information from `/proc/<pid>/status`. Rolling graphs make changes visible while requests are generated.

## 6. Resource CRUD and PDF
Administrator resource CRUD maintains `runtime/resources.json` and files under `resources/`. The UI explicitly accepts PDF/document/text uploads. The sample `University_Concurrency_Quick_Reference.pdf` is served by the C server with `application/pdf` content type.

## 7. Live events
C-server events are appended to `logs/events.jsonl`. The dashboard shows the chronological event stream so a click/action can be connected to server behavior.

## 8. Fault recovery
`POST /api/fault` sends SIGUSR1 to the C server. A worker consumes the fault request and exits. The supervisor detects the missing worker and creates a replacement. Worker state and event logs make the sequence visible.

## 9. Performance Lab / benchmark
The benchmark creates concurrent `/health` requests. It measures elapsed time, success/error counts and requests per second. A fair comparison keeps workload constant while changing worker counts, e.g. 1, 2, 4 and 8. Results are machine measurements, not fixed project claims.
