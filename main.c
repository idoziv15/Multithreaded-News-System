#include "main.h"

#define MAX_LINE_LENGTH 256
#define MESSAGE_SIZE 50
#define CO_EDITORS_COUNT 3
#define DONE_MESSAGE "DONE"

// Global variables
Producer *producers = NULL;
BoundedQueue **producersQueues;
UnBoundedQueue *coEditorsQueues[CO_EDITORS_COUNT];
BoundedQueue *screenManagerQueue;

int screenManagerQueueSize = 0;
int producersCount = 0;

char* findType(char* string) {
    char* keywords[] = {"SPORTS", "NEWS", "WEATHER", DONE_MESSAGE};
    int numKeywords = 4;

    for (int i = 0; i < numKeywords; i++) {
        if (strstr(string, keywords[i]) != NULL) {
            // Return the keyword found
            return keywords[i];
        }
    }

    return NULL;  // No keyword found
}

/**
 * Open a file and return its file descriptor.
 *
 * @param path The path of the file to open.
 * @return The file descriptor of the opened file.
 * @note If opening the file fails, the function writes an error message to stderr and exits the program.
 */
FILE *openFile(char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Error opening configuration file");
        exit(-1);
    }
    return file;
}

/**
 * Close a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @note If closing the file descriptor fails, the function writes an error message to stderr and exits the program.
 */
void closeFile(FILE *file) {
    if (fclose(file) != 0) {
        perror("Error closing file");
        exit(-1);
    }
}

/**
    Creates a new BoundedQueue with the specified size.
    @param size The maximum number of elements the queue can hold.
    @return A pointer to the newly created BoundedQueue.
**/
BoundedQueue* createBoundedQueue(int size) {
    // Allocate memory for the BoundedQueue structure
    BoundedQueue *queue = (BoundedQueue*) malloc(sizeof(BoundedQueue));
    if (queue == NULL) {
        perror("Error creating bounded queue");
        exit(-1);
    }

    // Allocate memory for the buffer array
    queue->buffer = malloc(size * sizeof(char *));

    // Initialize queue properties
    queue->size = size;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;

    // Initialize mutex and semaphores
    pthread_mutex_init(&queue->mutex, NULL);
    sem_init(&queue->full, 0, 0);
    sem_init(&queue->empty, 0, size);

    return queue;
}

/**
    Creates a new UnBoundedQueue.
    @return A pointer to the newly created UnBoundedQueue.
**/
UnBoundedQueue* createUnboundedQueue() {
    // Allocate memory for the BoundedQueue structure
    UnBoundedQueue *queue = malloc(sizeof(UnBoundedQueue));
    if (queue == NULL) {
        perror("Error creating unBounded queue");
        exit(-1);
    }
    // Allocate memory for the buffer array
//    queue->buffer = malloc(coEditorQueueSize * sizeof(char *));
    // Initialize queue properties
    queue->front = NULL;
    queue->rear = NULL;
    queue->count = 0;

    // Initialize mutex and condition variables
    pthread_mutex_init(&queue->mutex, NULL);
    sem_init(&queue->full, 0, 0);

    return queue;
}

/**
    Destroys the BoundedQueue and frees the allocated memory.
    @param queue A pointer to the BoundedQueue to destroy.
*/
void destroyBoundedQueue(BoundedQueue *queue) {
    // Free the buffer of the queue
    free(queue->buffer);

    // Destroy the mutex and condition variables
    pthread_mutex_destroy(&queue->mutex);
    sem_destroy(&queue->full);
    sem_destroy(&queue->empty);

    // Free the BoundedQueue structure
    free(queue);
}

/**
    Destroys the UnBoundedQueue and frees the allocated memory.
    @param queue A pointer to the BoundedQueue to destroy.
*/
void destroyUnBoundedQueue(UnBoundedQueue *queue) {
    // Destroy the mutex and condition variables
    pthread_mutex_destroy(&queue->mutex);
    sem_destroy(&queue->full);
    // Destroy the queue elements
    while (queue->front != NULL) {
        Element *temp = queue->front;
        queue->front = (Element*) queue->front->next;
        free(temp->content);
        free(temp);
    }
    // Free the BoundedBuffer structure
    free(queue);
}

/**
    Inserts an item into the BoundedQueue.
    @param queue A pointer to the BoundedQueue to insert the item into.
    @param item The item to be inserted into the buffer.
*/
void insertIntoBoundedQueue(BoundedQueue *queue, char *item) {
    // Wait until the queue is not full
    sem_wait(&queue->empty);
    // Acquire the mutex lock to ensure exclusive access to the buffer
    pthread_mutex_lock(&queue->mutex);
    // Insert the item into the queue at the rear index
    queue->buffer[queue->rear] = item;
    // Calculate the new rear index in a circular manner
    queue->rear = (queue->rear + 1) % queue->size;
    // Increment the count of elements in the queue
    queue->count++;
    // Release the mutex lock
    pthread_mutex_unlock(&queue->mutex);
    // Signal that the queue is no longer empty
    sem_post(&queue->full);
}

/**
    Inserts an item into the UnBoundedQueue.
    @param queue A pointer to the UnBoundedQueue to insert the item into.
    @param item The item to be inserted into the buffer.
*/
void insertIntoUnBoundedQueue(UnBoundedQueue *queue, char *item) {
    // Create new element with the item to insert the queue
    Element *newElement = (Element*) malloc(sizeof(Element));
    if (newElement == NULL) {
        // Memory allocation failed
        perror("Error: Failed to allocate memory inserting to UnBoundedQueue\n");
        exit(-1);
    }
    newElement->content = item;
    newElement->next = NULL;
    // Acquire the mutex lock to ensure exclusive access to the buffer
    pthread_mutex_lock(&queue->mutex);
    // Insert the new element into the queue
    if (queue->rear == NULL) {
        queue->front = newElement;
        queue->rear = queue->front;
    } else {
        queue->rear->next = (struct Element*) newElement;
        queue->rear = newElement;
    }
    // Increment the elements count of the queue
    queue->count++;
    // Release the mutex lock
    pthread_mutex_unlock(&queue->mutex);
    // Signal that the queue is no longer empty
    sem_post(&queue->full);
}

/**
    Removes and returns an item from the BoundedQueue.
    @param queue A pointer to the BoundedQueue to remove the item from.
    @return The item removed from the buffer.
*/
char* removeFromBoundedQueue(BoundedQueue *queue) {
    // Wait until the queue is not empty
    sem_wait(&queue->full);
    // Acquire the mutex lock to ensure exclusive access to the buffer
    pthread_mutex_lock(&queue->mutex);
    // Get the item from the queue at the front index
    char *item = queue->buffer[queue->front];
    // Calculate the new front index in a circular manner
    queue->front = (queue->front + 1) % queue->size;
    // Decrement the count of elements in the queue
    queue->count--;
    // Release the mutex lock
    pthread_mutex_unlock(&queue->mutex);
    // Signal that the buffer is no longer full
    sem_post(&queue->empty);
    // return the removed item
    return item;
}

/**
    Removes and returns an item from the UnBoundedQueue.
    @param queue A pointer to the UnBoundedQueue to remove the item from.
    @return The item removed from the buffer.
*/
char* removeFromUnBoundedQueue(UnBoundedQueue *queue) {
    // Wait until the queue is not empty
    sem_wait(&queue->full);
    // Acquire the mutex lock to ensure exclusive access to the buffer
    pthread_mutex_lock(&queue->mutex);
    // Get the item from the queue at the front index
    Element *el = queue->front;
    queue->front = (Element*) el->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    // Decrement the count of elements in the queue
    queue->count--;
    // Release the mutex lock
    pthread_mutex_unlock(&queue->mutex);
    char *item = el->content;
    // Free the memory allocated for the item content
    free(el);
    // return the removed item
    return item;
}

/**
    The producer function represents a producer thread that produces a specified
    number of products and inserts them into the corresponding producer's queue in the
    BoundedQueue. It takes a void pointer arg as an argument, which is expected to contain
    the number of products to produce.
*/
void* producerHandler(void* p) {
    Producer* producer = (Producer*) p;
    int sportCount = 0, newsCount = 0, weatherCount = 0;

    // Produce the specified number of products
    for (int i = 0; i < producer->productsCount; i++) {
        char *newMessage = malloc(MESSAGE_SIZE * sizeof(char));
        char *genre = NULL;
        int type = rand() % CO_EDITORS_COUNT;
        // Get the producer random type
        if (type == 0) {
            genre = "SPORTS";
            type = sportCount;
            sportCount++;
        } else if (type == 1) {
            genre = "NEWS";
            type = newsCount;
            newsCount++;
        } else if (type == 2) {
            genre = "WEATHER";
            type = weatherCount;
            weatherCount++;
        }
        // Generate the product information
        sprintf(newMessage, "Producer %d %s %d", producer->id, genre, type);
        // Insert the product into the corresponding producer's queue
        insertIntoBoundedQueue(producersQueues[producer->id], newMessage);
    }
    // Notify the last producer that it is done
    insertIntoBoundedQueue(producersQueues[producer->id], DONE_MESSAGE);

    return NULL;
}

/**
    The dispatcher function represents a dispatcher thread that continuously removes items
    from the producer queues in the BoundedBuffer and dispatches them to the appropriate
    dispatcher queues. It takes a void pointer arg as an argument, which is unused in this
    implementation.
    @param arg Unused argument.
    @return NULL.
*/
void* dispatcher() {
    int doneProducersCount = 0;
    int *doneProducers = malloc(producersCount * sizeof(int));
    if (doneProducers == NULL) {
        perror("Error in: allocating memory for doneProducers\n");
        exit(-1);
    }
    // Initialize the elements of the array
    for (int i = 0; i < producersCount; i++) {
        doneProducers[i] = 0;
    }
    while (1) {
        for (int i = 0; i < producersCount; i++) {
            // Check if all producers are done
            if (doneProducersCount == producersCount) {
                // Send "DONE" message to all co-editors queues
                for (int j = 0; j < CO_EDITORS_COUNT; j++) {
                    insertIntoUnBoundedQueue(coEditorsQueues[j], DONE_MESSAGE);
                }
                // Free the dynamically allocated memory of doneProducers
                free(doneProducers);
                // Finished all queues so return
                return NULL;
            }
            // Check if the producer is already DONE
            if (doneProducers[i]) {
                continue;
            }
            // Remove an item from the producer's queue
            char *item = removeFromBoundedQueue(producersQueues[i]);
            // Get the type of the item
            char *type = findType(item);
            // Determine the appropriate dispatcher queue based on the type
            if (type != NULL) {
                // Check if the item is the "DONE" message
                if (strcmp(type, DONE_MESSAGE) == 0) {
                    doneProducersCount++;
                    doneProducers[i] = 1;
                    continue;
                } else if (strcmp(type, "SPORTS") == 0) {
                    insertIntoUnBoundedQueue(coEditorsQueues[0], item);
                } else if (strcmp(type, "NEWS") == 0) {
                    insertIntoUnBoundedQueue(coEditorsQueues[1], item);
                } else if (strcmp(type, "WEATHER") == 0) {
                    insertIntoUnBoundedQueue(coEditorsQueues[2], item);
                }
            }
        }
    }
}

/**
    The handleCoEditor function represents a co-editor thread that continuously removes items from
    the dispatcher queues and performs editing operations on them. It then inserts the edited
    items into the co-editor queue.
*/
void* handleCoEditor(void* index) {
    int coEditorNum = *((int*) index);
    while (1) {
        // Remove an item from the corresponding co-editor queue
        char *item = removeFromUnBoundedQueue(coEditorsQueues[coEditorNum]);
        // Check if the item is the "DONE" message
        if (strcmp(item, DONE_MESSAGE) == 0) {
            // Insert Done message into the screenManagerQueue
            insertIntoBoundedQueue(screenManagerQueue, DONE_MESSAGE);
            break;
        } else {
            // Sleep for 0.1 seconds
            usleep(100000);
            // Insert the item into the co-editor queue
            insertIntoBoundedQueue(screenManagerQueue, item);
        }
    }
    return NULL;
}

/**
    The screenManager function represents a screen manager thread that continuously retrieves
    items from the co-editor queues and displays them on the screen.
    @return NULL.
*/
void* screenManager() {
    int doneCount = 0;
    while (1) {
        // Remove an item from the co-editor queue
        char *item = removeFromBoundedQueue(screenManagerQueue);
        // Check if the item is the "DONE" message
        if (strcmp(item, DONE_MESSAGE) == 0) {
            doneCount++;
            // Check if all co-editors are done
            if (doneCount == producersCount) {
                printf("%s\n", DONE_MESSAGE);
                break;
            }
        } else {
            printf("%s\n", item);
            free(item);
        }
    }
    return NULL;
}

/**
    Read producer details from a configuration file.
    This function reads each line from the specified configFile and extracts
    the producer details, such as ID, number of products, and queue size. The
    extracted information is used to populate the producers array.
    @param configFile Pointer to the configuration file to read.
*/
void readFromConfig(FILE *configFile) {
    char line[MAX_LINE_LENGTH]; // Buffer to store each line
    int id, productsCount, queueSize;
    int numProducers = 0;

    // Read and process each line from the config file
    while (fgets(line, sizeof(line), configFile) != NULL) {
        // Skip if the line is empty or consists of whitespace
        if (isspace((unsigned char) line[0])) {
            continue;
        }
        // Extract the producer ID
        id = (int) strtol(line, NULL, 10);

        // Get the number of products for the current producer
        fgets(line, sizeof(line), configFile);
        if (feof(configFile)) {
            break;
        }
        productsCount = (int) strtol(line, NULL, 10);

        // Get the queue size for the current producer
        fgets(line, sizeof(line), configFile);
        queueSize = (int) strtol(line, NULL, 10);

        // Resize the producers array and check for allocation failure
        Producer *temp = realloc(producers, (numProducers + 1) * sizeof(Producer));
        if (temp == NULL) {
            free(producers);
            exit(1);
        }

        // Update the producers array with the new producer details
        producers = temp;
        producers[numProducers].id = id - 1;
        producers[numProducers].productsCount = productsCount;
        producers[numProducers].queueCapacity = queueSize;
        // increase number of producers
        numProducers++;
    }

    producersCount = numProducers;
    // Read the screen manager queue size from the config file
    fgets(line, sizeof(line), configFile);
    screenManagerQueueSize = (int) strtol(line, NULL, 10);
    closeFile(configFile);
}

/**
    This function creates the bounded queues for each producer based on the
    number of products and queue capacity specified in the producers array.
*/
void createProducersQueues() {
    // Initialize array for all the producers queues
    producersQueues = malloc(producersCount * sizeof(BoundedQueue*));

    // Create producers queues
    for (int i = 0; i < producersCount; i++) {
        // Create a queue for current producer
        producersQueues[i] = createBoundedQueue(producers[i].queueCapacity);
    }
}

/**
    This function creates unbounded queues for the co-editors. It assigns the
    created queues to the coEditorsQueues array.
*/
void createCoEditorsQueues() {
    for (int i = 0; i < CO_EDITORS_COUNT; i++) {
        coEditorsQueues[i] = createUnboundedQueue();
    }
}

/**
    @brief Main function of the program.
    This function is the entry point of the program. It performs the following steps:
        Checks if the correct number of command-line arguments was provided.
        Opens the configuration file.
        Reads information from the configuration file.
        Creates producers queues.
        Creates producers threads and handles each producer in a different thread.
        Creates co-editors unbounded queues.
        Creates a dispatcher and runs it on a different thread.
        Creates a screen manager queue.
        Creates threads for each co-editor queue and activates each co-editor in a different thread.
        Creates a thread for the screen manager queue.
        Waits for all threads to finish execution.
        Cleans up resources by freeing memory allocated for queues.
    @param argc The number of command-line arguments.
    @param argv An array of strings representing the command-line arguments.
    @return 0 on successful execution.
*/
int main(int argc, char *argv[]) {
    // Check if the correct number of arguments was provided
    if (argc != 2) {
        exit(-1);
    }

    // Open the configuration file
    FILE *configFile = openFile(argv[1]);
    // Get information from the configuration file
    readFromConfig(configFile);

    // Create producers queues
    createProducersQueues();

    // Create producers threads
    pthread_t producerThreads[producersCount];
    // Handle each producer in a different thread
    for (int i = 0; i < producersCount; i++) {
        pthread_create(&producerThreads[i], NULL, producerHandler,&producers[i]);
    }

    // Create co-editors unbounded queues
    createCoEditorsQueues();

    // Create a dispatcher and run it on a different thread
    pthread_t dispatcherThread;
    pthread_create(&dispatcherThread, NULL, dispatcher, NULL);

    // Create screen manager
    screenManagerQueue = createBoundedQueue(screenManagerQueueSize);

    // Create 3 threads one for each co-editor queue
    pthread_t coEditorThreads[CO_EDITORS_COUNT];
    // Activate each co-editor in a different thread
    int coEditorsNum[] = {0, 1, 2};
    for (int i = 0; i < CO_EDITORS_COUNT; i++) {
        pthread_create(&coEditorThreads[i], NULL, handleCoEditor, &coEditorsNum[i]);
    }

    // Create thread for the screen manager queue
    pthread_t screenManagerThread;
    pthread_create(&screenManagerThread, NULL, screenManager, NULL);

    // Wait for all threads to finish:
    // Wait to the producers threads
    for (int i = 0; i < producersCount; i++) {
        pthread_join(producerThreads[i], NULL);
    }
    // Wait for the dispatcher thread
    pthread_join(dispatcherThread, NULL);
    // Wait to the co-editors threads
    for (int i = 0; i < CO_EDITORS_COUNT; i++) {
        pthread_join(coEditorThreads[i], NULL);
    }
    // Wait to the screen manager thread
    pthread_join(screenManagerThread, NULL);

    // Clean up resources:
    // free all producers queues
    for (int i = 0; i < producersCount; i++) {
        destroyBoundedQueue(producersQueues[i]);
    }
    free(producersQueues);
    // free all co-editors queues
    for (int i = 0; i < CO_EDITORS_COUNT; i++) {
        destroyUnBoundedQueue(coEditorsQueues[i]);
    }
    // free the screen manager queue
    destroyBoundedQueue(screenManagerQueue);
    // free the producers array
    free(producers);
    return 0;
}