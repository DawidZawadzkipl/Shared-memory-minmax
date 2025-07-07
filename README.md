# 🔍 Shared Memory Min/Max Finder

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
![POSIX](https://img.shields.io/badge/POSIX-Compliant-green)

A multi-threaded client-server application that finds minimum and maximum values in datasets using POSIX shared memory and semaphores.

## 📝 Description

This project implements a server that can process multiple client requests to find minimum or maximum values in integer arrays. The communication between clients and server is done through shared memory, making it efficient for large datasets.

### ✨ Features
- 🚀 Concurrent client handling using shared memory
- 📊 Real-time statistics tracking
- 💻 Interactive server console with commands
- 🔒 Thread-safe operations using mutexes and semaphores

## 🛠️ Requirements

- 🐧 Linux/Unix operating system
- 🔨 GCC compiler with C99 support
- 🧵 POSIX threads support (pthread)

## 🔧 Compilation

Compile both programs using GCC:

```
# Compile server
gcc -Wall -pthread src/server.c -o server -lrt

# Compile client  
gcc -Wall -pthread src/client.c -o client -lrt
```

⚠️ Note: The `-lrt` flag is required for POSIX real-time extensions (shared memory).

## 🚀 Usage

### 1. Start the server 🖥️

```
./server
```

The server will display:
```
Server started. Commands: stat, reset, quit
Server is up
```

Available server commands:
- `stat` - 📊 Display operation statistics
- `reset` - 🔄 Reset statistics to zero
- `quit` - 🛑 Gracefully shutdown the server

### 2. Run client 💻

In another terminal:

```
./client <filename> <operation>
```

Parameters:
- `<filename>` - 📄 Path to file containing space-separated integers
- `<operation>` - 🔍 Either `min` or `max`

Example:
```
./client test.txt min
```

Output:
```
Value min: 3
```

### 3. Input File Format 📄

The input file should contain space-separated integers:

```
42 17 99 3 56 78 23 45 67 12
```

Multiple lines are supported:
```
10 20 30
-5 15 25
100 200 300
```

## ⚙️ How It Works

1. **Server** 🖥️ creates shared memory segment and waits for clients
2. **Client** 💻 loads data into its own shared memory segment
3. **Client** 📤 sends request to server with reference to its data
4. **Server** ⚡ processes the request in a separate thread
5. **Server** 📨 returns the result through shared memory
6. **Client** ✅ receives result and cleans up resources

### 🔧 Technical Details

The project uses:
- **POSIX Shared Memory** (`shm_open`, `mmap`) for data transfer 💾
- **POSIX Semaphores** for synchronization 🚦
- **POSIX Threads** for concurrent request handling 🧵
- **Mutexes** for protecting shared statistics 🔒

### 📡 Communication Protocol

```
struct query_t {
    enum find_type type;        // Operation: min or max
    char shm_data_name[30];     // Client's shared memory name
    int data_size;              // Number of elements
    int32_t return_value;       // Result
    sem_t sem_start;            // Start processing signal
    sem_t sem_stop;             // Processing complete signal
};
```

## 💡 Example Session

Terminal 1 (Server) 🖥️:
```
$ ./server
Server started. Commands: stat, reset, quit
Server is up
Processed MIN request: result = -15
Processed MAX request: result = 99
stat
Min operations: 1, Max operations: 1
quit
Shutting down server
Server shutdown complete.
```

Terminal 2 (Client) 💻:
```
$ ./client data1.txt min
Value min: -15
$ ./client data2.txt max  
Value max: 99
```

## ⚠️ Error Handling

The program handles various error conditions:
- ❌ Server not running
- 📁 Invalid input file
- ❓ Invalid operation type
- 💾 Shared memory allocation failures
- 🧵 Thread creation failures


## 📜 License

This project is provided as-is under MIT license. 🎓
