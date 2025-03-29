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

struct t_args {
    pthread_t thread_id;
    int thread_num;
    int interval;
};

// a global variable
int stock;


static void* chef_thread(void* arg){
    // each thread will have a thread_id associated with it, and has an argument that contains that ID and the interval at which hotdogs are produced.
    struct t_args *thread_args = arg;
    
    // tests, declares the thread creation
    printf("Hotdog steamer %d running! (%p) \n  a hotdog is produced once every %d seconds.\n", thread_args->thread_num, (void *) &thread_args, thread_args->interval);
    fflush(stdout);
    
    // a loop held containing a sleep function which acts as a cancellation point for closing the thread using thread_close()
    while(1){
        sleep(thread_args->interval);
        ++stock;
        printf("\nHotdog Steamer %d finished coocking a dog! stock: %d \n", thread_args->thread_num, stock);
        fflush(stdout);
    }
}

int main(){
    /* preparing the environment with thread creation and shared memory preperations. */
    // Table
    const char *name = "table";  // name of shared memory %u
    const int SIZE = 8;          // size of shared memory, 2 words for 2 int values for now.
    
    int shm_fd;
    
    void *tbl_ptr;
    
    /* creates/opens the shared memory object for between processes. */
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1) handle_error_en(-1, "shm_open");
    
    ftruncate(shm_fd, SIZE);
    
    /* uses mmap for the sake of the consumer which could take one value at a time. */
    tbl_ptr = mmap(0, SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    // Thread
    /* this thread does not return anything, may only be one thread, and does not need the default thread allocations */
    //size_t num_threads;
    int s;
    void *status;
    ssize_t stack_size;
    pthread_attr_t attr;
    struct t_args *tarr;
    
    // Semaphore
    const char *sname = "tray";
    int VALUE = 1; // is a mutex, so it is initialized to 1 as available.
    sem_t *mutex;
    
    mutex = sem_open(sname, O_CREATE | O_RDWR, 0666, VALUE);
    // ready to call wait.
    
    /* set attributes; for termination: call cancel, then join expecting status PTHREAD_CANCELED in final check*/
    stack_size = 0x100000; // test to lower.
    
    //num_threads = [set if used, may not implement.]
    /* set the pthread attributes. */
    s = pthread_attr_init(&attr);
    if(s != 0) handle_error_en(s, "pthread_attr_init");
    s = pthread_attr_setstacksize(&attr, stack_size);
    if(s != 0) handle_error_en(s, "pthread_attr_setstacksize");
    
    /* Allocating memory for pthread_create; will use calloc because of my reference and possibly adding more threads. */   
    tarr = calloc(/*num_threads*/1, sizeof(*tarr));
    if(tarr == NULL) handle_error("calloc");
    
    /* Creating the thread(s). includes filling thread parameters. */
    tarr[0].thread_num = 1;
    tarr[0].interval = 5;
    s = pthread_create(&tarr[0].thread_id, &attr, &chef_thread, &tarr[0]);
    if(s != 0) handle_error_en(s, "pthread_create");
    
    /* destroy unneeded objects */
    s = pthread_attr_destroy(&attr);
    if(s != 0) handle_error_en(s, "thread_attr_destroy");
    
    
    /* at this point, the production has started and the main code is below. */
    
    for(int i = 0; i < 1440; i++) {
        //simulates 1 day
    }
    
    /* remove the threads and shared memory + mutex */
    sem_close(mutex);
    sem_unlink(sname);
    
    s = pthread_cancel(tarr[0].thread_id);
    if(s != 0) handle_error_en(s, "pthread_cancel");
    s = shm_unlink(name);
    if(s != 0) handle_error_en(s, "shm_unlink");
    nanosleep(100); // allows thread to cancel
    s = pthread_join(tarr[0].thread_id, &status);
    if(s != 0) handle_error_en(s, "pthread_join");
    if(status != PTHREAD_CANCELED) handle_error("pthread_cancel");
    printf("\nJoined all threads and unlinked shared memory.\n");
    
    
    free(tarr);
    exit(EXIT_SUCCESS);
    
}
