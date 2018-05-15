#define _GNU_SOURCE
#define _USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

const int max_name_length = 32;
const int max_numP = 1E1;
const int time_quantum = 5E2;
int numOfP_now = 0;
int numP_finish = 0;
#define FIFO 0
#define RR 1
#define SJF 2
#define PSJF 3
typedef struct process
{
    char *name;
    int readyT;
    int execT;
    int ID;
} Process;
int compare_Process(const void *a, const void *b)
{
    Process *P1 = (Process *)a;
    Process *P2 = (Process *)b;
    if(P1->readyT < P2->readyT)
        return -1;
    if(P1->readyT > P2->readyT)
        return 1;
    if(P1->ID < P2->ID)
        return -1;
    if(P1->ID > P2->ID)
        return 1;
    return 0;
}
void insertP(Process **waiting_list, int policy, Process *P)
{
    numOfP_now++; 
    if(policy == FIFO || policy == RR) 
        waiting_list[numOfP_now - 1] = P;
    if(policy == SJF || policy == PSJF) 
    {
        waiting_list[numOfP_now - 1] = P;
        for(int i = numOfP_now - 1; i > 0 && waiting_list[i]->execT < waiting_list[i - 1]->execT; i--)
        {
            Process *temp = waiting_list[i];
            waiting_list[i] = waiting_list[i - 1];
            waiting_list[i - 1] = temp;
        }
    }
}
int execP(Process **waiting_list, int policy)
{ 
    if(policy == FIFO || policy == SJF)
    {
        int execute_length = waiting_list[0]->execT;
        waiting_list[0]->execT = 0;
        waiting_list[0] = NULL;
        numOfP_now--;
        numP_finish++;
        for(int i = 1; i <= numOfP_now; i++)
        {
            Process *temp = waiting_list[i];
            waiting_list[i] = waiting_list[i - 1];
            waiting_list[i - 1] = temp;
        }
        return execute_length; 
    }
    if(policy == RR)
    {
        int execute_length;
        if(waiting_list[0]->execT > time_quantum)
        {
            execute_length = time_quantum;
            waiting_list[0]->execT -= time_quantum;
        }
        else
        {
            execute_length = waiting_list[0]->execT;
            waiting_list[0]->execT = 0;
            waiting_list[0] = NULL; 
        }
        for(int i = 1; i < numOfP_now; i++)
        {
            Process *temp = waiting_list[i];
            waiting_list[i] = waiting_list[i - 1];
            waiting_list[i - 1] = temp;
        }
        if(waiting_list[numOfP_now - 1] == NULL)
        {
            numOfP_now--;
            numP_finish++;
        }
        return execute_length;
    }
    if(policy == PSJF) 
    {
        waiting_list[0]->execT--;
        if(waiting_list[0]->execT == 0)
        {
            waiting_list[0] = NULL;
            numOfP_now--;
            numP_finish++;
            for(int i = 1; i <= numOfP_now; i++)
            {
                Process *temp = waiting_list[i];
                waiting_list[i] = waiting_list[i - 1];
                waiting_list[i - 1] = temp;
            }
        }
        return 1;
    }
}
int main()
{
    Process P[max_numP];
    char policy_name[5];
    scanf("%s", policy_name);
    int numP;
    scanf("%d", &numP);
    for(int i = 0; i < numP; i++)
    {
        char *process_name = malloc(max_name_length * sizeof(char));
        int ready_time;
        int execution_time;
        scanf("%s %d %d", process_name, &ready_time, &execution_time);
        P[i].name = process_name;
        P[i].readyT = ready_time;
        P[i].execT = execution_time;
        P[i].ID = i;
    }
    qsort(P, numP, sizeof(Process), compare_Process);
    int policy = 0;
    char policy_list[4][5] = {"FIFO", "RR", "SJF", "PSJF"};
    for(int i = 0; i < 4; i++)
        if(strcmp(policy_name, policy_list[i]) == 0)
            policy = i;
    Process *waiting_list[max_numP];
    for(int i = 0; i < max_numP; i++)
        waiting_list[i] = NULL;
    
    //set CPU
    
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(0, &cpu_mask);
    
    if(sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) != 0){
        perror("sched_setaffinity error 1");
        exit(EXIT_FAILURE);
    }
    
    const int priorityH = 80;
    const int priorityL = 10;
    struct sched_param param;
    param.sched_priority = 50;
    pid_t pidP = getpid();
    if(sched_setscheduler(pidP, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler error");
        exit(EXIT_FAILURE);  
    }
    
    int time_count = 0;
    int fork_count = 0;
    Process *exec_process = NULL;
    Process *exec_process_last = NULL;
    int numP_finish_last = 0;
    int execute_length = 0;
    pid_t pids[max_numP];
    while(!(numP_finish == numP && execute_length == 0))
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long starting_time_s = tv.tv_sec;
        long long starting_time_ns = tv.tv_usec;
        while(P[fork_count].readyT <= time_count && fork_count < numP)
        {
            pid_t pid = fork();
            
            if(pid < 0)   
                printf("error");   
            else if(pid == 0) {
                char exec_time[10];
                sprintf(exec_time, "%d", P[fork_count].execT);
                char starting_time_s_string[20]; 
                char starting_time_ns_string[20];
                sprintf(starting_time_s_string, "%lld", starting_time_s);
                sprintf(starting_time_ns_string, "%lld", starting_time_ns);
                if(execlp("./process", "process", P[fork_count].name, exec_time, starting_time_s_string, starting_time_ns_string, (char *)NULL) < 0){
                    perror("execlp error");
                    exit(EXIT_FAILURE);
                }
            }
            cpu_set_t cpu_mask;
            CPU_ZERO(&cpu_mask);
            CPU_SET(1, &cpu_mask);
    
            if(sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_mask) != 0){
                perror("sched_setaffinity error 2");
                exit(EXIT_FAILURE);
            }

            pids[P[fork_count].ID] = pid;
            insertP(waiting_list, policy, (P + fork_count));
            fork_count++;
        }
        if(waiting_list[0] == NULL && execute_length == 0)
        {
            time_count++;
            volatile unsigned long i;
            for(i=0;i<1000000UL;i++); 
            exec_process_last = NULL;
        }
        else
        {
            if(execute_length == 0)
            {
                exec_process = waiting_list[0];
                execute_length = execP(waiting_list, policy);
                if(exec_process_last == NULL || exec_process_last->execT == 0)
                {
                    pid_t pid = pids[exec_process->ID];
                    param.sched_priority = priorityH;
                    if(sched_setscheduler(pid, SCHED_RR, &param) != 0) {
                        perror("sched_setscheduler error");
                        exit(EXIT_FAILURE);
                    }
        
    
                }
                else
        {
                    pid_t pid_last = pids[exec_process_last->ID];
                    pid_t pid = pids[exec_process->ID];
                    param.sched_priority = priorityL;
                    if(sched_setscheduler(pid_last, SCHED_RR, &param) != 0) {
                        perror("sched_setscheduler error");
                        exit(EXIT_FAILURE);  
                    }

                    param.sched_priority = priorityH;
                    if(sched_setscheduler(pid, SCHED_FIFO, &param) != 0) {
                        perror("sched_setscheduler error");
                        exit(EXIT_FAILURE);  
                    }
                }   
            }

            execute_length--;
            time_count++;
            volatile unsigned long i;
            for(i=0;i<1000000UL;i++); 
            
            if(execute_length == 0)
            {
                if(numP_finish == numP_finish_last + 1)
                {
                    int status;
                    if(waitpid(pids[exec_process->ID], &status, 0) == -1)
                    {
                        perror("waitpid error");
                        exit(EXIT_FAILURE);
                    }
                }

                exec_process_last = exec_process;
                numP_finish_last = numP_finish;
            }   
        }    
    }

    for(int i = 0; i < numP; i++){
        printf("%s ", P[i].name);
        printf("%d\n", pids[P[i].ID]);
    }

}
