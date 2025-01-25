# KVS

KVS is a key-value store system, developed for our Operating Systems class. This project is divided into two parts, each implemented in a separate branch. The first part (kvs_server) focuses on creating a key-value store using a hash table, threads, and forks. The second part (main branch) extends the system to include client-server communication using named pipes building on top of the code in kvs_server.

## Structure of the Project

### Part 1: KeyValueStore system

1. **WRITE:** Write one or more key-value pairs to the store.
2. **READ:** Read the values of one or more keys from the store.
3. **DELETE:** Delete one or more key-value pairs from the store.
4. **SHOW:** Display all key-value pairs in the store.
5. **WAIT:** Introduce a delay in the execution of commands.
6. **BACKUP:** Create a backup of the current state of the store.

Example Commands:
<pre>
WAIT 1000
WRITE [(d,dinis)(c,carlota)]
READ [x,z,l,v]
SHOW
BACKUP
DELETE [c]
</pre>
    
 ### Part 2: Client-Server Communication

1. **DELAY:** Introduce a delay in the execution of commands.
2. **SUBSCRIBE:** Subscribe to specific keys to receive notifications.
3. **UNSUBSCRIBE:** Unsubscribe from specific keys.
4. **DISCONNECT:** Disconnect the client from the server.

Example Commands:
<pre>
DELAY 1000
SUBSCRIBE [a]
UNSUBSCRIBE [a]
DISCONNECT
</pre>

## Building and Usage of the project


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

- `<jobs_dir>`: Directory containing the job files.
- `<max_threads>`: Maximum number of threads to process job files.
- `<backups_max>`: Maximum number of concurrent backups.
- `<server_fifo_path>`: Path to the server registration FIFO.

#### Running Clients
To run a client, use the following command (in the src/client directory):

```shell
./client/client <client_id> <server_fifo_path>
```

- `<client_id>`: Unique identifier for the client.
- `<server_fifo_path>`: Path to the server registration FIFO.

> [!NOTE]\
> When running multiple clients use different client id's.

## Credits
This project was developed for educational purposes as part of our Operating Systems class. The base code and materials were provided by our teacher, Paolo Romano @ IST.
