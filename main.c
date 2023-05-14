//Naama Kashani 312400476

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

//create the queue
typedef struct queue_node {
    char *data;
    struct queue_node *next;
} queue_node_t;

typedef struct queue {
    queue_node_t *head;
    queue_node_t *tail;
} queue_t;

void init_queue(queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
}

void enqueue(queue_t *queue, char *data) {
    queue_node_t *new_node = (queue_node_t *) malloc(sizeof(queue_node_t));
    new_node->data = (char *) malloc(strlen(data) + 1);
    strcpy(new_node->data, data);
    new_node->next = NULL;

    if (queue->head == NULL) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
}

char *dequeue(queue_t *queue) {
    if (queue->head == NULL) {
        printf("Queue is empty\n");
        return NULL;
    }

    char *data = queue->head->data;
    queue_node_t *temp = queue->head;
    queue->head = queue->head->next;
    free(temp);

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    return data;
}

//create the unbounded queue
struct UBQ {
    pthread_mutex_t mutex;
    sem_t full;
    queue_t my_queue;
    char type;
    int done;

    char *(*pop_un)(struct UBQ *this);

    void (*init_un)(struct UBQ *this, char type);

    void (*push_un)(char *new, struct UBQ *this);


};


void init_un(struct UBQ *this, char type) {
    this->type = type;
    sem_init(&this->full, 0, 0);
    int result = pthread_mutex_init(&(this->mutex), NULL);
    if (result != 0) {
        printf("Failed to initialize mutex\n");
        exit(-1);
    }
    init_queue(&this->my_queue);
    this->done = 1;
}

void push_un(char *new, struct UBQ *this) {

    pthread_mutex_lock(&this->mutex);
    enqueue(&this->my_queue, new);
    pthread_mutex_unlock(&this->mutex);
    sem_post(&this->full);

}

char *pop_un(struct UBQ *this) {
    sem_wait(&this->full);
    pthread_mutex_lock(&this->mutex);
    char *data = dequeue(&this->my_queue);
    pthread_mutex_unlock(&this->mutex);
    return data;


}

//create the bounded queue
struct BQ {
    pthread_mutex_t mutex;
    sem_t empty, full;
    queue_t my_queue;
    int size;
    int done;

    char *(*pop)(struct BQ *this);

    void (*init)(struct BQ *this, int size);

    void (*push)(char *new, struct BQ *this);


};


void init(struct BQ *this, int size) {
    this->size = size;
    sem_init(&this->empty, 0, this->size);
    sem_init(&this->full, 0, 0);
    int result = pthread_mutex_init(&(this->mutex), NULL);
    if (result != 0) {
        printf("Failed to initialize mutex\n");
        exit(-1);
    }
    init_queue(&this->my_queue);
    this->done = 1;
}

void push(char *new, struct BQ *this) {
    sem_wait(&this->empty);
    pthread_mutex_lock(&this->mutex);
    enqueue(&this->my_queue, new);
    pthread_mutex_unlock(&this->mutex);
    sem_post(&this->full);

}

char *pop(struct BQ *this) {
    sem_wait(&this->full);
    pthread_mutex_lock(&this->mutex);
    char *data = dequeue(&this->my_queue);
    pthread_mutex_unlock(&this->mutex);
    sem_post(&this->empty);
    return data;


}

//define the global variables
struct BQ *producersBQ;
struct UBQ dispatcher_queues[3];
struct BQ co_editor_array;

//define the arguments for the producer
struct ProducerArgs {
    int num;
    int queue_size;
    int num_producers;
};

//
void *producer(void *arg) {

    // Seed the random number generator
    srand(time(NULL));

    // Define the data types
    const char *dataTypes[] = {"SPORTS", "NEWS", "WEATHER"};
    struct ProducerArgs *args = (struct ProducerArgs *) arg;
    // Initialize the queue
    int producer_num = args->num;
    int queue_size = args->queue_size;
    int num_producers = args->num_producers;
    producersBQ[producer_num].init = &init;
    producersBQ[producer_num].init(&producersBQ[producer_num - 1], queue_size);
    int counter = 0;
    int counter_sports = 0, counter_news = 0, counter_whether = 0;
    while (counter < num_producers) {
        sleep(2);
        // Generate a random number to select a data type
        int randNum = rand() % 3;
        const char *dataType = dataTypes[randNum];
        char data[1024];
        strcpy(data, "Producer ");
        char num_str[16];
        sprintf(num_str, "%d", producer_num);
        strcat(data, num_str);
        strcat(data, " ");
        strcat(data, dataType);
        strcat(data, " ");
        char num[16];
        // Generate a random number to select a data type
        switch (randNum) {
            case 0:
                sprintf(num, "%d", counter_sports);
                counter_sports++;
                break;
            case 1:
                sprintf(num, "%d", counter_news);
                counter_news++;
                break;
            case 2:
                sprintf(num, "%d", counter_whether);
                counter_whether++;
                break;


        }
        strcat(data, num);
        strcat(data, "\n");
        //write(1,data,strlen(data));
        push(data, &producersBQ[producer_num - 1]);
        counter++;
    }

    push("DONE", &producersBQ[producer_num - 1]);
    pthread_exit(NULL);

}

void *dispatcher(void *arg) {
    int num = *((int *) arg);
    int flag = 1;
    //initialize the dispatcher queues
    dispatcher_queues[0].init_un = &init_un;
    dispatcher_queues[0].init_un(&dispatcher_queues[0], 'S');
    dispatcher_queues[1].init_un = &init_un;
    dispatcher_queues[1].init_un(&dispatcher_queues[1], 'N');
    dispatcher_queues[2].init_un = &init_un;
    dispatcher_queues[2].init_un(&dispatcher_queues[2], 'W');
    while (flag == 1) {
        flag = 0;
        for (int i = 0; i < num; i++) {
            //if the producer is not done, continue. if all loop continue flag will be 0 and break.
            if (producersBQ[i].done == 0) {
                continue;
            }
            flag = 1;

            char *data = pop(&producersBQ[i]);

            if (strcmp(data, "DONE") == 0) {
                producersBQ[i].done = 0;
            } else {
                //push the data to the dispatcher correct queue
                if (strstr(data, "NEWS")) {
                    push_un(data, &dispatcher_queues[1]);
                }
                if (strstr(data, "WEATHER")) {
                    push_un(data, &dispatcher_queues[2]);

                }
                if (strstr(data, "SPORTS")) {
                    push_un(data, &dispatcher_queues[0]);
                }
            }

        }
    }
    //after all the producers are done, push "DONE" to the dispatcher queues
    for (int i = 0; i < 3; i++) {
        push_un("DONE", &dispatcher_queues[i]);
    }
    pthread_exit(NULL);


}

void *coEditor(void *arg) {
    int num = *((int *) arg);
    while (1) {
        //pop drom the dispatcher queues and push to the co-editor array
        char *data = pop_un(&dispatcher_queues[num]);
        if (strcmp(data, "DONE") != 0) {
            usleep(100000); // sleep for 0.1 seconds (100,000 microseconds)
            push(data, &co_editor_array);
        } else {
            //if the dispatcher queue is done, push "DONE" to the co-editor array and exit
            push(data, &co_editor_array);
            break;
        }
    }
    pthread_exit(NULL);

}

void *screenManager(void *arg) {
    int counter = 0;
    //pop from the co-editor array and write to the screen
    while (counter < 3) {
        char *data = pop(&co_editor_array);
        //if the co-editor array is not done, write.if the counter is 3 break.
        if (strcmp(data, "DONE") != 0) {
            write(1, data, strlen(data));
        } else
            counter++;

    }
    write(1, "DONE", strlen("DONE"));
    pthread_exit(NULL);

}

//count number of producers in the given file
int number_of_producers(char *file, int *option) {
    int num_producers = 0;
    char line[256];
    //option is the format of the config.txt
    *option = 1;
    FILE *file1 = fopen(file, "r");
    //count number os "PRODUCER" in the file
    while (fgets(line, sizeof(line), file1)) {
        if (strncmp(line, "PRODUCER", strlen("PRODUCER")) == 0) {
            num_producers++;
        }
    }
    if (num_producers == 0) {
        *option = 2;
        FILE *file2 = fopen(file, "r");
        int num_lines = 0;
        while (fgets(line, sizeof(line), file2)) {
            if (strcmp(line, "\n") != 0) {
                num_lines++;
                if (num_lines == 3) {
                    num_producers++;
                    num_lines = 0;
                }
            }
        }
    }
    return num_producers;


}

void extract_conf(struct ProducerArgs *args, char *file, int *co_editor_size, int option) {
    char line[256];
    int i = 0;
    FILE *fd = fopen(file, "r");
    //if the format is 1, extract the data from the config.txt file
    if (option == 1) {
        while (fgets(line, sizeof(line), fd) > 0) {
            if (strncmp(line, "PRODUCER", strlen("PRODUCER")) == 0) {
                sscanf(line, "PRODUCER %d", &args[i].num);
                fgets(line, sizeof(line), fd);
                sscanf(line, "%d", &args[i].num_producers);
                fgets(line, sizeof(line), fd);
                sscanf(line, "queue size = %d", &args[i].queue_size);
                fgets(line, sizeof(line), fd);
                i++;
            } else if (strncmp(line, "Co-Editor queue size", strlen("Co-Editor queue size")) == 0) {
                sscanf(line, "Co-Editor queue size = %d", co_editor_size);


            }
        }
    } else {
        //if the format is 2, extract the data from the config.txt file
        while (fgets(line, sizeof(line), fd) > 0) {
            sscanf(line, "%d", &args[i].num);
            fgets(line, sizeof(line), fd);
            sscanf(line, "%d", &args[i].num_producers);
            fgets(line, sizeof(line), fd);
            sscanf(line, "%d", &args[i].queue_size);
            fgets(line, sizeof(line), fd);
            i++;
        }
        fgets(line, sizeof(line), fd);
        sscanf(line, "%d", co_editor_size);


    }

}


int main(int argc, char *argv[]) {
    // Check if file name is provided
    if (argc != 2) {
        exit(-1);
    }
    int co_editor = 0;
    int *co_editor_size = &co_editor;
    int option = 1;
    int *ptr_option = &option;
    int num_producers = number_of_producers(argv[1], ptr_option);
    struct ProducerArgs args[num_producers];
    extract_conf(args, argv[1], co_editor_size, *ptr_option);
    //initialize the size of producer BQ
    producersBQ = (struct BQ *) malloc(sizeof(struct BQ) * num_producers);
    co_editor_array.init = &init;
    co_editor_array.init(&co_editor_array, *co_editor_size);
    pthread_t threads[num_producers];

    //initialize the producer threads
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&threads[i], NULL, producer, &args[i]);
    }
    int index0 = 0, index1 = 1, index2 = 2;
    pthread_t dispatch, coedit1, coedit2, coedit3, screen_manager;
    //create dispatcher, co-editor and screnn manager threads
    pthread_create(&dispatch, NULL, dispatcher, &num_producers);
    pthread_create(&coedit1, NULL, coEditor, &index0);
    pthread_create(&coedit2, NULL, coEditor, &index1);
    pthread_create(&coedit3, NULL, coEditor, &index2);
    pthread_create(&screen_manager, NULL, screenManager, NULL);

    //wait for all the threads to finish and wait for the last thread is the same
    pthread_join(screen_manager, NULL);
    free(producersBQ);
}
