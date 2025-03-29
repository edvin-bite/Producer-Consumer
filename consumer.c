#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main(){
    printf("Customer generation algorithim running...");
    const char* name = "table";
    const int SIZE = 4096;
    const char* msg0 = "one";
    const char* msg1 = "two";
    int shm_fd;
    void *ptr;
    
    shm_fd = shm_open(name, O_RDONLY, 0666);
    ptr = mmap(0, SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    
    printf("%s", (char *)ptr);
    
    shm_unlink(name);
    return 0;
}
