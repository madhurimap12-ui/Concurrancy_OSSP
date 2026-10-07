#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 9090
#define MAX_WORKERS 64
#define QUEUE_CAP 256
#define BUF_SIZE 16384
#define SMALL_BUF 512

static char ROOT_DIR[1024];
static char RUNTIME_DIR[1024];
static char LOG_FILE[1024];
static int listen_fd = -1;
static pthread_t workers[MAX_WORKERS];
static int worker_count = 8;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;
static int request_queue[QUEUE_CAP];
static int q_head = 0, q_tail = 0, q_size = 0;
static _Atomic int running = 1;
static _Atomic int fault_requests = 0;
static _Atomic unsigned long requests_total = 0, requests_ok = 0, requests_failed = 0;
static _Atomic unsigned long cache_hits = 0, cache_misses = 0;
static _Atomic unsigned long bytes_sent = 0;
static _Atomic unsigned long active_connections = 0;
static _Atomic unsigned long queue_rejected = 0;
static _Atomic int worker_alive[MAX_WORKERS];
static _Atomic int worker_busy[MAX_WORKERS];
static _Atomic unsigned long worker_requests[MAX_WORKERS];
static struct timespec started_at;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
#define CACHE_CAP 64
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
static char path_cache[CACHE_CAP][1024];
static int path_cache_size = 0;

static double now_seconds(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void timestamp(char *out, size_t n) {
    time_t t = time(NULL); struct tm tmv; localtime_r(&t, &tmv);
    strftime(out, n, "%Y-%m-%dT%H:%M:%S", &tmv);
}

static void log_event(const char *event, int worker, const char *detail) {
    pthread_mutex_lock(&log_mutex);
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        char ts[64]; timestamp(ts, sizeof ts);
        fprintf(f, "{\"timestamp\":\"%s\",\"event\":\"%s\",\"worker\":%d,\"detail\":\"%s\"}\n", ts, event, worker, detail ? detail : "");
        fclose(f);
    }
    pthread_mutex_unlock(&log_mutex);
}

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) atomic_store(&running, 0);
    if (sig == SIGUSR1) atomic_fetch_add(&fault_requests, 1);
}

static void close_client(int fd) { shutdown(fd, SHUT_RDWR); close(fd); }

static int cache_lookup(const char *path) {
    pthread_mutex_lock(&cache_mutex);
    for (int i=0;i<path_cache_size;i++) {
        if (!strcmp(path_cache[i], path)) { pthread_mutex_unlock(&cache_mutex); return 1; }
    }
    if (path_cache_size < CACHE_CAP) { snprintf(path_cache[path_cache_size], sizeof path_cache[0], "%s", path); path_cache_size++; }
    pthread_mutex_unlock(&cache_mutex); return 0;
}

static int parse_request(int fd, char *method, size_t mn, char *path, size_t pn) {
    char buf[BUF_SIZE]; ssize_t n = recv(fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) return -1; buf[n] = 0;
    if (sscanf(buf, "%15s %1023s", method, path) != 2) return -1;
    method[mn-1] = 0; path[pn-1] = 0; return 0;
}

static void send_text(int fd, int code, const char *type, const char *body) {
    const char *reason = code == 200 ? "OK" : code == 404 ? "Not Found" : code == 405 ? "Method Not Allowed" : "Bad Request";
    char hdr[SMALL_BUF]; size_t len = strlen(body);
    int h = snprintf(hdr, sizeof hdr, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n", code, reason, type, len);
    send(fd, hdr, h, 0); send(fd, body, len, 0); atomic_fetch_add(&bytes_sent, (unsigned long)(h + len));
}

static const char *content_type(const char *p) {
    const char *e = strrchr(p, '.');
    if (!e) return "application/octet-stream";
    if (!strcasecmp(e, ".html")) return "text/html; charset=utf-8";
    if (!strcasecmp(e, ".css")) return "text/css";
    if (!strcasecmp(e, ".js")) return "application/javascript";
    if (!strcasecmp(e, ".txt")) return "text/plain; charset=utf-8";
    if (!strcasecmp(e, ".json")) return "application/json";
    if (!strcasecmp(e, ".pdf")) return "application/pdf";
    return "application/octet-stream";
}

static void serve_file(int fd, const char *fullpath, int worker) {
    int hit = cache_lookup(fullpath);
    if (hit) atomic_fetch_add(&cache_hits, 1); else atomic_fetch_add(&cache_misses, 1);
    log_event(hit ? "CACHE_HIT" : "CACHE_MISS", worker, fullpath);
    int f = open(fullpath, O_RDONLY);
    if (f < 0) { atomic_fetch_add(&requests_failed, 1); send_text(fd, 404, "text/plain", "Resource not found\n"); log_event("RESOURCE_NOT_FOUND", worker, fullpath); return; }
    struct stat st;
    if (fstat(f, &st) < 0 || st.st_size > 64*1024*1024) { close(f); atomic_fetch_add(&requests_failed, 1); send_text(fd, 400, "text/plain", "Invalid resource\n"); return; }
    size_t len = (size_t)st.st_size;
    char *data = NULL;
    if (len > 0) {
        data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, f, 0);
        if (data == MAP_FAILED) { data = NULL; }
    }
    char hdr[SMALL_BUF]; int h = snprintf(hdr, sizeof hdr, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n", content_type(fullpath), len);
    send(fd, hdr, h, 0); if (len && data) send(fd, data, len, 0);
    if (data) munmap(data, len);
    close(f); atomic_fetch_add(&bytes_sent, (unsigned long)(h + len)); atomic_fetch_add(&requests_ok, 1);
    log_event("RESOURCE_SERVED", worker, fullpath);
}

static void handle_client(int fd, int worker) {
    char method[16], path[1024];
    if (parse_request(fd, method, sizeof method, path, sizeof path) < 0) { close_client(fd); return; }
    atomic_fetch_add(&requests_total, 1); atomic_fetch_add(&active_connections, 1); atomic_store(&worker_busy[worker], 1); atomic_fetch_add(&worker_requests[worker], 1);
    char detail[1100]; snprintf(detail, sizeof detail, "%s %s", method, path); log_event("REQUEST_RECEIVED", worker, detail);
    if (strcmp(method, "GET") != 0) { send_text(fd, 405, "text/plain", "GET only\n"); atomic_fetch_add(&requests_failed, 1); goto done; }
    if (!strcmp(path, "/health")) { send_text(fd, 200, "text/plain", "OK\n"); atomic_fetch_add(&requests_ok, 1); log_event("HEALTH_CHECK", worker, "OK"); goto done; }
    if (!strcmp(path, "/api/server") || !strcmp(path, "/api/stats")) {
        char body[4096]; double uptime = now_seconds() - ((double)started_at.tv_sec + (double)started_at.tv_nsec/1e9);
        snprintf(body, sizeof body, "{\"running\":%d,\"port\":9090,\"workers\":%d,\"queue\":%d,\"requests\":%lu,\"ok\":%lu,\"failed\":%lu,\"cache_hits\":%lu,\"cache_misses\":%lu,\"bytes_sent\":%lu,\"active\":%lu,\"queue_rejected\":%lu,\"uptime\":%.2f}", atomic_load(&running), worker_count, q_size, atomic_load(&requests_total), atomic_load(&requests_ok), atomic_load(&requests_failed), atomic_load(&cache_hits), atomic_load(&cache_misses), atomic_load(&bytes_sent), atomic_load(&active_connections), atomic_load(&queue_rejected), uptime);
        send_text(fd, 200, "application/json", body); atomic_fetch_add(&requests_ok, 1); goto done;
    }
    if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
        char p[1200]; snprintf(p, sizeof p, "%s/portal/index.html", ROOT_DIR); serve_file(fd, p, worker); goto done;
    }
    if (!strncmp(path, "/portal/", 8)) {
        char safe[1024]; snprintf(safe, sizeof safe, "%s", path + 8); if (strstr(safe, "..")) { send_text(fd, 400, "text/plain", "Bad path\n"); goto done; }
        char p[1400]; snprintf(p, sizeof p, "%s/portal/%s", ROOT_DIR, safe); serve_file(fd, p, worker); goto done;
    }
    if (!strncmp(path, "/resource/", 10)) {
        char safe[1024]; snprintf(safe, sizeof safe, "%s", path + 10); if (strstr(safe, "..")) { send_text(fd, 400, "text/plain", "Bad path\n"); goto done; }
        char p[1400]; snprintf(p, sizeof p, "%s/resources/%s", ROOT_DIR, safe); serve_file(fd, p, worker); goto done;
    }
    send_text(fd, 404, "text/plain", "Route not found\n"); atomic_fetch_add(&requests_failed, 1);
done:
    atomic_store(&worker_busy[worker], 0); atomic_fetch_sub(&active_connections, 1); close_client(fd);
}

static int queue_push(int fd) {
    pthread_mutex_lock(&queue_mutex);
    if (q_size == QUEUE_CAP) { pthread_mutex_unlock(&queue_mutex); atomic_fetch_add(&queue_rejected, 1); return -1; }
    request_queue[q_tail] = fd; q_tail = (q_tail + 1) % QUEUE_CAP; q_size++;
    pthread_cond_signal(&queue_not_empty); pthread_mutex_unlock(&queue_mutex); return 0;
}

static int queue_pop(void) {
    pthread_mutex_lock(&queue_mutex);
    while (q_size == 0 && atomic_load(&running)) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_nsec += 200000000;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&queue_not_empty, &queue_mutex, &ts);
        if (atomic_load(&fault_requests) > 0) { pthread_mutex_unlock(&queue_mutex); return -2; }
    }
    if (q_size == 0) { pthread_mutex_unlock(&queue_mutex); return -1; }
    int fd = request_queue[q_head]; q_head = (q_head + 1) % QUEUE_CAP; q_size--; pthread_cond_signal(&queue_not_full); pthread_mutex_unlock(&queue_mutex); return fd;
}

static void *worker_main(void *arg) {
    int id = *(int*)arg; free(arg); atomic_store(&worker_alive[id], 1); char d[64]; snprintf(d, sizeof d, "worker-%d", id); log_event("WORKER_STARTED", id, d);
    while (atomic_load(&running)) {
        if (atomic_load(&fault_requests) > 0) {
            int expected = atomic_load(&fault_requests);
            if (expected > 0 && atomic_compare_exchange_strong(&fault_requests, &expected, expected-1)) {
                log_event("FAULT_INJECTED", id, "worker fault requested"); atomic_store(&worker_alive[id], 0); return NULL;
            }
        }
        int fd = queue_pop();
        if (fd == -2) continue;
        if (fd < 0) break;
        log_event("WORKER_ASSIGNED", id, "request dequeued"); handle_client(fd, id);
    }
    atomic_store(&worker_alive[id], 0); log_event("WORKER_STOPPED", id, "worker exit"); return NULL;
}

static void spawn_worker(int id) {
    int *p = malloc(sizeof(int)); *p = id;
    if (pthread_create(&workers[id], NULL, worker_main, p) != 0) { free(p); atomic_store(&worker_alive[id], 0); log_event("WORKER_START_FAILED", id, "pthread_create failed"); }
}

static void write_metrics(void) {
    char p[1200]; snprintf(p, sizeof p, "%s/metrics.json", RUNTIME_DIR);
    FILE *f = fopen(p, "w"); if (!f) return;
    fprintf(f, "{\"running\":%d,\"port\":9090,\"workers\":%d,\"queue_size\":%d,\"queue_capacity\":%d,\"requests_total\":%lu,\"requests_ok\":%lu,\"requests_failed\":%lu,\"cache_hits\":%lu,\"cache_misses\":%lu,\"bytes_sent\":%lu,\"active_connections\":%lu,\"queue_rejected\":%lu,\"worker_states\":[", atomic_load(&running), worker_count, q_size, QUEUE_CAP, atomic_load(&requests_total), atomic_load(&requests_ok), atomic_load(&requests_failed), atomic_load(&cache_hits), atomic_load(&cache_misses), atomic_load(&bytes_sent), atomic_load(&active_connections), atomic_load(&queue_rejected));
    for (int i=0;i<worker_count;i++) fprintf(f, "%s{\"id\":%d,\"alive\":%d,\"busy\":%d,\"requests\":%lu}", i?",":"", i+1, atomic_load(&worker_alive[i]), atomic_load(&worker_busy[i]), atomic_load(&worker_requests[i]));
    fprintf(f, "]}\n"); fclose(f);
}

static void *supervisor_main(void *arg) {
    (void)arg;
    while (atomic_load(&running)) {
        for (int i=0;i<worker_count;i++) {
            if (!atomic_load(&worker_alive[i])) {
                int err = pthread_tryjoin_np(workers[i], NULL);
                if (err == 0 || err == ESRCH || err == EINVAL) {
                    if (atomic_load(&running)) { spawn_worker(i); log_event("WORKER_RECOVERED", i, "replacement worker created"); }
                }
            }
        }
        write_metrics(); sleep(1);
    }
    write_metrics(); return NULL;
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT; if (argc > 1) worker_count = atoi(argv[1]); if (argc > 2) port = atoi(argv[2]);
    if (worker_count < 1) worker_count = 1; if (worker_count > MAX_WORKERS) worker_count = MAX_WORKERS;
    if (!getcwd(ROOT_DIR, sizeof ROOT_DIR)) return 1; snprintf(RUNTIME_DIR, sizeof RUNTIME_DIR, "%s/runtime", ROOT_DIR); snprintf(LOG_FILE, sizeof LOG_FILE, "%s/logs/events.jsonl", ROOT_DIR);
    mkdir(RUNTIME_DIR, 0755); mkdir("logs", 0755); FILE *lf = fopen(LOG_FILE, "a"); if (lf) fclose(lf);
    FILE *pf = fopen("runtime/server.pid", "w"); if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }
    signal(SIGINT, signal_handler); signal(SIGTERM, signal_handler); signal(SIGUSR1, signal_handler); signal(SIGPIPE, SIG_IGN); clock_gettime(CLOCK_MONOTONIC, &started_at);
    listen_fd = socket(AF_INET, SOCK_STREAM, 0); if (listen_fd < 0) { perror("socket"); return 1; }
    int yes=1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr = {0}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY); addr.sin_port=htons((uint16_t)port);
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof addr)<0) { perror("bind"); return 1; }
    if (listen(listen_fd, 128)<0) { perror("listen"); return 1; }
    int fl = fcntl(listen_fd, F_GETFL, 0); if (fl >= 0) fcntl(listen_fd, F_SETFL, fl | O_NONBLOCK);
    log_event("SERVER_STARTED", 0, "concurrent resource server");
    for (int i=0;i<worker_count;i++) { atomic_init(&worker_alive[i],0); atomic_init(&worker_busy[i],0); atomic_init(&worker_requests[i],0); spawn_worker(i); }
    pthread_t sup; pthread_create(&sup, NULL, supervisor_main, NULL);
    while (atomic_load(&running)) {
        struct sockaddr_in client; socklen_t cl=sizeof client; int fd=accept(listen_fd,(struct sockaddr*)&client,&cl);
        if (fd<0) {
            if (!atomic_load(&running)) break;
            if (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK) { usleep(20000); continue; }
            continue;
        }
        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&client.sin_addr,ip,sizeof ip); log_event("CONNECTION_ACCEPTED",0,ip);
        if (queue_push(fd)<0) { send_text(fd, 503, "text/plain", "Server queue full\n"); close_client(fd); }
        else log_event("REQUEST_QUEUED",0,"socket queued");
    }
    atomic_store(&running,0); shutdown(listen_fd,SHUT_RDWR); close(listen_fd); pthread_cond_broadcast(&queue_not_empty);
    for (int i=0;i<worker_count;i++) pthread_join(workers[i],NULL); pthread_join(sup,NULL);
    log_event("SERVER_STOPPED",0,"graceful shutdown"); unlink("runtime/server.pid"); write_metrics(); return 0;
}
