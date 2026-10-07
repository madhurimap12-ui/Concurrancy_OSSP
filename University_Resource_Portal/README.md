# University Resource Portal — CampusPulse Edition

A University Resource Portal backed by a concurrent Linux C web server. The upgraded presentation layout keeps the original server architecture and adds role-aware dashboard access, a responsive executive/analytics UI, CPU + memory observability, explicit concurrency visualization, PDF resource support, and a benchmark explanation/workflow.

## Roles
- **Administrator:** overview, resource CRUD/upload, worker pool, telemetry, live events, Fault Lab, Performance Lab, server lifecycle.
- **Student:** read-only Student Dashboard, resources, server observability and a button to open the student portal. Student access does not expose server-control operations.

Demo credentials are local-project credentials: `admin/admin123` and `student/student123`.

## Run
```bash
./scripts/start.sh
```
Open `http://127.0.0.1:8080` and sign in. The C resource server runs on `http://127.0.0.1:9090`.

## Concurrency story
Browser clients → `accept()` → bounded request queue → POSIX worker pool → cache/file/mmap resource path → HTTP response. The queue is synchronized using a mutex and condition variable. This allows multiple requests to be in flight/processed by different workers.

## CPU & memory observability
The dashboard reads Linux `/proc/meminfo` for memory and `/proc/stat` for aggregate CPU utilization. It also reads the C server process status for PID, RSS and thread count. Rolling graphs make changes visible while traffic is generated.

## Performance Lab
The benchmark generates concurrent `/health` requests and reports elapsed time, success/error counts and requests/sec. For a fair experiment, keep request count constant and compare worker configurations such as 1, 2, 4 and 8. Treat results as measurements from the current machine, not fixed claims.

## PDF support
Dashboard → Resources → Add Resource / PDF accepts PDF and common document/text files. A sample `University_Concurrency_Quick_Reference.pdf` is included in `resources/` and catalogued in `runtime/resources.json`.
