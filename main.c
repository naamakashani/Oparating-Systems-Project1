// Noga Ben Ari 208304220
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>




//the bounded queue
typedef struct {
    char **buffer;
    int size;
    int start;
    int end;
    sem_t full;//number of full members in the queue
    sem_t empty; //number of empty members in the queue
    pthread_mutex_t mutex; //used to synchronize access to the shared deque object.
} BoundedQueue;

// constructor
BoundedQueue *create_bounded_queue(int capacity) {
    BoundedQueue *queue = malloc(sizeof(BoundedQueue));
    queue->buffer = malloc(sizeof(char *) * capacity);
    queue->size = capacity;
    queue->start = 0;
    queue->end = 0;
    //initiation of the semaphores
    sem_init(&queue->full, 0, 0);
    sem_init(&queue->empty, 0, capacity);
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

//destructor
void destroy_bounded_queue(BoundedQueue *queue) {
    free(queue->buffer);
    sem_destroy(&queue->full);
    sem_destroy(&queue->empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

//inserts a new object
void push_to_bounded_queue(BoundedQueue *queue, char *element) {
    //waits for empty members to push to them the item - means non-zero value
    sem_wait(&queue->empty);
//lock before pushing a new element
    pthread_mutex_lock(&queue->mutex);
    queue->buffer[queue->end] = element;
    queue->end = (queue->end + 1) % queue->size;
    //after inserting the element - release the lock
    pthread_mutex_unlock(&queue->mutex);
//    increase the value of the full members
    sem_post(&queue->full);
}

//removes the first object from the bounded buffer and return it to the user
char *pop_from_bounded_queue(BoundedQueue *queue) {
    sem_wait(&queue->full);
    pthread_mutex_lock(&queue->mutex);
    //lock before pop a element

    char *element = queue->buffer[queue->start];
    queue->start = (queue->start + 1) % queue->size;
    //release the lock
    pthread_mutex_unlock(&queue->mutex);
//    increase the value of the empty members
    sem_post(&queue->empty);
    return element;
}


//the unbounded queue
typedef struct {
    char **buffer;
    int size;
    int start;
    sem_t full;//number of full members in the queue
    pthread_mutex_t mutex; // used to synchronize access to the shared deque object
} UnboundedQueue;

// constructor
UnboundedQueue *create_unbounded_queue() {
    UnboundedQueue *queue = malloc(sizeof(UnboundedQueue));
    queue->buffer = malloc(sizeof(char *));
    queue->size = 1;
    queue->start = 0;
    sem_init(&queue->full, 0, 0);
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

//destructor
void destroy_unbounded_queue(UnboundedQueue *queue) {
    free(queue->buffer);
    sem_destroy(&queue->full);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

// inserts a new object
void push_to_unbounded_queue(UnboundedQueue *queue, char *element) {
    pthread_mutex_lock(&queue->mutex);
    queue->buffer = realloc(queue->buffer, sizeof(char *) * (queue->size + 1));
    queue->buffer[queue->size - 1] = element;
    queue->size++;
    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->full);
}

// removes the first object from the unbounded buffer and return it to the user
char *pop_from_unbounded_queue(UnboundedQueue *queue) {
    sem_wait(&queue->full);
    pthread_mutex_lock(&queue->mutex);
    char *element = queue->buffer[queue->start];
    queue->start++;
    pthread_mutex_unlock(&queue->mutex);
    return element;
}


// Global variables
char *news[] = {"SPORTS", "NEWS", "WEATHER"};
BoundedQueue **producersQueues;
UnboundedQueue **dispatcherQueues;
BoundedQueue *sharedQueue;
pthread_t *producers;
pthread_t *coEditors;
pthread_t dispatcherThread;
pthread_t screenManagerThread;
int numOfProducers = 0;


typedef struct {
    int id;
    int numberOfProducts;
} producer_parms;

//producer
void *producer(void *args) {
    //args is of type producer_parms
    producer_parms *parms = args;
    int id = parms->id;
    int numberOfProducts = parms->numberOfProducts;
    int amount[3] = {0,0,0};
    for (int i = 0; i < numberOfProducts; i++) {
        int newsId = rand() % 3;
        const char* format = "Producer %d %s %d";
        int length = snprintf(NULL, 0, format, id , news[newsId], amount[newsId]);
        // Create a dynamic buffer with the calculated length
        char* str = (char*)malloc(length + 1);

        // Use snprintf with the dynamic buffer
        snprintf(str, length + 1, format, id , news[newsId], amount[newsId]);
        push_to_bounded_queue(producersQueues[id - 1], str);
//        printf("push:  to producer %d - %s \n", id, str); //test
        amount[newsId]++;

    }
    // done with all the news
    push_to_bounded_queue(producersQueues[id-1], "DONE");
//    printf("push:  to producer %d - DONE\n", id); //test
    free(parms);


    return NULL;
}

typedef struct {
    int id;
} coEditor_id;


void *co_editor(void *args) {
    coEditor_id *coEditorId = args;
    int id = coEditorId->id;
    //get the value from the dispatcherQueues
    char *str = pop_from_unbounded_queue(dispatcherQueues[id]);
//    printf("pop: from dispatcher %d - %s\n", id, str); //test
    //runs until the popped string is equal to the string "DONE"
    while (strcmp(str, "DONE") != 0) {
        // Sleep for 100ms
        usleep(100000);

        push_to_bounded_queue(sharedQueue, str);
//        printf("push: to shared %s\n", str); //test
        str = pop_from_unbounded_queue(dispatcherQueues[id]);
//        printf("pop: from dispatcher %d - %s\n", id, str); //test
    }
    destroy_unbounded_queue(dispatcherQueues[id]);
    dispatcherQueues[id]=NULL;
    push_to_bounded_queue(sharedQueue, "DONE");
//    printf("push: to shared - DONE\n"); //test
    free(coEditorId);
    return NULL;
}


//dispacher
void *dispacher() {
    unsigned long long queues = numOfProducers;
    unsigned long long currentQueue = 0;
    while (queues != 0) {
        currentQueue = (currentQueue + 1) % numOfProducers;
        if (producersQueues[currentQueue] == NULL) {
            continue;
        }
        char *str = pop_from_bounded_queue(producersQueues[currentQueue]);
//        printf("pop:  from producer %llu - %s\n", currentQueue+1, str); //test
        if (strcmp(str, "DONE") == 0) {
            queues--;
            destroy_bounded_queue(producersQueues[currentQueue]);
            producersQueues[currentQueue] = NULL;
            continue;
        }
        for (int i = 0; i < 3; i++) {
            if (strstr(str, news[i]) != NULL) {
                push_to_unbounded_queue(dispatcherQueues[i], str);
//                printf("push: to dispatcher %d - %s\n", i, str); //test
                break;
            }
        }
        currentQueue++;
    }
    for (size_t i = 0; i < 3; i++) {
        push_to_unbounded_queue(dispatcherQueues[i], "DONE");
//        printf("push: to dispatcher %zu - DONE\n", i); //test
    }
    return NULL;

}


// waits for elements to be available in the shared queue and prints them to the console.
void *screenManager() {
    //counts the "DONE"
    int dones = 0;
    while (dones < 3) {
        // get an element from the shared queue.
        char *str = pop_from_bounded_queue(sharedQueue);
//        printf("pop: from shared- %s\n", str); //test
        if (strcmp(str, "DONE") == 0) {
            dones++;
        } else {
            printf("%s\n", str);
        }
    }
    destroy_bounded_queue(sharedQueue);
    sharedQueue = NULL;
    //when it has received as many messages as the total size of the "news" array
    printf("DONE\n");
    return NULL;
}


//reads the configuration file and inits the globals variables and threads
void init(char *config_file) {
    int num1, num2, num3;
    //read the configuration file
    FILE *config_f = fopen(config_file, "r");
    //if the open operation was failed
    if (config_f == NULL) {
        printf("Error opening config file\n");
        exit(-1);
    }
    //counts the number of producers in the configuration file
    while (fscanf(config_f, "%d", &num1) == 1) {
        if (fscanf(config_f, "%d", &num2) == 1 && fscanf(config_f, "%d", &num3) == 1) {
            numOfProducers++;
        }
    }
    fseek(config_f,0,SEEK_SET);
    //producers queues
    producersQueues = malloc(sizeof(BoundedQueue*) * numOfProducers);
    //dispatcher queues
    dispatcherQueues = malloc(sizeof(UnboundedQueue*) * 3);
    producers = malloc(sizeof(pthread_t) * numOfProducers);
    char *line = NULL;
    size_t len = 0;
    int co_size = -1;
    int readFile = 1;
    int prod_num = 0;
    while (readFile != -1) {
        int numbers[3];
        for (int i = 0; i < 3; i++) {
            readFile = getline(&line, &len, config_f);
            if (readFile == -1) {
                co_size = numbers[0];
                break;
            }//if the line is empty
            if (line[0] == '\n' || line[0] == '\r') {
                i = -1;
                continue;
            }
            line[strcspn(line, "\r\n ")] = '\0';
//            printf("%s\n", line);
            numbers[i] = atoi(line);
        }
        //creation of producer
        if(co_size!=-1){
            break;
        }
        producersQueues[prod_num] = create_bounded_queue(numbers[2]);
        pthread_t id;
        producer_parms *parms = malloc(sizeof(producer_parms));
        parms->id = numbers[0];
        parms->numberOfProducts = numbers[1];
        //a thread for every producer
        pthread_create(&id, NULL, &producer, parms);
        producers[prod_num] = id;
        prod_num++;
    }
    //creation of the co - editors shared queue
    sharedQueue = create_bounded_queue(co_size);
    coEditors = malloc(sizeof(pthread_t)*3);
    //create a thread for each co- editor
    for (int j = 0; j < 3; j++) {
        coEditor_id *co_parms= malloc(sizeof(coEditor_id));
        co_parms->id = j;
        dispatcherQueues[j] = create_unbounded_queue();
        //a thread for every co-editor
        pthread_create(coEditors+j, NULL, &co_editor, co_parms);

    }
    //the creation of the dispatcher thread
    pthread_create(&dispatcherThread, NULL, &dispacher, NULL);
    //the creation of the screen manager thread
    pthread_create(&screenManagerThread, NULL, &screenManager, NULL);
    //closing the configuration file
    fclose(config_f);
}


void waitForAll() {
    size_t num_co_editors = 3;

    for (size_t i = 0; i < numOfProducers; i++) {
        //wait for the completion of each thread
        pthread_join(producers[i], NULL);
    }

    pthread_join(dispatcherThread, NULL);

    for (size_t i = 0; i < num_co_editors; i++) {
        //wait for the completion of each thread
        pthread_join(coEditors[i], NULL);
    }

    pthread_join(screenManagerThread, NULL);

}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("not enough arguments");
        exit(-1);
    }
    init(argv[1]);
    waitForAll();

    return 0;
}
