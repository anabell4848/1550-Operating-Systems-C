/*
Hanyu Xiong 
hax12@pitt.edu
cs 1550
project 2

gcc -m32 -o prodcons -I /u/OSLab/hax12/linux-2.6.23.1/include/ prodcons.c
*/

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h> 
#include <linux/unistd.h>

//semaphore structure
struct cs1550_sem {
	int value;
	struct Node *first; 	// Pointer to the first of the queue
	struct Node *last; 		// Pointer to the last of the queue
};

//wrapper functions
void up(struct cs1550_sem *sem) {
       syscall(__NR_cs1550_up, sem);
}
void down(struct cs1550_sem *sem) {
       syscall(__NR_cs1550_down, sem);
}

int main(int argc, char *argv[]) {
	if(argc < 4 || argc > 4) { // If the number of arguments is not 4
		printf("Please enter the number of consumers, producers, and the size of the buffer.\n");
		exit(0);
	}
	
	int numOfProd, numOfCons, bufSize, status; 
	
	//set arguments to variables
	numOfProd = atoi(argv[1]);
	numOfCons = atoi(argv[2]);
	bufSize = atoi(argv[3]);
	
	// Print some info for the user
	printf("Producers: %d\n", numOfProd);
	printf("Consumers: %d\n", numOfCons);
	printf("Buffer Size: %d\n", bufSize);
	printf("Press enter to continue...\n");
	
	struct cs1550_sem *empty;
	struct cs1550_sem *full;
	struct cs1550_sem *mutex;
	
	//allocate the memory for the shared semaphores
	void* ptr1 = mmap(NULL, sizeof(struct cs1550_sem)*3, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	empty = (struct cs1550_sem*)ptr1; 		// Sets the first chunk of shared memory to the empty semaphore
	full = 1+(struct cs1550_sem*)ptr1; 		// Sets the second chunk of shared memory to the full semaphore
	mutex = 2+(struct cs1550_sem*)ptr1; 	// Sets the third chunk of shared memory to the mutex

	//initialize data in semaphores
	empty->first = NULL;
	mutex->first = NULL;
	full->first = NULL;
	empty->value = bufSize;
	mutex->value = 1;
	full->value = 0;
	empty->last = NULL;
	mutex->last = NULL;
	full->last = NULL;
	
	//allocate the memory for the shared buffer
	void* ptr2 = mmap(NULL, (bufSize + 1)*sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	int *size_ptr, *pro_ptr, *con_ptr, *buf_ptr, i; // Pointers into the shared info/buffer memory
	
	size_ptr = (int*)ptr2; 			// Holds the buffer size (aka the number of items, n)
	pro_ptr = 1+(int*)ptr2; 		// Holds the position for the producer(s)
	con_ptr = 2+(int*)ptr2; 		// Holds the position for the consumer(s)
	buf_ptr = 3+(int*)ptr2; 		// Points to the buffer. Used as an array
	
	*size_ptr = bufSize; 	// Set n to the buffer size
	*pro_ptr = 0; 			// Set the producer(s) position to zero
	*con_ptr = 0; 			// Set the consumer(s) position to zero
	
	for(i = 0; i < bufSize; i++) { // Fill the buffer with zeros for potential debug
		buf_ptr[i] = 0;
	}
	
	getchar(); // Wait for the user to press enter before continuing
	int prod_item, con_item;
	for(i = 0; i < numOfProd; i++) { 	//loop to create all producers
		if(fork() == 0) { 				//fork and check if it is the child (producer) process
			while(1) {
				down(empty);
				down(mutex); // Lock the mutex
				prod_item = *pro_ptr;
				buf_ptr[*pro_ptr] = prod_item;
				*pro_ptr = (*pro_ptr+1) % *size_ptr;
				printf("Producer %c Produced: %d\n", i+65, prod_item); // Turn i into a letter by adding 65
				fprintf(stderr,"Producer %c Produced: %d\n", i+65, prod_item); 
				up(mutex); // Unlock the mutex
				up(full);
			}
		}
	}
	
	for(i = 0; i < numOfCons; i++) { 	//loop to create all of the desired consumers
		if(fork() == 0) { 				//fork and check if it is the child (consumer) process
			while(1) {
				down(full);
				down(mutex); // Lock the mutex
				con_item = buf_ptr[*con_ptr]; 
				*con_ptr = (*con_ptr+1) % *size_ptr;
				printf("Customer %c Consumed: %d\n", i+65, con_item); // Turn into letter by adding 65
				fprintf(stderr,"Customer %c Consumed: %d\n", i+65, con_item); 
				up(mutex); // Unlock the mutex
				up(empty);
			}
		}
	}
	
	wait(&status); // Wait until all processes complete
	return 0; // Finished successfully
}
