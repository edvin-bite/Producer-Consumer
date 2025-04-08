#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

// error handling and used example codes from chapter 3 lesson slides and the linux manual pages (man7.org/linux/man_pages/...)
#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while(0)
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* 
  
  Producer contains 2 threads with one shared global value "stock" that is the number of available hotdogs.
  
  the producing thread will add 1 to "stock" at a regular interval. duplicates of this thread may be made. this will be a function ran through a thread.
  
  the main thread will set up the environment for the combined environments and pull from the "stock" value 2 at a time and insert it into the shared memory "table"
  
*/

struct steamer {
    //pthread_t thread_id;
    //int thread_num;
    //int interval;

    int inventory;
};

// a global variable
//sem_t ready;


/******* due to not knowing schedualing at the time, this thread is prioritized and finished before the rest of the processes are schedualed, timing with sleeps does not work with preemptive schedualers ********************************************************************

const unsigned long base_h = 1800L;

static void* chef_thread(void* arg){
    // each thread will have a thread_id associated with it, and has an argument that contains that ID and the interval at which hotdogs are produced.
    struct steamer *stand_info = arg;
    
    int s; //error variable
    
    int ready_released = 0;
    // tests, declares the thread creation
    printf("Hotdog steamer %d running! (%p) \n  a hotdog is produced once every %d seconds.\n", stand_info->thread_num, (void *) &stand_info, stand_info->interval);
    fflush(stdout);
    
    // a loop held containing a sleep function which acts as a cancellation point for closing the thread using thread_close()
    while(stand_info->inventory > 0) {
        
        unsigned long delay = ((base_h * 1000L) / 60) * stand_info->interval; // ??type cast here?
        struct timespec delay_f = {(delay / 1000000)*60,((delay % 1000000)*1000)*60};
        nanosleep(&delay_f,NULL);
        
        ++stand_info->stock;
        //printf("\nHotdog Steamer %d finished coocking a dog! stock: %d \n", stand_info->thread_num, stand_info->stock);
        fflush(stdout);
        --stand_info->inventory;
        s = sem_getvalue(&ready, &ready_released);
        if(s != 0) handle_error_en(s, "sem_getvalue");
        if(ready_released == 0) sem_post(&ready);
    }
}
***************************************************************************************/
int main(){
    const int INV_SIZE = 50;
    //const int PRODUCTION_RATE = 5;
    int dog_id = 0;//=1
    
    /* preparing the environment with thread creation and shared memory preperations. */
    // Table
    const char *name = "table";  // name of shared memory %u
    const int SIZE = 8;          // size of shared memory, 2 words for 2 int values for now.
    
    int shm_fd;
    
    void *tbl_ptr; // points to the table address
    int *ctr;

    
    /* creates/opens the shared memory object for between processes. */
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1) handle_error_en(-1, "shm_open");
    
    ftruncate(shm_fd, SIZE);
    
    /* uses mmap for the sake of the consumer which could take one value at a time. */
    tbl_ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    // producer pointer will stay on first, and fill both when there is no item, and return to first, producer stays on last
    ctr = tbl_ptr;
    ctr[0] = -1;
    ctr[1] = -1;
    // Thread
    /* this thread is removed due to schedualling issues causing it to complete too fast. */
    int s;
    //void *status;
    //ssize_t stack_size;
    //pthread_attr_t attr;
    struct steamer *stmr;
    
    // Semaphore
    /*
    usage:
        the mutex is the semaphore neccesary for accessing the table, it's usage is purely that it must be called to use the table
          - on that note, to reduce delays, the mutex is compeated for and up to the control semaphores to prevent locks.
    */
    const char *sname = "tray";
    sem_t *mutex;
    mutex = sem_open(sname, O_CREAT, 0666, 1);
    /*
        the full and empty semaphores are the control semaphores.
        both consumer and producer change them after interacting with the table, but before control is released.
        before the mutex wait, the producer and consumer must check their condition, then instantly claim the mutex then release the semaphore of their condition.
        eg:
        wait(full);  // checks producer condition and waits until the table is not full
        wait(mutex); // claims control
        post(full);  // relases wait to keep semaphore value true, 
        ...
        
        full and empty are semaphores with a 2 value, and are changed at the end of the producer and consumer (which trigger one at a time) as: 
        [item, item] = full{0} empty{2}
        [item, none] = full{1} empty{1}
        [none, none] = full{2} empty{0}
        
        idealy this setup prevents adding or removing more items than the table can hold by having the adder/remover wait on a semaphore with value 0 rather than the mutex when no items can be added/removed.
    */
    const char *fname = "full";
    const char *ename = "empty";
    sem_t *full;
    sem_t *empty;
    full = sem_open(fname, O_CREAT, 0666, 2);
    empty = sem_open(ename, O_CREAT, 0666, 0);
    // no need to call sem_init on named semaphores
    
    
    stmr->inventory = INV_SIZE;
    
    /* set up table
       rules:
         2 int words
         negative(-) means no item in either slot
         positive is an int that is the # of the hotdog produced.
         0 means that no more is arriving
    */
    
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /* the producer leaves scope if table is full
     * 
     * when adding to table(Crit), set up, wait/post full/empty semaphores, insert item, leave mutex(Crit)
    while trigger not tripped(by 0 in table)
    
    sem ready locks if no stock.
    */
    int running = 1;
    printf("Running producer loop.");
    fflush(stdout);
    while(running == 1) {
        /*
            start of the loops has the end of program exceptions
            if the values are already 0, set by consumer, then also end
        */
        if(stmr->inventory == 0) {
            // replace -1 with 0, both -1 if ctr[0] is -1
            if(ctr[0] == -1) {
                sem_wait(mutex);
                ctr[1] = 0;
                ctr[0] = 0;
                sem_post(mutex);
                running = 0;
            }
            continue;
        }
        if(ctr[0] == 0) {
            //end
            running = 0;
            continue;
        }
    


        //done before claiming critical section mutex.
        ++dog_id; // this is now the next item's value
        // now "when the table is complete, produceer will wait."
        s = sem_wait(full); // init to 2, decriment stays as one table resource is taken
        if(s != 0) handle_error_en(s, "sem_wait full");
	
        s = sem_wait(mutex);
        if(s != 0) handle_error_en(s, "sem_wait mutex");
        // ############### Critical Section ##########################
        
        // get index to use; full is either 1 or 0, but reversed to empty, fill left to right
        // empty value is 0 or 1, use as index
        int tbl_index;
        s = sem_getvalue(empty, &tbl_index);
        if(s != 0) handle_error_en(s,"sem_getvalue");
        
        // used another pointer to avoid typecasting
        ctr[tbl_index] = dog_id;
        stmr->inventory -= 1;
        
        sem_post(empty); // init to 0, incrememnt to signal consumer item is available.
        
        sem_post(mutex);
        // ############## End Critical Section #############################
        
        
    }
    // clean up space, table is set to signal consumer, 
    printf("selling %d Hotdogs!", INV_SIZE);
    fflush(stdout);
    printf("%d hotdogs unsold!", stmr->inventory);
    fflush(stdout);
    
    /* remove the threads and shared memory + mutex */
    sem_close(mutex);
    sem_close(full);
    sem_close(empty);
    sem_unlink(sname);
    sem_unlink(fname);
    sem_unlink(ename);

    
    s = shm_unlink(name);
    if(s != 0) handle_error_en(s, "shm_unlink");
    
    
    printf("exiting producer");
    fflush(stdout);
    exit(EXIT_SUCCESS);
    
}
