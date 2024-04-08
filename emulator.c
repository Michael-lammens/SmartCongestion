
/*
 * Michael lammens
 * XLW945
 * 11335630
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <list.h>
#include <protocol.h>
#define BUFFER_SIZE 2048
#define B_SEND_TIME 1
#define C_SEND_TIME 1
#define QUEUE_SIZE 500
//#define BPS 100
#define PSIZE 300 

double first_start;
int BPS;
int token_bucket;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dequeue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t token_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t fwd_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct {
    	LIST *data;
    	int cur_size;
    	int max_size;
}Queue;
/*
typedef struct {
	int pack_id;
	int source_port;
    	int num_bytes;
	char start_time[30];
}Segment;
*/
typedef struct {
    	int throughput;
	int token_bucket;
    	double min_rtt;
    	double max_additional_rtt;
    	int em_port, serv_port;
	Queue *inf_queue;
}Emulator;

typedef struct {
    Emulator* emulator;
    Segment* dq_packet;
    pthread_cond_t* cond;
    pthread_mutex_t* mutex;
    int is_done; // Flag to indicate if forward_after_prop_routine has finished
} ForwardArgs;

void init_queue(Emulator *eminem) {
    eminem->inf_queue = malloc(sizeof(Queue));
    if (eminem->inf_queue == NULL || eminem->inf_queue == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    eminem->inf_queue->cur_size = 0;
    eminem->inf_queue->max_size = 1000;
    eminem->inf_queue->data = ListCreate();
   
    if (eminem->inf_queue->data == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
}
/*Create restriction methods on list for queue*/





/*Function to check queue is empty
 *returns 1 if empty
  0 if not empty
 * */
int isEmpty(Queue* queue){	
	return (ListCount(queue->data)==0);
}
// -1 if failed
// 0 else
// Mallocs a clone of seg. Adds clone to List. Frees original
int enqueue(Queue* queue, Segment* seg){
	/* heap our new segments memory so receiver_a doesnt overwrite it. 
	 * Then queue.
	*/
	int prepend_status;
	Segment* temp = (Segment*)malloc(sizeof(Segment));
        temp->pack_id = seg->pack_id;
	temp->source_port = seg->source_port;
	temp->ws = seg->ws;
	temp->num_bytes = seg->num_bytes;
	strcpy(temp->start_time,seg->start_time);
	
//	printf("Timer Val=%s\n", temp->start_time);

	prepend_status = ListPrepend(queue->data,temp);
	if(prepend_status==-1){
		printf("\nPrepend failed\n");
		exit(-1);
	}
//	free(seg->start_time);
	free(seg);
	queue->cur_size+=1;
	return 0;
}

void* dequeue(Queue* queue){
	Segment* deleted;
	if(isEmpty(queue)){
		return NULL;
	}
	deleted = ListTrim(queue->data);/*Freed in caller*/
	queue->cur_size-=1;
	return deleted;
}
void* queue_head(Queue* queue){
	if(isEmpty(queue)){
                return NULL;
        }
	return ListFirst(queue->data);
}




/*
int calc_total_delay(Emulator * emulator){
	srand(time(NULL));
	int additional = rand() % (emulator->max_additional_rtt + 1);
 	return emulator->min_rtt + additional;
}
*/

// Mallocs memory. Returns pointer to it. 
// Callers responsibility to free
Segment* receive_packet_deserialize(int socket) {
    	char data[512];
    	struct sockaddr_in client_addr;
    	int bytes_received;
    	Segment* temp;
    	socklen_t addr_len = sizeof(client_addr);
    	memset(data, '\0', sizeof(data));

    	bytes_received = recvfrom(socket, data, sizeof(data), 0,
                              (struct sockaddr *)&client_addr,
                              &addr_len);
    	if (bytes_received == -1) {
    	    perror("recvfrom");
    	    exit(EXIT_FAILURE);
    	}
	temp = (Segment*)malloc(sizeof(Segment)); // Allocate memory for temp
	if (temp == NULL) {
	        perror("Memory allocation failed");
	        exit(EXIT_FAILURE);
	}
	deserialize(data, temp);
	//current_unix_timestamp()
	strcpy(temp->start_time, current_unix_timestamp());
	
	return temp;
}


void increase_BPS(){
	 	char* curTimeStamp_str = current_unix_timestamp();
                double curTimeStamp =  strtod(curTimeStamp_str, NULL);
		if((int)(curTimeStamp - first_start) % 5 == 0){
			BPS = BPS+50;
			token_bucket=token_bucket+50;
		}
	printf("\nBPS:%d, Time: %s", BPS, curTimeStamp_str);
}

void *forward_after_prop_routine(void *arg){
	/*MUTEXED AREA*/
	ForwardArgs* delay_args = (ForwardArgs*)arg;
	Emulator* emulator = delay_args->emulator;	

		/*DEQUEUE*/
	pthread_mutex_lock(&queue_mutex);
	Segment* dq_packet = dequeue(emulator->inf_queue);
	pthread_mutex_unlock(&queue_mutex);

	int pack_size = dq_packet->num_bytes;
		/*WAIT FOR TOKENS*/
	while(token_bucket - pack_size < 0){
		;/*Busy wait until we have enough tokens*/
	}
		/*REMOVE x TOKENS FROM BUCKET*/
        pthread_mutex_lock(&token_mutex);
	token_bucket = token_bucket - pack_size;
        pthread_mutex_unlock(&token_mutex);
	
	delay_args->is_done = 1;
	pthread_cond_signal(delay_args->cond);

	/*MUTEXED AREA STOPS*/

	/*DATARATE DELAY*/
	double delay = (double)pack_size*10 / BPS; //calc_total_delay(emulator);
	if(nano_second_sleep(delay)==0){
		;
	}else{perror("nanosleep");}
        /*RELEASE TOKENS AFTER DATA DELAY*/
	pthread_mutex_lock(&token_mutex);
	token_bucket = token_bucket + pack_size;
	pthread_mutex_unlock(&token_mutex);
	
	/*MIN RTT(Rm) DELAY*/
	nano_second_sleep(emulator->min_rtt);/*Non congestive delay min rtt*/
        
	
	/*ADDITIONAL DELAY 0-D*/
	double d_delay = generate_random_value(emulator->max_additional_rtt, (double)dq_packet->pack_id);
	nano_second_sleep(d_delay);
	/*Send to Server*/
        int sockfd_b;
        struct sockaddr_in dest_addr_b;
        if ((sockfd_b = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
        }
        dest_addr_b.sin_family = AF_INET;
        dest_addr_b.sin_addr.s_addr = inet_addr("127.0.0.1");
        dest_addr_b.sin_port = htons(emulator->serv_port); 
        fflush(stdout);
	char buffer_out[512] = {'\0'};      
	int bytes = serialize((Segment*)dq_packet, buffer_out);	
	sendto(sockfd_b, buffer_out,bytes+1, 0,
                        (struct sockaddr *)&dest_addr_b, sizeof(dest_addr_b));
	/*FREE MEMORY AND CLOSE*/
	free(dq_packet);
	free(delay_args);
        close(sockfd_b);
        pthread_exit(NULL);
}


// Function that creates threa(forward_after_prop_routine)
// args(Emulator, Packet most recently Dequeued)
//
// Creates thread and finishes
void launch_forward_after_prop_thread(ForwardArgs* args) {
	pthread_t forward_thread_pid;
    	int result;	
    	result = pthread_create(&forward_thread_pid, NULL, forward_after_prop_routine, args);
	if (result != 0) {
        	fprintf(stderr, "Error creating sleeper thread: %s\n", strerror(result));
        	exit(EXIT_FAILURE); // Exit the program in case of error
    	}
    	// Detach the sleeper thread
    	result = pthread_detach(forward_thread_pid);
    	if (result != 0) {
        	fprintf(stderr, "Error detaching sleeper thread: %s\n", strerror(result));
   	}
}


/* Always running. If theres a packet in queue, wait the relative time PackSize/BPS
 * Before dequeue
 *
 * After dequeue -> send packet to thread for forwarding
 * */
void *dequeue_background_routine(void *arg){
        Emulator *emulator = (Emulator *)arg;
	int pack_size;

	pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond_fwd = PTHREAD_COND_INITIALIZER;
	
	while(1){
		if(!isEmpty(emulator->inf_queue)){
			ForwardArgs *args = malloc(sizeof(ForwardArgs));
	                if (args == NULL) {
	                	// Handle memory allocation failure
	                	perror("Memory allocation failed");
	                	exit(EXIT_FAILURE);
	                }
	    		args->emulator = emulator;
	    		args->cond = &cond_fwd;
	    		args->mutex = &cond_mutex;	   
			args->is_done = 0; // Initialize the flag
	
			/* WAIT HERE UNTIL SIGNALLED */
			pthread_mutex_lock(&cond_mutex);
			launch_forward_after_prop_thread(args);
			while(!args->is_done){
            			pthread_cond_wait(&cond_fwd, &cond_mutex);
			}
			pthread_mutex_unlock(&cond_mutex);
		}
	}
	return NULL;
}
// Function to launch the dequeue loop
// Creates it and returns
void launch_dequeue_background_thread(Emulator* emulator) {
    pthread_t dequeue_thread_pid;
    int result;

    result = pthread_create(&dequeue_thread_pid, NULL, dequeue_background_routine, emulator);
    if (result != 0) {
        fprintf(stderr, "Error creating sleeper thread: %s\n", strerror(result));
        exit(EXIT_FAILURE); // Exit the program in case of error
    }

    result = pthread_detach(dequeue_thread_pid);
    if (result != 0) {
        fprintf(stderr, "Error detaching sleeper thread: %s\n", strerror(result));
        // Handle the error (e.g., log it), but continue execution
    }
}


/*
 * Waits to receive packet from B or C
 * if packet from B:
 * 	add to queue C
 * 	launch thread that sleeps for d then sends packet to C + dequeue from C
 * Vice Versa
 * */
void *receive_and_process(void *arg) {

    	Emulator *emulator = (Emulator *)arg;
    	int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    	if (sock_fd == -1) {
        	perror("Socket create failed");
    	}
    	int enqueue_status;
    	struct sockaddr_in server_addr;
    	
	server_addr.sin_family = AF_INET;
    	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	server_addr.sin_port = htons(emulator->em_port);
    	if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) 
		    == -1) {
        	perror("Bind failed");
        	close(sock_fd);
		exit(-1);
    	}
 
	/*Every Emulator has a dequeue process running concurrently*/	
        launch_dequeue_background_thread(emulator);/*TODO See if we will have this thread manage individual 
						     queues per client port? one queue total? have router determine server?*/
	
	Segment* packet_from_client;
    	while (1) {
		packet_from_client = receive_packet_deserialize(sock_fd); /*Returns Malloc'd Segment*/
		pthread_mutex_lock(&queue_mutex);
		enqueue_status = enqueue(emulator->inf_queue, packet_from_client);/*Clones packet. Frees packet_from_client*/
		if(enqueue_status != 0){
                        printf("\nEnqueue Failed\n");
                        exit(EXIT_FAILURE);
                }
		pthread_mutex_unlock(&queue_mutex);	
	}
    return NULL;
}



/*  Emulator Initialization. Make this a module.  */
void initialize_emulator_queue(Emulator *eminem) {
	/* Queue object pointer */
    	eminem->inf_queue = malloc(sizeof(Queue));
    	if (eminem->inf_queue == NULL) {
        	perror("Memory allocation failed");
        	exit(EXIT_FAILURE);
    	}
    	eminem->inf_queue->cur_size = 0;
	eminem->inf_queue->max_size = 500;
	eminem->inf_queue->data = ListCreate();
	/*Queue struct LIST Object Memory*/
    	if (eminem->inf_queue->data == NULL) {
        	perror("Memory allocation failed");
       		exit(EXIT_FAILURE);
    	}
}
void initialize_emulator(Emulator* emulator, char* argv[]) {
    emulator->throughput = atoi(argv[1]);
   // emulator->min_rtt = atoi(argv[2]);
   // emulator->max_additional_rtt = atoi(argv[3]);
    emulator->em_port = atoi(argv[4]);
    emulator->serv_port = atoi(argv[5]);
    /*Initialize queue.*/
    init_queue(emulator);
}

// Launch emulator thread with emulator data provided
int launch_emulator_blocking(Emulator* emulator_data) {
    	pthread_t receiver_thread;
	//realistically we should validate emulator fields here but im an optimist
    	if (pthread_create(&receiver_thread, NULL, receive_and_process, emulator_data) != 0) {
        	fprintf(stderr, "Error creating thread\n");
        	exit(EXIT_FAILURE);
    	}
    	if (pthread_join(receiver_thread, NULL) != 0) {
        	fprintf(stderr, "Error joining thread\n");
        	exit(EXIT_FAILURE);
    	}
    	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 6) {
        	fprintf(stderr, "Usage: %s <data-rate(Bytes/s)>, Rm, D(Max)"
				"<our_port> <server_port>\n",
			       	argv[0]);
        	exit(EXIT_FAILURE);
    	}
    	printf("\n *** TO ADJUST SENDERS INTERVAL TIME. "
		    "CHANGE MACRO AT FILE TOP\n\n\n");
    	
	
	/*Create. Initialize. Launch Emulator. (Not up to date)*/
	Emulator* emulator;
    	emulator = (Emulator*)malloc(sizeof(Emulator));
	emulator->throughput = atoi(argv[1]);
    	emulator->min_rtt = strtod(argv[2],NULL);
    	emulator->max_additional_rtt = strtod(argv[3],NULL);
    	emulator->em_port = atoi(argv[4]);
  	emulator->serv_port = atoi(argv[5]);
    /*Initialize queue.*/
        init_queue(emulator);

	BPS = atoi(argv[1]);
	token_bucket = BPS;

	 char* curTimeStamp_str = current_unix_timestamp();
               first_start =  strtod(curTimeStamp_str, NULL);

//	initialize_emulator(emulator, argv);

	if(launch_emulator_blocking(emulator))
		printf("Emulator Finished\n");


	return 0;
}












