# Demo Script — CampusPulse / UniResource

1. Run `./scripts/start.sh` in Linux/WSL.
2. Open `http://127.0.0.1:8080`.
3. Show the role-aware login page.
4. Sign in as Student (`student / student123`) and explain why the student gets a portal button but no server-control buttons.
5. Open the student portal and open a PDF resource.
6. Return to dashboard and sign in as Administrator (`admin / admin123`).
7. Show Overview: requests, requests/sec, queue, active connections and cache hit rate.
8. Open Concurrency and explain the producer/consumer queue + worker pool.
9. Open Memory & CPU and explain `/proc/meminfo`, `/proc/stat`, RSS and thread count.
10. Generate several portal requests and point at worker states and Live Events.
11. Show Resources and upload a PDF if desired.
12. Use Performance Lab: keep request count fixed and compare worker counts 1/2/4/8. Explain benchmark as a controlled workload for measuring throughput and queue/system behavior.
13. Optionally use Fault Lab and point to the worker failure/recovery events.
14. Finish with the request flow: browser → TCP → queue → workers → resource/cache → HTTP response.
