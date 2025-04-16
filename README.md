# OS Challenge Canteen 202

## Repo structure

The repo is organised into 5 different branches, each branch except for the main branch represents an experiment. 

- main
- experiment-cache
- experiment-multiprocess
- experiment-multithread
- priority-level

## Experiments

- Experiment Cache (Goh Jinghann s241753)
- Multiprocess Experiment (Gao Tianrun s241728)
- Multithread Experiment (CHUNG Tsz Yan Giann s241734)
- Priority Queue Experiment (Khoo Thien Zhi s242121)

## Caching Experiment (Goh Jinghann s241753)

## Motivation
From running both the client and server files, we can see that the client sends packets of which many are repeated. As a result, we can utilize this to improve our server by storing computations in cache which can improve our server performance as we avoid redundant computations. Therefore, we initialize a hashmap, which acts as our cache storage which would allow repeated packets to be computed much quicker. 

### Implementation
```c
// Cache entry structure for hashmap
struct CacheEntry {
    uint8_t hash[32];
    uint64_t result;
    struct CacheEntry* next;
};

struct CacheEntry* cache[HASHMAP_SIZE];


// Hash function which computes the calculation of the packet 
unsigned long hash_function(uint8_t shahash[32]) {
    unsigned long hash = 5381;
    for (int i = 0; i < 32; i++) {
        hash = ((hash << 5) + hash) + (unsigned long) shahash[i];
    }
    return hash % HASHMAP_SIZE;
}

// Store result into hashmap in the event of a cache miss
void insert_cache(uint8_t hash[32], uint64_t result) {
    unsigned long hash_index = hash_function(hash);
    struct CacheEntry* new_entry = (struct CacheEntry*)malloc(sizeof(struct CacheEntry));
    memcpy(new_entry->hash, hash, 32);
    new_entry->result = result;
    new_entry->next = cache[hash_index];
    cache[hash_index] = new_entry;
}

// Retrieve result from hashmap in the event of a cache hit 
bool retrieve_cache(uint8_t hash[32], uint64_t *result) {
    unsigned long hash_index = hash_function(hash);
    struct CacheEntry* entry = cache[hash_index];
    while (entry != NULL) {
        if (memcmp(entry->hash, hash, 32) == 0) {
            *result = entry->result;
            return true;
        }
        entry = entry->next;
    }
    return false;
}

// For each packet, check if result is in cache else perform computation
// Check if result is in cache 
if (retrieve_cache(client_request.hash, &answer)) {
    printf("Cache hit: returning cached result\n");
} else {
    // Compute the answer if not in cache
    answer = reverseHash(client_request.hash, start, end);
    insert_cache(client_request.hash, answer);
    printf("Cache miss: computed and cached new result\n");
}
```


### Experiment Setup
The baseline server with cache implementation is compared against the base server with only changes to the seed to ensure a different series of packets are transmitted from the client each time. Here, we observe the impact that cache implementation has on the score of the server. The scores are obtained from running the run-client-milestone.sh script. Scores are inversely proportional to speed of the server. 

### Results
|Run number |Score from baseline server     |Score from baseline server with cache implementation|
|-----------|-------------------------------|----------------------------------------------------|
|1          |7204393                        |1494534                                             |
|2          |7251639                        |1447990                                             |
|3          |7204968                        |1478094                                             |
|4          |7351888                        |1453719                                             |
|5          |7213505                        |1505474                                             |
|6          |8055917                        |1450839                                             |
|7          |7768700                        |1571076                                             |
|8          |8392425                        |1683643                                             |
|9          |8260213                        |1552816                                             |
|10         |8168114                        |1658296                                             |
|Average    |7687176                        |1529648                                             |

### Conclusion 
We can conclude that the implementation of cache significantly sped up and improved the server, causing an average of a 5x decrease in scores when the repeatability of packets was set at the default 20%. This is expected as 1/5 packets generated would be a repeat and would result in a cache hit, where the result can be retrieved from the hashmap. This also suggests that if the client had a higher packet repeatabiliy rate, the score would be even lower. 

We also concluded that a hashmap of size 1000 would be ideal. This is because a lower hashmap size would likely lead to segementation faults as the hash function would index out of bounds as well as the higher likelihood of collisions occuring because multiple keys map to the same index. On the other hand, a larger hashmap would result in wasteful memory allocation and result in an overall system performance decrease. 

## Multiprocess Experiment (Gao Tianrun s241728)

## Motivation
From running the command 'lscpu' in the test environment, I noticed that it has 2 cores that it can use. This made me think about introducing multiprocessing into the server code to make it run faster. Although creating a new process is more resource intensive than creating a new thread, true parallelism can be achieved by running processes in the different cores at the same time. To take advantage of this, I should be using multiprocessing to speed up a computationally intensive task. This is because the cores will be able to do the calculations in parallel, which takes full advantage of multiprocessing. Since brute forcing the SHA256 hash is the computationally intensive task in our server code, I decided to split up that workload into different processes that can compute at the same time to significantly speed up the server.


### Implementation
```c
// Divide workload into the different processes
uint64_t range = (end - start) / process_no;
pid_t processes[process_no];

// Spawn child processes
for (int i = 0; i < process_no; ++i) {
    uint64_t range_start = start + i * range;
    uint64_t range_end = (i == process_no - 1) ? end : range_start + range;

    if ((processes[i] = fork()) == 0) {
        process_work(client_socket, client_request, range_start, range_end);
    }
}

```
Each client request is broken into N ranges based on the number of processes `process_no`. N child processes are spawned to brute force their own smaller ranges to try and find the answer to the client request. For the experiment, I will be changing the `process_no` variable to determine the optimal number of processes to generate. 

### Experiment Setup
The server with multiprocessing is compared against the base server without any optimisations to observe the difference that multiprocessing makes to the score of the server. The scores are obtained from running the run-client-milestone.sh script. 

### Results
|Run number|Base server|Server with 2 processes|Server with 3 processes|Server with 4 processes|Server with 5 processes|Server with 6 processes|
|----------|-----------|-----------------------|-----------------------|-----------------------|-----------------------|-----------------------|
|1         |7804320    |481507                 |483407                 |451193                 |517888                 |532727                 |
|2         |7804336    |486352                 |477122                 |466263                 |528811                 |541232                 |
|3         |8015033    |483511                 |482702                 |463078                 |525399                 |541007                 |
|4         |7891048    |484327                 |477565                 |455237                 |523701                 |588551                 |
|5         |7855600    |483800                 |476320                 |457481                 |535298                 |547176                 |
|6         |7966990    |484838                 |481203                 |461293                 |534670                 |578617                 |
|7         |7837048    |483676                 |475429                 |457323                 |527830                 |557936                 |
|8         |7895199    |483186                 |477003                 |450786                 |516312                 |556113                 |
|9         |7720692    |484056                 |477315                 |463693                 |534367                 |587759                 |
|10        |7861494    |483962                 |476284                 |463019                 |532194                 |605375                 |
|Average   |7865167    |483921.5               |478435                 |**458945.6**           |527647                 |563649.3               |

### Conclusion
Utilising multiprocessing definitely sped up the server, causing at least a 15x decrease in scores. From the experiment results, it can be concluded that using 4 processes to split up the reverse hashing process led to the lowest average time, and is the most optimal number of processes to be used. It can also be seen that the scores first decreased then increased, which suggests that as more processes are created to speed up the reverse hashing process, the overhead of generating the new processes as well as running much more processes than there are actual cores can cause the server to slow down and reduce the benefits that more processes bring. This tradeoff is ultimately balanced at 4 processes. 

## Multithread Experiment (CHUNG Tsz Yan Giann s241734)

### Goals
1. Evaluate how the use + number of threads affect server performance
2. Identify the optimal number of threads 
3. Measure stability of server across multiple runs

### Experiment Setup
Additional modules:
- used `pthreads` to implement threads
- added variable to limit the maximum number of threads generated at a time

Metrics:
1. Time to complete all requests, reflected in the output score.
2. Stability measures (standard deviation, range) of performance.
3. Tested Thread Counts: baseline, 1, 2, 4, 8, 16, 64, and unlimited threads.
4. Test Inputs: Each request involves brute-forcing SHA256 hashes within specified ranges.

### Results Summary
| Threads   | Mean Score over 10 runs (Time)    | Speed compared to baseline (x) | Observations           |
|-----------|--------------|----------------|------------------------|
| Baseline  | 128,453,189.2   | 1.00        | Baseline model         |
| 1         | 148,818,015.2   | 0.86           | Baseline model with more overhead, increase in processing time  |
| 2         | 53,655,227.7   | 2.40        | Use of two threads enables multitasking, reduction in processing time   |
| **4**         | **16939085.7**         | **7.58**          | **Optimal solution**    |
| 8         | 18748351.2      | 6.85          | Performance worsens from here on, increase in processing time    |
| 16        | 20090197.7         | 6.39        |   |
| 64        | 21950132.7   |  5.85          |   |
|unlimited  | 22408703.3     | 5.73         |

### Key Observations
1. Performance improvement
* An increase in the number of threads significantly cuts down the processing time (2 threads speeds the processing time up by over 2x, and 4 threads speeds the processing time up by almost 8x). 
* This is because multithreading allows for different processors to handle different tasks simultaneously, instead of limiting the processors to handle each task sequentially and thus spending more time waiting.
2. Optimal Threads
* Performance peaks at 4 threads, achieving over 7.5x speedup over the baseline. 
* This is most likely because the virtual machine used for the server has 4 processors, and 4 threads allows each processor to handle one task simultaneously. 
3. Performance Degration
* Beyond 4 threads or when there is only one thread, performance diminishes.
* This is likely due to thread management overhead and resource contention.
* For beyond 4 threads, even though there are more threads, as each of the four processors can only handle one task at a time, the increased number of threads cannot be processed until the previous threads have been processed, so there is no extra multitasking going on, and thus no reduction in time.
* The increased number of threads also involve more overhead processes (like splitting the tasks into different threads and such), which contribute to the increased amount of time taken.
* This increase in overhead processes also explain why using 1 thread is actually slower compared to the baseline, which does not involve any thread-related overhead processes.


## Priority Queue Experiment (Khoo Thien Zhi s242121)

### Server Overview:
This experiment analyzes the impact of caching on server performance 
**Baseline Server**:
* Uses fixed number of worker threads (4)
* Process tasks using FIFO order
**Priority Scheduled Server**:
* Uses fixed number of worker threads (4)
* Tasks are queued in a max-heap priority queue
* Requests with higher priorities are processed first

### Experiment Setup
* Both servers use 4 worker threads `(NUM_THREADS=4)`.
* Each requests are assigned a priority value
```c
struct PacketRequest {
    uint8_t hash[32];  
    uint64_t start;    
    uint64_t end;       
    uint8_t priority;   // Priority level (0-255, with higher being more important)
};
```

### Implementation
**Max-Heap Implementation**:
   - A max-heap is used to maintain the priority queue.
   - Operations include:
     - **`enqueue()`**: Inserts a new task while preserving heap properties.
     - **`dequeue()`**: Removes and returns the task with the highest priority.

### Enqueue
```c
void enqueue(struct PacketRequest request, int client_socket) {
    if (heap_size >= MAX_HEAP_SIZE) {
        fprintf(stderr, "Priority queue is full\n");
        close(client_socket);
    } else {
        heap[heap_size].request = request;
        heap[heap_size].client_socket = client_socket;
        heapifyUp(heap_size);
        heap_size++;
    }
}
```
### Dequeue
```c
struct PriorityNode dequeue() {
    struct PriorityNode highest_priority_node = heap[0];
    heap[0] = heap[--heap_size];
    heapifyDown(0);
    return highest_priority_node;
}
```

### Experiment Setup
The server with priority queue and maximum 4 threads is compared against the multithreaded server with maximum 4 threads to compare the difference that the priority queue makes. The scores are obtained from running the run-client-milestone.sh script. 

### Results summary
|Run number |Score from multithreaded server|Score from multithreaded server with priority queue|
|-----------|-------------------------------|---------------------------------------------------|
|1          |932405                         |910387                                             |
|2          |931234                         |904381                                             |
|3          |939946                         |914539                                             |
|4          |924817                         |910673                                             |
|5          |930230                         |894169                                             |
|6          |928375                         |903989                                             |
|7          |933683                         |914383                                             |
|8          |940141                         |905132                                             |
|9          |926248                         |902334                                             |
|10         |935203                         |911474                                             |
|Average    |932282.2                       |**907146.1**                                       |

### Evaluation
* Ensure high-priority requests are handled more promptly than low-priority ones
* Tasks with higher priorities experience lower latency
* Priority based scheduling improves server efficiency
* The difference in scores is not that high, it might be even higher if there are more client requests with higher priority levels

## Final Solution
|Run number|Base server|Multiprocessing + Caching|4 threads + 4 processes|Final Solution (4 processes + 1 thread) Times   |
|----------|-----------|-------------------------|-----------------------|-----------------------|
|Average   |256781476  |17488006                 |79422718               |16144150               |

These scores were taken from the OS challenge continuous execution environment webpage. 

In conclusion, we adopted all 4 experiments into our final solution. This resulted in an average 16x improvement compared to the base server. This is due to the enhanced performance of our server in certain scenarios where performance, concurrency and task prioritization are crucial. 

At first we wanted to use 4 processes and threads, but after running it on the continuous execution environment we obtained a worse score, which might be because there are more processes and threads than the cores can handle which can significantly slow down the server. So we decided to use 4 processes and 1 threads, ensuring that we still have both multiprocessing and multithreading while also keeping overhead to the minimum. 

Caching can improve performance by storing frequently accessed data closer to the processing units, reducing access times for multiple processes who leverage multiple CPU cores for parallel processing. With this in mind, tasks can be distributed across these processes and from within each process, multithreading allows for simultaneous execution of tasks which makes better use of resources such as CPU cores and memory. Additionally, by incorporating a priority queue, tasks are executed in a prioritzed order which means higher-priority tasks are processed efficiently which allows the system to enhance performance by handling critical tasks first. All in all, the system is able to handle a higher workload efficiently and reduce the system's score. 

