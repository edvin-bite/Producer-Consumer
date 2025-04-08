#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    /*
    
    this is a test program designed to test the limits of the system being used.
    
    277 ns is the minimum sleep time posible as input is mesured in ms, and is 1/1,000,000
    docs are based on used system which returned a range around 300 us
    
    the use of nanosleep() controls the schedualing so the OS changes what happens offten
    while testing nnumbers ranged from up to 400,000 ns, to 277,000 ns, once even 3,467,487 ns
    
    */
    struct timespec req = {0,9000000000};
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    nanosleep(&req,NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    
    printf("Requested sleep time: %ld ns \n", req.tv_nsec);
    printf("actual sleep time:    %ld ns \n", elapsed_ns);
    printf("                      %ld us \n", elapsed_ns / 1000);
    
   
    
}
