#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <openssl/sha.h>
#include "messages.h"

/* Constant and mutex declarations */
#define HEAP_SIZE 1000
#define HASHMAP_SIZE 1000
#define PROCESS_NO 4
#define THREAD_NO 1

int heap_size = 0;
// Lock to protect priority queue
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
// Lock to protect cache
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* Custom structs */
// PacketRequest structure based on the project specification
struct PacketRequest {
    uint8_t hash[32];  // 32 bytes for the SHA256 hash
    uint64_t start;    // Start of the range
    uint64_t end;      // End of the range
    uint8_t priority;  // Priority level
};

// Cache entry structure for hashmap
struct CacheEntry {
    uint8_t hash[32];
    uint64_t result;
    struct CacheEntry* next;
};

// Structure to be stored in priority queue and hold arguments passed to the thread
struct PriorityNode {
    int client_socket;
    struct PacketRequest request;
};

/* Brute forcing functions */
// Hash function
unsigned long hash_function(uint8_t shahash[32]) {
    unsigned long hash = 5381;
    for (int i = 0; i < 32; i++) {
        hash = ((hash << 5) + hash) + (unsigned long) shahash[i];
    }
    return hash % HASHMAP_SIZE;
}

// Function to compute the SHA256 hash of a 64-bit number
void computeSHA256(uint64_t num, uint8_t* outputHash) {
    unsigned char data[8];
    memcpy(data, &num, sizeof(uint64_t));
    
    SHA256(data, sizeof(data), outputHash);
}

// Function to reverse the hash by brute-forcing within the specified range
uint64_t reverseHash(uint8_t* targetHash, uint64_t start, uint64_t end) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    uint64_t num;

    for (num = start; num < end; num++) {
        computeSHA256(num, hash);

        if (memcmp(hash, targetHash, SHA256_DIGEST_LENGTH) == 0) {
            return num;
        }
    }
    return UINT64_MAX;
}

/* Data Structures */
struct CacheEntry* cache[HASHMAP_SIZE];
struct PriorityNode heap[HEAP_SIZE];

/* Cache functions */
// Insert into hashmap
void insert_cache(uint8_t hash[32], uint64_t result) {
    unsigned long hash_index = hash_function(hash);
    struct CacheEntry* new_entry = (struct CacheEntry*)malloc(sizeof(struct CacheEntry));
    memcpy(new_entry->hash, hash, 32);
    new_entry->result = result;
    new_entry->next = cache[hash_index];
    cache[hash_index] = new_entry;
}

// Retrieve from hashmap
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

/* Priority queue (max-heap) functions */
// Bubble up operation after insertion of new node
void heapifyUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[index].request.priority > heap[parent].request.priority) {
            struct PriorityNode temp = heap[index];
            heap[index] = heap[parent];
            heap[parent] = temp;
            index = parent;
        } else {
            break;
        }
    }
}

// Bubble down operation after removal of node
void heapifyDown(int index) {
    while (2 * index + 1 < heap_size) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;

        if (left < heap_size && heap[left].request.priority > heap[largest].request.priority) {
            largest = left;
        }
        if (right < heap_size && heap[right].request.priority > heap[largest].request.priority) {
            largest = right;
        }
        if (largest != index) {
            struct PriorityNode temp = heap[index];
            heap[index] = heap[largest];
            heap[largest] = temp;
            index = largest;
        } else {
            break;
        }
    }
}

// Add request to priority queue
void enqueue(struct PacketRequest request, int client_socket) {
    pthread_mutex_lock(&queue_mutex);
    if (heap_size >= HEAP_SIZE) {
        // Should not reach here, assuming number of requests < 1000
        fprintf(stderr, "Priority queue is full\n");
        close(client_socket);
    } else {
        heap[heap_size].request = request;
        heap[heap_size].client_socket = client_socket;
        heapifyUp(heap_size);
        heap_size++;
        // Signal threads waiting for work
        pthread_cond_signal(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}

// Remove top node of priority queue
struct PriorityNode dequeue() {
    struct PriorityNode highest_priority_node;
    pthread_mutex_lock(&queue_mutex);
    while (heap_size == 0) {
        // Wait if queue is empty
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    highest_priority_node = heap[0];
    heap[0] = heap[--heap_size];
    heapifyDown(0);
    pthread_mutex_unlock(&queue_mutex);
    return highest_priority_node;
}

/* Process and thread functions */
// Function to do work for each process
void process_work(int fd[8], int client_socket, struct PacketRequest client_request, uint64_t range_start, uint64_t range_end, int i) {
    uint64_t answer = reverseHash(client_request.hash, range_start, range_end);

    if (answer == UINT64_MAX) {
        close(fd[2 * i]);
        close(fd[2 * i + 1]);
        exit(EXIT_FAILURE);
    }

    answer = htobe64(answer);

    // Write to pipe
    close(fd[2 * i]);
    write(fd[2 * i + 1], &answer, sizeof(answer));
    close(fd[2 * i + 1]);

    ssize_t bytes_sent = send(client_socket, &answer, sizeof(answer), 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending response\n");
    }
    exit(EXIT_SUCCESS);
}

// Return the first successful process
pid_t wait_for_success() {
    int status;
    pid_t pid;

    // Check if exited process is successful
    while ((pid = wait(&status)) > 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            return pid;
        }
    }
    return -1;
}

// Thread function to handle the client request
void* worker_thread(void* arg) {
    while (true) {
        struct PriorityNode node = dequeue();
        int client_socket = node.client_socket;
        struct PacketRequest client_request = node.request;

        // Start actual brute forcing
        uint64_t answer;
        int fd[2 * 4];
        bool in_cache = false;
        pthread_mutex_lock(&cache_lock);
        in_cache = retrieve_cache(client_request.hash, &answer);
        pthread_mutex_unlock(&cache_lock);

        // Check if result is in cache
        if (in_cache) {
            printf("Cache hit: returning cached result\n");
            ssize_t bytes_sent = send(client_socket, &answer, sizeof(answer), 0);
            if (bytes_sent < 0) {
                fprintf(stderr, "Error sending response\n");
            }
        } else {
            printf("Cache miss: computed and cached new result\n");
            // Convert start and end to host byte order (big-endian to little-endian)
            uint64_t start = be64toh(client_request.start);
            uint64_t end = be64toh(client_request.end);

            // Divide workload into the different processes
            uint64_t range = (end - start) / PROCESS_NO;
            pid_t processes[PROCESS_NO];

            // Spawn child processes
            for (int i = 0; i < PROCESS_NO; ++i) {
                uint64_t range_start = start + i * range;
                uint64_t range_end = (i == PROCESS_NO - 1) ? end : range_start + range;
                pipe(&fd[2 * i]);
                if ((processes[i] = fork()) == 0) {
                    process_work(fd, client_socket, client_request, range_start, range_end, i);
                }
            }

            pid_t success = wait_for_success();

            // Find successful process to add to cache
            if (success > 0) {
                for (int i = 0; i < PROCESS_NO; ++i) {
                    if (processes[i] == success) {
                        // Find the final answer and store it in the cache
                        uint64_t answer;
                        if (read(fd[2 * i], &answer, sizeof(answer)) > 0) {
                            pthread_mutex_lock(&cache_lock);
                            insert_cache(client_request.hash, answer);
                            pthread_mutex_unlock(&cache_lock);
                        }
                    } else {
                        // Kill all processes as answer is already found
                        kill(processes[i], SIGTERM);
                    }
                }
            }

            // Wait for child processes to finish to prevent zombie processes
            for (int i = 0; i < PROCESS_NO; i++) {
                close(fd[2 * i]);
                close(fd[2 * i + 1]);
                waitpid(processes[i], NULL, 0);
            }
        }

        close(client_socket);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    int port = 5003;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Port number must be between 1 and 65535\n");
            exit(EXIT_FAILURE);
        }
    }

    struct sockaddr_in server_address, client_address;

    // Create a TCP socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        fprintf(stderr, "Unable to create server socket\n");
        exit(EXIT_FAILURE);
    }

    // Enable address reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }

    // Enable port reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt SO_REUSEPORT failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind the socket to the address and port number
    if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "Unable to bind socket\n");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 100) < 0) {
        fprintf(stderr, "Listen failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", port);

    pthread_t threads[THREAD_NO];
    for (int i = 0; i < THREAD_NO; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // Main loop to handle client requests
    int client_socket;
    socklen_t client_address_size = sizeof(client_address);
    while (true) {
        client_socket = accept(server_socket, (struct sockaddr*) &client_address, &client_address_size);
        if (client_socket < 0) {
            fprintf(stderr, "Server accept failed\n");
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, "Accepted a connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        struct PacketRequest client_request;
        ssize_t bytes_read = recv(client_socket, &client_request, sizeof(client_request), 0);

        if (bytes_read < 0) {
            fprintf(stderr, "Error receiving request\n");
            close(client_socket);
            continue;
        }

        // Add request to priority queue and continue listening to client
        enqueue(client_request, client_socket);
    }

    close(server_socket);
    return 0;
}
