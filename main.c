#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "osPRNG.h"

#define MAX_PROCESSES 100
#define MAX_PRIORITY_LEVELS 8
#define MAX_QUEUE_SIZE 100
#define PROB_OF_IO_REQ 10
#define PROB_OF_IO_TERM 4

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int estimated_burst_time;
    int remaining_time;
    int priority;
    int state; // 0: Ready, 1: Running, 2: Waiting, 3: Finished
    int ready_time;
    int wait_time;
    int total_time;
} Process;

typedef struct {
    Process *queue[MAX_QUEUE_SIZE];
    int front;
    int rear;
} PriorityQueue;

typedef struct {
    Process *processes[MAX_PROCESSES];
    int count;
} SJFQueue;

PriorityQueue priority_queues[MAX_PRIORITY_LEVELS];
SJFQueue sjf_queue;
Process processes[MAX_PROCESSES];
int process_count = 0;
int clock = 0;
int scheduling_algorithm = 0; // 0: FMP, 1: SJF
float aging_factor = 0.5;
bool verbose = false;

// variaveis relacionadas com SJF
float ultimo_burst_time = 5;
float ultimo_burst_time_estimado = 5;

int IOReq() {
    if (osPRNG() % PROB_OF_IO_REQ == 0)
        return 1;
    else
        return 0;
}

int IOTerm(){
    if (osPRNG() % PROB_OF_IO_TERM == 0)
        return 1;
    else
        return 0;
}

void init_priority_queues() {
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        priority_queues[i].front = -1;
        priority_queues[i].rear = -1;
    }
}

void enqueue(PriorityQueue *pq, Process *process) {
    if (pq->rear == MAX_QUEUE_SIZE - 1) {
        return; // Fila cheia
    }
    if (pq->front == -1) {
        pq->front = 0;
    }
    pq->rear++;
    pq->queue[pq->rear] = process;
}

Process* dequeue(PriorityQueue *pq) {
    if (pq->front == -1 || pq->front > pq->rear) {
        return NULL; // Fila vazia
    }
    Process *process = pq->queue[pq->front];
    pq->front++;
    if (pq->front > pq->rear) {
        pq->front = pq->rear = -1;
    }
    return process;
}

Process* choose_process_fmp() {
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        if (priority_queues[i].front != -1) {
            return dequeue(&priority_queues[i]);
        }
    }
    return NULL; // Nenhum processo disponível
}

void init_sjf_queue() {
    sjf_queue.count = 0;
}


double calculate_next_estimated_time(float last_burst_time, float previous_estimated_time, float aging_factor) {
    ultimo_burst_time_estimado = (aging_factor * last_burst_time) + ((1 - aging_factor) * previous_estimated_time);
    return ultimo_burst_time_estimado;
}


void add_to_sjf_queue(Process *process) {
    if (process->state == 3) {
        return;
    }
    int i = sjf_queue.count - 1;
    if (process->estimated_burst_time == 0) {
        process->estimated_burst_time = calculate_next_estimated_time(ultimo_burst_time * 1.0, ultimo_burst_time_estimado* 1.0, aging_factor);
    }
    while (i >= 0 && sjf_queue.processes[i]->estimated_burst_time > process->estimated_burst_time) {
        sjf_queue.processes[i + 1] = sjf_queue.processes[i];
        i--;
    }
    sjf_queue.processes[i + 1] = process;
    sjf_queue.count++;
}

Process* remove_from_sjf_queue() {
    if (sjf_queue.count == 0) {
        return NULL;
    }
    Process *process = sjf_queue.processes[0];
    for (int i = 1; i < sjf_queue.count; i++) {
        sjf_queue.processes[i - 1] = sjf_queue.processes[i];
    }
    sjf_queue.count--;
    return process;
}

Process* choose_process_sjf() {
    return remove_from_sjf_queue();
}

void update_statistics(Process *process) {
    if (process->state == 0) { // Ready
        process->ready_time++;
    } else if (process->state == 2) { // Wait
        process->wait_time++;
    } else if (process->state == 1) { // Running
        process->total_time++;
    }
}

void parse_input(char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    char line[100];
    while (fgets(line, sizeof(line), file)) {
        Process process;
        sscanf(line, "%d:%d:%d:%d", &process.pid, &process.arrival_time, &process.burst_time, &process.priority);
        process.remaining_time = process.burst_time;
        process.state = -1; // not in processor
        process.ready_time = 0;
        process.wait_time = 0;
        process.total_time = 0;
        process.estimated_burst_time = 0;
        processes[process_count++] = process;
    }

    fclose(file);
}

void print_statistics() {
    printf("=========+=================+=================+============   \n");
    printf("Processo | Tempo total     | Tempo total     | Tempo total   \n");
    printf("         | em estado Ready | em estado Wait  | no processador\n");
    printf("=========+=================+=================+============    \n");

    for (int i = 0; i < process_count; i++) {
        printf("%d       | %d               | %d               | %d\n", processes[i].pid, processes[i].ready_time,
               processes[i].wait_time, processes[i].total_time);
    }

    printf("=========+=================+=================+============\n");
    printf("Tempo total de simulação.: %d\n", clock);
    printf("Número de processos......: %d\n", process_count);
}

int main(int argc, char **argv) {
    char *input_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-F") == 0) {
            scheduling_algorithm = 0;
        } else if (strcmp(argv[i], "-S") == 0) {
            scheduling_algorithm = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-' && strlen(argv[i + 1]) < 4) {
                aging_factor = atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else {
            input_filename = argv[i];
        }
    }

    if (input_filename == NULL) {
        fprintf(stderr, "Uso: %s [-F | -S [a]] [-v] <inputfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    parse_input(input_filename);
    init_priority_queues();
    init_sjf_queue();

    while (1) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].arrival_time == clock && processes[i].state == -1) {
                processes[i].state = 0; // Ready
                if (scheduling_algorithm == 0) {
                    enqueue(&priority_queues[processes[i].priority], &processes[i]);
                } else {
                    add_to_sjf_queue(&processes[i]);
                }
            }
        }

        for (int i = 0; i < process_count; i++) {
            if (processes[i].state == 2 && IOTerm()) {
                processes[i].state = 0; // Ready
                if (scheduling_algorithm == 0) {
                    enqueue(&priority_queues[0], &processes[i]); // Requisições de I/O retornam com prioridade 0
                } else {
                    add_to_sjf_queue(&processes[i]);
                }
            }
        }

        Process *current_process = NULL;
        if (scheduling_algorithm == 0) {
            current_process = choose_process_fmp();
        } else {
            current_process = choose_process_sjf();
        }

        if (current_process != NULL) {
            current_process->state = 1; // Running
            current_process->remaining_time--;
            if (verbose) {
                printf("%d:%d:%d:%d:%d:%d\n", clock, current_process->pid, current_process->remaining_time,
                       IOReq(), IOTerm(), current_process->state);
            }
            if (current_process->remaining_time == 0) {
                update_statistics(current_process);
                current_process->state = 3; // Finished
                ultimo_burst_time = current_process->burst_time * 1.0;
            } else if (IOReq() && current_process->state == 1) {
                current_process->state = 2; // Wait
            } else {
                if (scheduling_algorithm == 0) {
                    if (current_process->remaining_time > 0) {
                        int new_priority = current_process->priority + 1;
                        if (new_priority >= MAX_PRIORITY_LEVELS) {
                            new_priority = MAX_PRIORITY_LEVELS - 1;
                        }
                        current_process->priority = new_priority;
                        enqueue(&priority_queues[current_process->priority], current_process);
                    }
                } else {
                    add_to_sjf_queue(current_process);
                }
            }
        }

        for (int i = 0; i < process_count; i++) {
            if (processes[i].state != 3 && clock >= processes[i].arrival_time) {
                update_statistics(&processes[i]);
            }
        }

        int all_finished = 1;
        for (int i = 0; i < process_count; i++) {
            if (processes[i].state != 3) {
                all_finished = 0;
                break;
            }
        }
        if (all_finished) break;

        clock++;
    }
    print_statistics();
    // printf("aging: %f", aging_factor);
    // printf("ultimo burst: %f", ultimo_burst_time);
    // printf("ultimo burst estimado: %f", ultimo_burst_time_estimado);
    return 0;
}
