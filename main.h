#ifndef Multithreaded_News_System_h
#define Multithreaded_News_System_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>

// Element Structure
typedef struct {
    char* content;
    struct Element* next;
} Element;

// Bounded Buffer Structure
typedef struct {
    char **buffer;         // Array to hold the elements
    int size;              // Maximum number of elements the buffer can hold
    int front;             // Index of the front element
    int rear;              // Index of the rear element
    int count;             // Number of elements in the buffer
    pthread_mutex_t mutex; // Mutex for thread safety
    sem_t full;            // Semaphore for buffer full
    sem_t empty;           // Semaphore for buffer empty
} BoundedQueue;

// Unbounded Buffer Structure
typedef struct {
    Element* front;             // Element in the front of the queue
    Element* rear;              // Element in the rear of the queue
    int count;                  // Number of elements in the buffer
    pthread_mutex_t mutex;      // Mutex for thread safety
    sem_t full;                 // Semaphore for queue full
} UnBoundedQueue;

// Producer Structure
typedef struct {
    int id;
    int productsCount;
    int queueCapacity;
} Producer;

FILE *openFile(char *path);
void closeFile(FILE *file);
BoundedQueue* createBoundedQueue(int size);
UnBoundedQueue* createUnboundedQueue();
void destroyBoundedQueue(BoundedQueue *queue);
void destroyUnBoundedQueue(UnBoundedQueue *queue);
void insertIntoBoundedQueue(BoundedQueue *queue, char *item);
void insertIntoUnBoundedQueue(UnBoundedQueue *queue, char *item);
char* removeFromBoundedQueue(BoundedQueue *queue);
char* removeFromUnBoundedQueue(UnBoundedQueue *queue);
void* producerHandler(void* p);
void* dispatcher();
void* handleCoEditor(void* index);
void* screenManager();
void readFromConfig(FILE *configFile);
void createProducersQueues();
void createCoEditorsQueues();

#endif //Multithreaded_News_System_h