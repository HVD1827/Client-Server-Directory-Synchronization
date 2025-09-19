# Networked Directory Synchronization Application  
**CS3205 – Programming Assignment 2 (Exercise 1)**  

## Overview  
This project implements a **directory synchronization system** between a central **server** and multiple **clients** using **TCP sockets** and the Linux **inotify API**.  
- The server monitors a designated *sync directory* and detects file creation, deletion, and movement (recursively, including subdirectories).  
- Updates are broadcasted to all connected clients.  
- Each client maintains a local mirror of the server’s sync directory, while excluding file types listed in its *ignore list*.  
- Both server and client are **multi-threaded** to handle concurrent operations efficiently.  

---

## Build Instructions  
Compile both the server and client using `gcc`:  

```bash
gcc -o syncserver syncserver.c -lpthread
gcc -o syncclient syncclient.c -lpthread
```

---

## Usage  

### **Start the Server**  
```bash
./syncserver <path_to_sync_directory> <port> <max_clients>
```

**Example:**  
```bash
./syncserver ./server_sync 8080 5
```

- `path_to_sync_directory` → Directory to monitor.  
- `port` → TCP port for client connections.  
- `max_clients` → Maximum number of concurrent clients.  

---

### **Start a Client**  
```bash
./syncclient <path_to_local_directory> <path_to_ignore_list_file>
```
## Ignore List Format  
- The ignore list is a plain text file containing comma-separated extensions:  

```
.mp4,.exe,.zip
```

- On connection, the client sends this list to the server. The format used in our implementation is:  

```
<number_of_extensions>;<ext1>;<ext2>;...;<extN>
```

Example sent by client:  
```
3;.mp4;.exe;.zip
```
