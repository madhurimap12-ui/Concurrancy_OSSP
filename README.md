# Concurrent Web Server for University Resource Portal

A Linux-based concurrent web server developed in **C/C++** for serving
university academic resources to multiple clients simultaneously. The
project demonstrates core **Operating Systems and Linux
systems-programming concepts**, including TCP sockets, POSIX threads,
synchronization, file I/O, memory mapping, signals, logging, and
resource monitoring.

## 📌 Project Overview

The **Concurrent Web Server for University Resource Portal** is designed
to provide academic resources such as:

-   University notices
-   Course materials
-   Examination schedules
-   PDF documents
-   HTML files
-   Text documents

The server is designed to handle multiple TCP client requests
concurrently using a **worker-thread pool** and a **shared synchronized
request queue**.

The project focuses on practical implementation of concurrency,
synchronization, file handling, resource management, and performance
analysis in a Linux environment.

## 🎯 Objectives

1.  Develop a concurrent web server using C on Linux.
2.  Handle multiple TCP client connections simultaneously.
3.  Implement a worker-thread pool for request processing.
4.  Design a thread-safe shared request queue.
5.  Use mutexes, condition variables, and semaphores for
    synchronization.
6.  Use file descriptors and `mmap()` for efficient file access.
7.  Implement signal-based server control and graceful shutdown.
8.  Maintain server activity and request logs.
9.  Monitor active threads and server resource usage.
10. Measure throughput, response time, and server performance under
    different workloads.
11. Keep the architecture modular for future scalability.

## 🏗️ System Architecture

``` text
Client Request
      ↓
  TCP Server
      ↓
 Request Queue
      ↓
 Worker Thread
      ↓
Synchronization
      ↓
File Access (mmap)
      ↓
   Response
      ↓
Logging & Monitoring
```

## 🔄 How the Server Works

1.  The server creates a TCP socket.
2.  It binds the socket to the required address and port.
3.  The server listens for incoming client connections.
4.  Client requests are accepted and placed into a shared bounded
    request queue.
5.  Worker threads retrieve requests from the queue.
6.  Synchronization mechanisms protect shared data and coordinate worker
    threads.
7.  The requested academic resource is located and accessed using Linux
    file I/O.
8.  Suitable static or large files can be accessed using `mmap()`.
9.  The requested resource is sent back to the client.
10. Requests, errors, file access, and completion information are
    logged.
11. Signals can be used for server control and graceful shutdown.
12. Linux `/proc` facilities are used for resource monitoring.

## 🧵 Concurrency and Synchronization

The server uses a **POSIX worker-thread pool** so that multiple client
requests can be processed concurrently.

Synchronization mechanisms include:

-   **Mutexes** --- protect shared queues and data structures.
-   **Condition variables** --- coordinate workers waiting for requests.
-   **Semaphores** --- control access to shared and limited resources.
-   **Shared request queue** --- safely transfers client requests to
    worker threads.

These mechanisms help prevent race conditions and coordinate concurrent
request processing.

## 🖥️ Operating Systems Concepts & Linux APIs

  -----------------------------------------------------------------------
  Concept / API                       Purpose
  ----------------------------------- -----------------------------------
  `socket()`                          Creates a TCP socket

  `bind()`                            Assigns an address to the server
                                      socket

  `listen()`                          Enables the server to accept
                                      connections

  `accept()`                          Accepts incoming client connections

  `pthread_create()`                  Creates worker threads

  `pthread_mutex_*`                   Protects shared data

  Condition variables                 Coordinates worker threads

  `sem_*`                             Synchronizes access to shared
                                      resources

  `open()` / `read()` / `write()` /   File access and resource serving
  `close()`                           

  `mmap()`                            Memory-mapped file access

  `signal()` / `sigaction()`          Signal handling and server control

  `/proc` filesystem                  Resource and process/thread
                                      monitoring

  `malloc()` / `free()`               Dynamic memory management

  File descriptors                    Manages sockets and opened files

  `fork()` / `wait()` / `waitpid()`   Optional process-based modules

  `errno`                             System-call error handling

  `strace`                            System-call tracing

  `gdb`                               Debugging
  -----------------------------------------------------------------------

## 🛠️ Technologies & Tools

-   **Operating System:** Linux / Ubuntu
-   **Language:** C / C++
-   **Networking:** TCP/IP Sockets
-   **Concurrency:** POSIX Threads (`pthread`)
-   **Synchronization:** Mutexes, Condition Variables, Semaphores
-   **File Handling:** Linux File I/O
-   **Memory Mapping:** `mmap()`
-   **Signals:** `sigaction()` and related APIs
-   **Monitoring:** `/proc` filesystem
-   **Compiler:** GCC
-   **Build:** Makefile
-   **Debugging:** GDB
-   **System-call Analysis:** strace
-   **Version Control:** Git
-   **Testing:** Linux Terminal and test scripts

## 📊 Performance Testing

The system is intended to be tested under different workloads by
varying:

-   Number of concurrent clients
-   Number of worker threads
-   Requested files
-   Request load

The following metrics can be measured:

-   **Throughput**
-   **Response time**
-   **CPU usage**
-   **Memory/resource consumption**
-   Overall server behavior under concurrent workloads

## 👥 Team Contributions

  -----------------------------------------------------------------------
  Team Member             Roll Number             Responsibility
  ----------------------- ----------------------- -----------------------
  **Revu.Rohith Varma**   2520030070              Server & Networking ---
                                                  TCP socket
                                                  communication, server
                                                  socket creation, client
                                                  connection handling,
                                                  HTTP request
                                                  processing, and
                                                  response handling

  **P. Venkata Sri Sai    2520030183              Concurrency &
  Madhurima**                                     Synchronization ---
                                                  POSIX worker threads,
                                                  shared request queue,
                                                  mutexes, condition
                                                  variables/semaphores,
                                                  and concurrent client
                                                  testing

  **S. Lahari**           2520030311              File Management,
                                                  Monitoring &
                                                  Performance --- file
                                                  I/O, `mmap()`, logging,
                                                  signal handling,
                                                  `/proc`-based resource
                                                  monitoring, and
                                                  performance analysis
  -----------------------------------------------------------------------

## 🚀 Future Enhancements

The modular design allows future additions such as:

-   Configurable worker-pool size
-   File caching
-   Asynchronous file processing
-   Enhanced resource monitoring
-   Improved scalability
-   Additional performance optimization

## 📁 Suggested Project Structure

``` text
Concurrent-Web-Server-for-University-Resource-Portal/
│
├── src/
│   ├── server.c
│   ├── worker.c
│   ├── queue.c
│   ├── file_handler.c
│   ├── logger.c
│   └── monitor.c
│
├── include/
│   └── *.h
│
├── resources/
│   ├── notices/
│   ├── course_material/
│   ├── exams/
│   └── documents/
│
├── tests/
│
├── Makefile
├── README.md
└── LICENSE
```

> **Note:** The structure above is a suggested organization for the
> implementation. The project submission document does not specify the
> exact source-file structure.

## ▶️ Build and Run

The project is intended to be compiled and executed in a Linux/Ubuntu
environment using GCC and a Makefile.

``` bash
make
```

Then run the generated server executable:

``` bash
./server
```

Client-side testing can be performed using suitable TCP/HTTP test
clients or project test scripts.

> The exact executable name, port number, command-line arguments, and
> client commands depend on the final implementation and are not
> specified in the project submission document.

## 📚 Academic Context

**Course:** Operating Systems and Systems Programming (25CS2104E)\
**Term:** 2026--27, Term-I\
**Section:** 03\
**Team:** 09

### Project Title

**Concurrent Web Server for University Resource Portal**

### Faculty

**Dr. K Hema**

## 🔗 Repository

GitHub Repository:

https://github.com/RevuRohithVarma/Concurrent-Web-Server-for-University-Resource-Portal

## 📄 Expected Outcome

The completed system is expected to provide a functional concurrent web
server running on Linux that can serve university academic resources to
multiple clients simultaneously.

The project demonstrates practical Operating Systems and Linux
systems-programming concepts including TCP sockets, POSIX threads,
synchronization, file descriptors, file I/O, `mmap()`, signals, logging,
and `/proc`-based resource monitoring.
