# Server/Client Hash Table Application

## Overview

This application demonstrates a server-client architecture for managing a hash table using shared memory (POSIX shm) for interprocess communication. The server initializes and manages a concurrent hash table with operations like insert, read, and delete, while clients enqueue requests for these operations.

## Build Instructions

### 1. Generate Build Files

```
cmake -B build -S .
```

### 2. Build the Project

```
cmake --build build
```

## Run Instructions

### Server

Run the server by specifying the hash table size:

```
./build/server <hash_table_size>
```

**Example:**

```
./build/server 10
```

**Note:** The server must be started before running the client.

### Client

Run the client:

```
./build/client
```

## Program Workflow

### 1. Server Startup

- Initializes a hash table with the specified size.

- Listens for client requests via the shared memory buffer.

### 2. Client Operations

- Enqueues insert, read, and delete operations into the shared memory buffer.

- Each client thread generates unique keys and values for operations.

### 3. Request Processing

- The server dequeues and processes client requests in a loop.

- Requests are handled concurrently using locks for thread safety.

### 4. Shutdown

- Press Ctrl+C to gracefully stop the server, ensuring cleanup of shared memory and resources.

## Code Structure

- `server.cpp`: Implements the server logic, including hash table initialization and request processing.

- `client.cpp`: Implements the client logic, including request generation and queuing.

- `shared_memory.h`: Defines the shared memory structure and request format.

- `hash_map.h`: Implements the hash table with collision resolution using linked lists.

- `linked_list.h`: Implements linked list functionality for managing hash table buckets.

## Notes

- The server and client must run on the same machine and have proper permissions to access shared memory.