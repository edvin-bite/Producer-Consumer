#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while(0)
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

sem_t use_queue;

/* global value representing how many miliseconds you want to take to simulate 1 hour.
     eg.:= 1000L; means 1 second simulates about 1 hour making 1 second simulated in ~277000 ns
     
     mesurement is not guarenteed, and depend on system, test.c explains a part of the issue
     with tested system, sleep lasts for around 300,000 ns and 500,000 is the value used
     
     1800L for 1 hour translates to 500000L ns for 1 second
*/
const unsigned long base_h = 1800L;

struct gen {
    pthread_t thread_id;
    int value;
};

static void* queue_thread(void *q) {
    struct gen *q_info = q;
    // use_queue taken on initiation.
    int s_value = 0;
    int s; // error var
    // scale of how many minuets it takes for customer to show
    const int scale[] = {60,30,20,15,12,10,6,5,4,2}; 
    const int busy_hours[] = {0,0,0,0,1,3,3,2,1,2,7,9,8,4,3,3,2,5,6,3,2,1,1,1};
    const int group_size[] = {2,1,3,2,1,4,2,1,1,1,2,1,3,1,1}; // use with rand
    int index = 0; // hour of day
    int time = 0; // increment controller of index by min
    printf("consumer thread begins!\n");
    fflush(stdout);
    
    /* 
        loop: do while index is in range (only simulate one day)
    */
    while(index < 24) {
        // get a time to delay for throughout the loop in "min"
        int min_fq = scale[busy_hours[index]];
        // the maximum I will account for is someone simulating 1 hour taking 1 hour (3,600,000 ms)
        // times 1000 to get to microseconds but not overflow
        // get base 1 min for this thread
        // *1000 gets accuracy close without overflow | /60 to get to min | *scale to get delay
        unsigned long delay = ((base_h * 1000L) / 60) * min_fq; // ??type cast here?
        struct timespec delay_f = {(delay / 1000000),((delay % 1000000)*1000)};
        nanosleep(&delay_f,NULL);
        time += min_fq;
        //person(s) approaches and are queued
        q_info->value += group_size[rand() % 15]; // add customers to line
        
        s = sem_getvalue(&use_queue, &s_value);
        if(s != 0) handle_error_en(s, "sem_getvalue");
        // if semaphore is taken(0), free it as queue was added to
        if(s_value == 0) sem_post(&use_queue);
        // !when used, check if queue is empty and wait if true.
        
        // get the index for the nextloop
        index = time / 60; // basically turn the "minuet" counter into "hours"
    } 
    printf("\n\ndaylight ran out!\n\n");
    fflush(stdout);
    q_info->value = -1;
}


int main(){
    int s; //error variable
    /* 
    consumer works until the shared memory reads 0. 
    
    set up shared memory
    */
    const char *name = "table";  
    const int SIZE = 8;          
    
    int shm_fd;
    
    void *tbl_ptr;
    int *ctr;

    shm_fd = shm_open(name, O_RDWR, 0666);
    if(shm_fd == -1) handle_error_en(-1, "shm_open");
    
    ftruncate(shm_fd, SIZE);
    tbl_ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    ctr = tbl_ptr;
    // shared memory is 2 ints at ctr[0] and ctr[1]
    /**
    the shared memory is used in the wrong way, it seems passing control over to a int pointer to avoid the void pointer errors I recive when using the class example method was not a valid option. it seems the pointer I use is pointing to odd locations, is mixing with diffrent changes, is referenced in the wrong size, or is constantly cahnging location. 
    it also seemes that producer fails and ends early while consumer errors uncaught and never ends...
    **/
    
    /*
    semaphores are created for the shared memory and the integers.
    sem usage explained in producer
    */
    const char *sname = "tray";
    sem_t *mutex;
    mutex = sem_open(sname, O_CREAT, 0666, 1);
    const char *fname = "full";
    const char *ename = "empty";
    sem_t *full;
    sem_t *empty;
    full = sem_open(fname, O_CREAT, 0666, 2);
    empty = sem_open(ename, O_CREAT, 0666, 0);
    
    // local semaphores are global variables
    if(sem_init(&use_queue, 0, 1) == -1) handle_error("sem_init");
    sem_wait(&use_queue);
    /*
    starts by making an independent generation thread that fills a queue of "customers" that takes from the stand.
    when using the queue, the generation thread does not interupt the main thread unless adding to the queue, and the same vice versa
    
    main thread will start with waiting on producer to claim control, it does not release control until 
    */
    void *status;
    ssize_t stack_size;
    pthread_attr_t attr;
    //input for generators
    struct gen *queue;
    
    stack_size = 0x100000;
    
    s = pthread_attr_init(&attr);
    if(s != 0) handle_error_en(s, "pthread_attr_init");
    s = pthread_attr_setstacksize(&attr, stack_size);
    if(s != 0) handle_error_en(s, "pthread_attr_setstacksize");
    
    queue = calloc(1, sizeof(*queue));
    if(queue == NULL) handle_error("calloc");
    
    queue[0].value = 0;
    
    /*
    start the threads
    */
    s = pthread_create(&queue[0].thread_id, &attr, &queue_thread, &queue[0]);
    if(s != 0) handle_error_en(s, "pthread_create");
    
    /* destroy unneeded objects */
    s = pthread_attr_destroy(&attr);
    if(s != 0) handle_error_en(s, "thread_attr_destroy");
    
    /*
    ----
    gen thread never waits. one line max for wait useage.
    ----
    write a critical section, exit value that runs for the conditional
    */
    
    //!!!!!!!!!!!!!!!!!!!!!!!!!!
    printf("running consumer loop\n");
    fflush(stdout);
    int running = 1;
    while(running == 1) {
        /* in loop:
        check for stop conditions
        */
        // stop if the table has any 0
        if(ctr[0] == 0 || ctr[1] == 0) {
            // reads 0 and is not end of day, stock ran out, unhappy customers
            printf("\n\nthe stand closed! that leaves %d very unhappy people...\n\n",queue[0].value);
            fflush(stdout);
            running = 0;
            continue;
        }
        // stops if the daytime ran out
        if(queue[0].value == -1) {
            printf("\n\ndaytime ranout!\n\n");
            fflush(stdout);
            running = 0;
            // consumer closes execution, nothing to wait as this shutdown is sudden unlike producer
            sem_wait(mutex);
            ctr[0] = 0;
            ctr[1] = 0;
            sem_post(mutex);
            continue;
        }
        
        // make sure there is something in the queue to use
        sem_wait(&use_queue);
        // if someone is waiting in line^ then wait if empty
        sem_wait(empty);
        //grab mutex
        sem_wait(mutex);
        // ########### CRITICAL SECTION ###############
        // use empty value for index
        int tbl_index;
        s = sem_getvalue(empty, &tbl_index);
        if(s != 0) handle_error_en(s,"sem_getvalue");
        
        // consume
        printf("customer ate dog #%d! they are happy\n",ctr[tbl_index]);
        fflush(stdout);
        ctr[tbl_index] = -1;
        queue[0].value -= 1;
        
        // fix semaphores
        // post a full as new slot is opened
        sem_post(full);
        
        sem_post(mutex);
        //######### END OF CRITICAL SECTION ######
        
        // conditional for queue
        if(queue[0].value > 0) {
            sem_post(&use_queue);
        } // else it is locked until new arival
    }
    
    /*
    close all loose ends.
    */
    sem_close(mutex);
    sem_close(full);
    sem_close(empty);
    sem_unlink(sname);
    sem_unlink(fname);
    sem_unlink(ename);
    sem_destroy(&use_queue);
    
    s = pthread_cancel(queue[0].thread_id);
    if(s != 0) handle_error_en(s, "pthread_cancel");
    s = shm_unlink(name);
    if(s != 0) handle_error_en(s, "shm_unlink");

    s = pthread_join(queue[0].thread_id, &status);
    if(s != 0) handle_error_en(s, "pthread_join");
    if(status != PTHREAD_CANCELED) handle_error("pthread_cancel");
    printf("\nJoined all threads and unlinked shared memory.\n");
    fflush(stdout);
    
    free(queue);
    exit(EXIT_SUCCESS);
    
}
