#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

# define N 10


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

struct BQ *array;


struct ProducerArgs {
    int num;
    int queue_size;
    int num_producers;
};

void *producer(void *arg) {

    // Seed the random number generator
    srand(time(NULL));

    // Define the data types
    const char *dataTypes[] = {"SPORTS", "NEWS", "WEATHER"};
    struct ProducerArgs *args = (struct ProducerArgs *) arg;
    int producer_num = args->num;
    int queue_size = args->queue_size;
    int num_producers = args->num_producers;
    array[producer_num].init = &init;
    array[producer_num].init(&array[producer_num - 1], queue_size);
    int counter = 0;
    int counter_sports = 0, counter_news = 0, counter_whether = 0;
    while (counter < num_producers) {
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
        push(data, &array[producer_num - 1]);
        counter++;
    }

    push("DONE", &array[producer_num - 1]);
    pthread_exit(NULL);

}

void *consumer(void *arg) {
    int num = *((int *) arg);
    int flag = 1;
    while (flag == 1) {
        flag = 0;
        for (int i = 0; i < num; i++) {
            if (array[i].done == 0) {
                continue;
            }
            flag = 1;
            char *data = pop(&array[i]);
            
            if (strcmp(data, "DONE")) {
                array[i].done = 0;
            }

        }

    }
}

int number_of_producers(char *file, int *option) {
    int num_producers = 0;
    char line[256];
    *option = 1;
    FILE *file1 = fopen(file, "r");
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

void extract_conf(struct ProducerArgs *args, char *file, int num_producers, int *co_editor_size, int option) {
    char line[256];
    int i = 0;
    FILE *fd = fopen(file, "r");
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
    extract_conf(args, argv[1], num_producers, co_editor_size, *ptr_option);
    array = (struct BQ *) malloc(sizeof(struct BQ) * num_producers);
    producer(&args[0]);
    pthread_t threads[num_producers];
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&threads[i], NULL, producer, &args[i]);
    }


}
