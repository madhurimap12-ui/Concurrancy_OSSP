# OS Concept Mapping

| Concept | Implementation | Evidence in demo |
|---|---|---|
| TCP sockets | `socket/bind/listen/accept` in `portal_server.c` | Student portal request |
| Concurrency | POSIX worker threads | Multiple worker cards |
| Producer/consumer | Shared bounded request queue | Queue counter/events |
| Mutual exclusion | pthread mutex | Queue/log/cache protection |
| Condition synchronization | condition variable + timed wait | Workers wait for work |
| File systems | `open/fstat/close` | Resource serving |
| Memory mapping | `mmap/munmap` | Resource serving path |
| Signals | SIGINT/SIGTERM/SIGUSR1 | Stop + Fault Lab |
| Process telemetry | `/proc/meminfo` from Python control layer | Memory dashboard |
| Event logging | JSONL event journal | Live Event Timeline |
| Fault recovery | Supervisor replaces failed worker | Fault Lab |
| Performance | concurrent benchmark requests | Performance Lab |

## Important accuracy note
The dashboard memory calculation is `MemTotal - MemAvailable`. It represents Linux system memory, not a claim that all used memory belongs to this project. Worker/request values are runtime measurements from the machine where the server is executed.
