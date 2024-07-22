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

typedef struct
{
    int pid;
    int arrival_time;
    int burst_time;
    float estimated_burst_time;
    int remaining_time;
    int priority;
    int quantum;
    int state; // 0: Ready, 1: Running, 2: Waiting, 3: Finished
    int ready_time;
    int wait_time;
    int total_time;
} Process;

typedef struct
{
    Process *queue[MAX_QUEUE_SIZE];
    int rear;
} PriorityQueue;

typedef struct
{
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
bool process_finished = false;
Process *current_process = NULL;

// variaveis relacionadas com SJF
float ultimo_burst_time = 5;
float ultimo_burst_time_estimado = 5;

// variaveis relacionadas ao verbose
int listaTerminosIO[MAX_PROCESSES];
int existem_terminos = 0;

// variaveis relacionadas ao priority queue
int quantum_restante = 0;

void inserirNaListaDeTerminosIO(int pid)
{
    existem_terminos = 1;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (listaTerminosIO[i] == -1)
        {
            listaTerminosIO[i] = pid;
            break;
        };
    };
};

void limparListaDeTerminosIO()
{
    existem_terminos = 0;
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        listaTerminosIO[i] = -1;
    };
};

void printListaDeTerminosIO()
{
    if (existem_terminos == 1)
    {
        for (int i = 0; i < MAX_PROCESSES; i++)
        {
            if (listaTerminosIO[i] != -1)
            {
                printf("%d:", listaTerminosIO[i]);
            }
        };
    }
    else
    {
        printf("0:");
    }
};

int IOReq()
{
    if (osPRNG() % PROB_OF_IO_REQ == 0)
        return 1;
    else
        return 0;
}

int IOTerm()
{
    if (osPRNG() % PROB_OF_IO_TERM == 0)
        return 1;
    else
        return 0;
}

int calculate_quantum(int priority)
{
    return (1 << priority); // 2^priority
}

void init_priority_queues()
{
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        priority_queues[i].rear = -1;
    }
}

void enqueue(PriorityQueue *pq, Process *process)
{
    if (pq->rear == MAX_QUEUE_SIZE - 1)
    {
        return; // Fila cheia
    }
    pq->rear++;
    pq->queue[pq->rear] = process;
    process->state = 0;
    process->quantum = calculate_quantum(process->priority);
}

Process *dequeue(PriorityQueue *pq)
{
    if (pq->rear == -1)
    {
        return NULL; // Fila vazia
    }
    Process *process = pq->queue[0];
    process->state = 1;

    // Move all elements one position to the front
    for (int i = 1; i <= pq->rear; i++)
    {
        pq->queue[i - 1] = pq->queue[i];
    }
    pq->rear--;

    quantum_restante = process->quantum;
    return process;
}

Process *choose_process_fmp()
{
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        if (priority_queues[i].rear != -1)
        {
            return dequeue(&priority_queues[i]);
        }
    }
    return NULL; // Nenhum processo disponível
}

void init_sjf_queue()
{
    sjf_queue.count = 0;
}

double calculate_next_estimated_time(float last_burst_time, float previous_estimated_time, float aging_factor)
{
    ultimo_burst_time_estimado = (aging_factor * last_burst_time) + ((1 - aging_factor) * previous_estimated_time);
    return ultimo_burst_time_estimado;
}

void add_to_sjf_queue(Process *process)
{
    if (process->state == 3 || process->remaining_time <= 0)
    {
        return;
    }
    int i = sjf_queue.count - 1;
    process->state = 0;
    if (process->estimated_burst_time == 0)
    {
        process->estimated_burst_time = calculate_next_estimated_time(ultimo_burst_time * 1.0, ultimo_burst_time_estimado * 1.0, aging_factor);
    }
    while (i >= 0 && sjf_queue.processes[i]->estimated_burst_time > process->estimated_burst_time)
    {
        sjf_queue.processes[i + 1] = sjf_queue.processes[i];
        i--;
    }
    sjf_queue.processes[i + 1] = process;
    sjf_queue.processes[0]->state = 1;
    sjf_queue.count++;
}

Process *remove_from_sjf_queue()
{
    if (sjf_queue.count == 0)
    {
        return NULL;
    }
    Process *process = sjf_queue.processes[0];
    for (int i = 1; i < sjf_queue.count; i++)
    {
        sjf_queue.processes[i - 1] = sjf_queue.processes[i];
    }
    process->state = 1;
    sjf_queue.count--;
    return process;
}

int get_next_pid()
{
    if (scheduling_algorithm == 1)
    {
        if (sjf_queue.processes[0] != NULL)
        {
            return sjf_queue.processes[0]->pid;
        }
        else
        {
            return -1;
        }
    }
    else if (scheduling_algorithm == 0)
    {
        for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
        {
            if (priority_queues[i].rear != -1)
            {
                return priority_queues[i].queue[0]->pid;
            }
        }
        return -1; // Nenhum processo disponível
    }
    return -1; // Caso inválido
}

Process *choose_process_sjf()
{
    return remove_from_sjf_queue();
}

int compare_arrival_time(const void *a, const void *b)
{
    Process *processA = (Process *)a;
    Process *processB = (Process *)b;
    return processA->arrival_time - processB->arrival_time;
}

void parse_input(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    char line[100];
    while (fgets(line, sizeof(line), file))
    {
        Process process;
        sscanf(line, "%d:%d:%d:%d", &process.pid, &process.arrival_time, &process.burst_time, &process.priority);
        process.remaining_time = process.burst_time;
        process.state = -1; // not in processor
        process.ready_time = 0;
        process.wait_time = 0;
        process.total_time = 0;
        process.estimated_burst_time = 0;
        process.quantum = -1;
        processes[process_count++] = process;
    }

    fclose(file);

    qsort(processes, process_count, sizeof(Process), compare_arrival_time);
}

void print_statistics()
{
    printf("=========+=================+=================+============\n");
    printf("Processo | Tempo total     | Tempo total     | Tempo total\n");
    printf("         | em estado Ready | em estado Wait  | no processador\n");
    printf("=========+=================+=================+============\n");

    int total_execution_time = 0;
    int total_ready_wait_time = 0;
    int min_execution_time = processes[0].total_time;
    int max_execution_time = processes[0].total_time;

    for (int i = 0; i < process_count; i++)
    {
        printf("%d       | %d               | %d               | %d\n", processes[i].pid, processes[i].ready_time,
               processes[i].wait_time, processes[i].total_time);

        total_execution_time += processes[i].total_time;
        total_ready_wait_time += processes[i].ready_time + processes[i].wait_time;

        if (processes[i].total_time < min_execution_time)
        {
            min_execution_time = processes[i].total_time;
        }

        if (processes[i].total_time > max_execution_time)
        {
            max_execution_time = processes[i].total_time;
        }
    }

    float avg_execution_time = (float)total_execution_time / process_count;
    float avg_ready_wait_time = (float)total_ready_wait_time / process_count;

    printf("=========+=================+=================+============\n");
    printf("Tempo total de simulação.: %d\n", clock);
    printf("Número de processos......: %d\n", process_count);
    printf("Menor tempo de execução..: %d\n", min_execution_time);
    printf("Maior tempo de execução..: %d\n", max_execution_time);
    printf("Tempo médio de execução..: %.2f\n", avg_execution_time);
    printf("Tempo médio em Ready/Wait: %.2f\n", avg_ready_wait_time);
}

int main(int argc, char **argv)
{
    char *input_filename = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-F") == 0)
        {
            scheduling_algorithm = 0;
        }
        else if (strcmp(argv[i], "-S") == 0)
        {
            scheduling_algorithm = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-' && strlen(argv[i + 1]) < 4)
            {
                aging_factor = atof(argv[++i]);
            }
        }
        else if (strcmp(argv[i], "-v") == 0)
        {
            verbose = true;
        }
        else
        {
            input_filename = argv[i];
        }
    }

    if (input_filename == NULL)
    {
        fprintf(stderr, "Uso: %s [-F | -S [a]] [-v] <inputfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    parse_input(input_filename);
    init_priority_queues();
    init_sjf_queue();

    while (1)
    {
        int lastPid = 0;
        int solicitouIO = 0;

        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].arrival_time == clock && processes[i].state == -1)
            {
                processes[i].state = 0; // Ready
                if (scheduling_algorithm == 0)
                {
                    enqueue(&priority_queues[processes[i].priority], &processes[i]);
                }
                else
                {
                    add_to_sjf_queue(&processes[i]);
                }
            }
        }

        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].state == 2 && IOTerm())
            {
                processes[i].state = 0; // Ready
                inserirNaListaDeTerminosIO(processes[i].pid);

                if (scheduling_algorithm == 0)
                {
                    enqueue(&priority_queues[0], &processes[i]); // Requisições de I/O retornam com prioridade 0
                }
                else
                {
                    add_to_sjf_queue(&processes[i]);
                }
            }
        }

        if (scheduling_algorithm == 0 && quantum_restante == 0)
        {
            current_process = choose_process_fmp();
        }
        else if (scheduling_algorithm == 1)
        {
            current_process = choose_process_sjf();
        }

        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].state == 2)
            {
                processes[i].wait_time++;
            }
            if (processes[i].state == 0)
            {
                processes[i].ready_time++;
            }
        }

        if (current_process != NULL)
        {
            current_process->remaining_time--;
            current_process->total_time++;
            if (scheduling_algorithm == 0)
            {
                quantum_restante--;
            };

            if (current_process->remaining_time == 0)
            {
                current_process->state = 3; // Finished
                quantum_restante = 0;
                ultimo_burst_time = current_process->burst_time * 1.0;
            }
            else if (IOReq() && current_process->state == 1)
            {
                solicitouIO = 1;
                current_process->state = 2; // Wait
                quantum_restante = 0;
            }
            else
            {
                if (scheduling_algorithm == 0 && current_process->state != 3 && current_process->state != 2 && quantum_restante == 0)
                {
                    if (current_process->priority != MAX_PRIORITY_LEVELS - 1)
                    {
                        current_process->priority++;
                    }
                    enqueue(&priority_queues[current_process->priority], current_process);
                }
                else if (scheduling_algorithm == 1 && current_process->state != 3 && current_process->state != 2)
                {
                    add_to_sjf_queue(current_process);
                }
            }
        }

        int all_finished = 1;
        for (int i = 0; i < process_count; i++)
        {
            if (processes[i].state != 3)
            {
                all_finished = 0;
                break;
            }
        }
        if (all_finished)
        {
            break;
        }

        if (verbose)
        {
            if (scheduling_algorithm == 0)
            {
                if (current_process != NULL)
                {
                    printf("%d:%d:%d:%d:", clock, current_process->pid, current_process->remaining_time,
                           solicitouIO);
                    printListaDeTerminosIO();

                    if (quantum_restante == 0 && current_process->remaining_time > 0 && solicitouIO == false)
                    {
                        printf("0\n");
                    }
                    else if (solicitouIO)
                    {
                        printf("2\n");
                    }
                    else if (current_process->remaining_time > 0)
                    {
                        printf("1\n");
                    }
                    else
                    {
                        printf("3\n");
                    };
                }
                else if (current_process == NULL)
                {
                    printf("%d:-1:-1:%d:", clock, solicitouIO);
                    printListaDeTerminosIO();
                    printf("-1\n");
                }
            }
            else if (scheduling_algorithm == 1)
            {
                int nextPID = get_next_pid();
                if (current_process != NULL && nextPID != -1)
                {
                    printf("%d:%d:%d:%d:", clock, current_process->pid, current_process->remaining_time,
                           solicitouIO);
                    printListaDeTerminosIO();

                    if (nextPID != current_process->pid && current_process->remaining_time > 0 && solicitouIO == false)
                    {
                        printf("0\n");
                    }
                    else if (solicitouIO)
                    {
                        printf("2\n");
                    }
                    else if (current_process->remaining_time > 0)
                    {
                        printf("1\n");
                    }
                    else
                    {
                        printf("3\n");
                    };
                }
                else if (current_process == NULL)
                {
                    printf("%d:-1:-1:%d:", clock, solicitouIO);
                    printListaDeTerminosIO();
                    printf("-1\n");
                }
            }
        }
        limparListaDeTerminosIO();
        clock++;
    }
    print_statistics();

    return 0;
}
