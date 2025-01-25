# KVS
KVS is a key-value store system, developed for our Operating Systems class. This project is divided into two parts, each implemented in a separate branch. This is the first part (kvs_server) and it focuses on creating a key-value store using a hash table, threads, and forks. The second part can be found in the main branch.

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

## Building and Usage of the project

#### Building
To build the project, use the provided Makefile (in the src directory):

```shell
make
```

#### Running the Server
To run the server, use the following command (in the src/server directory):

```shell
./server/kvs <jobs_dir> <max_threads> <backups_max>
```

- `<jobs_dir>`: Directory containing the job files.
- `<max_threads>`: Maximum number of threads to process job files.
- `<backups_max>`: Maximum number of concurrent backups.

## Credits
This project was developed for educational purposes as part of our Operating Systems class. The base code and materials were provided by our teacher, Paolo Romano @ IST.
