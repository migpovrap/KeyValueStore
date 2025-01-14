# KVS

KVS is a key-value store system developed for our operating systems class. The project is divided into two parts, each implemented in a separate branch. The first part focuses on creating a key-value store using a hash table, threads, and forks. The second part extends the system to include client-server communication using named pipes, this was based on the work done for the first part.

## Struture of the Project

- ### Part 1: KeyValueStore system

1. **WRITE:** Write one or more key-value pairs to the store.
2. **READ:** Read the values of one or more keys from the store.
3. **DELETE:** Delete one or more key-value pairs from the store.
4. **SHOW:** Display all key-value pairs in the store.
5. **WAIT:** Introduce a delay in the execution of commands.
6. **BACKUP:** Create a backup of the current state of the store.

- ### Part 2: Client-Server Communication

1. **Client-Server Communication:** Clients connect to the server using named pipes and send requests to monitor key-value pairs.
2. **Subscriptions:** Clients can subscribe to specific keys and receive notifications whenever the values of those keys change.
3. **Session Management:** The server manages multiple client sessions concurrently and uses signals to handle client disconnections.


# Building and Usage of the project

#### Building
To build the project, use the provided Makefile (in the src directory):

```shell
make
```

#### Running the Server
To run the server, use the following command (in the src/server directory):

```shell
./server/kvs <jobs_dir> <max_threads> <backups_max> <server_fifo_path>
```

- <jobs_dir>: Directory containing the job files.
- <max_threads>: Maximum number of threads to process job files.
- <backups_max>: Maximum number of concurrent backups.
- <server_fifo_path>: Path to the server registration FIFO.

#### Running the client
To run a client, use the following command (in the src/client directory):

```shell
./client/client <client_id> <server_fifo_path>
```

- <client_id>: Unique identifier for the client.
- <server_fifo_path>: Path to the server registration FIFO.

# License
This project was developed for educational purposes as part of our operating systems class. The base code and materials were provided by our teacher, Paolo Romano, IST@2024.
