
/*
 * Michael lammens
 * XLW945
 * 11335630
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <assert.h>
#include <list.h>
//#define myport 8050
#define MSS 1460
#define MAX_HISTORY_SIZE 1000

int myport;
int PSIZE;
int window_size;
int cur_out;
int dmin;
int dmax;
int total_bytes;
int d_variance;
double base_rtt;
int avg_tp;

LIST* received_packets;
int sock_fd, sock_fd_s;
struct sockaddr_in client_addr, dest_addr;
double alpha;// =.0000001; //0.000001;
double beta;// = .01;
pthread_mutex_t window_mtx  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cur_out_mutex  = PTHREAD_MUTEX_INITIALIZER;


int exp_rtt;

char* start_time_str;
double start_time;

char* end_time_str;
int end_time;



/*New features*/
int adapt_timeout;
int total_transit_time;
void calculate_adaptive_thresholds();
double moving_average(double data[], int n);
/*Timeouts Sending*/
double min_adapt_timeout = 0.01; // Minimum adaptive timeout value (in seconds)
double max_adapt_timeout = 0.1; // Maximum adaptive timeout value (in seconds)
double adapt_timeout_step = 0.005; // Step size for adjusting adaptive timeout (in seconds)
// Function to calculate adaptive timeout based on network conditions
double calculate_adaptive_timeout();
/*Multiple alpha ranges*/
double alpha2;
double alpha3;
double beta2;
double beta3;
int D1_index = 0;
double diffs_10[10];
double diffs_50[50];
double diffs_100[100];
void update_diff_history(double diff); 


typedef struct {
    double throughput[MAX_HISTORY_SIZE];
    double rtt[MAX_HISTORY_SIZE];
    int count;
} HistoryData;

HistoryData* history; 



typedef struct {
    int pack_id;
    int source_port;
    int num_bytes;
    int ws;
    char start_time[30];
    char end_time[30];
    double rtt;
    int throughput;
    int throughput_ind;
}Segment;



char* current_unix_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double value_d = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;

    char *timestamp_str = (char *)malloc(30 * sizeof(char)); // Allocate memory dynamically
    if (timestamp_str == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    snprintf(timestamp_str, 30, "%.6lf", value_d);

  //  printf("\nFUNC current time=%s", timestamp_str);
    return timestamp_str;
}



int nano_sleep_seconds(double seconds) {
    //printf("\ninside nanosleep: %f", seconds);
    struct timespec sleepTime;
    sleepTime.tv_sec = (time_t)floor(seconds); // Extract the integer part for seconds
    sleepTime.tv_nsec = (long)((seconds - floor(seconds)) * 1000000000); // Extract the fractional part for nanoseconds
    // Sleep for the calculated duration
    return nanosleep(&sleepTime, NULL);
}


/* Serializing response struct fields into buffer string
 *
 * Pack: Pointer to data that we will put in buffer
 * Buf: Buffer to load data in
 *
 * Return num bytes serialized
 */
uint16_t serialize(Segment* toSend, char* buffer) {
    uint16_t bytes = 0;
    memcpy(buffer + bytes, &toSend->pack_id, sizeof(toSend->pack_id));
    bytes += sizeof(toSend->source_port);
    memcpy(buffer + bytes, &toSend->source_port, sizeof(toSend->source_port));
    bytes += sizeof(toSend->source_port);
    memcpy(buffer + bytes, &toSend->num_bytes, sizeof(toSend->num_bytes));
    bytes += sizeof(toSend->num_bytes);
    memcpy(buffer + bytes, &toSend->ws, sizeof(toSend->ws));
    bytes += sizeof(toSend->ws);
    memcpy(buffer + bytes, &toSend->start_time, strlen(toSend->start_time));
    bytes += strlen(toSend->start_time);



    return bytes;
}
/* Opposite of serialize, populate a packet struct with data from a buffer: Buf
 *
 * Pack: Pointer to packet we want to fill
 * Buf: Buffer string with data we want to put into pack
 *
 * */
int deserialize(char* buf, Segment* pack) {
    size_t offset = 0;
    memcpy(&pack->pack_id, buf + offset, sizeof(int));
    pack->pack_id = ntohs(pack->pack_id);
    offset += sizeof(int);
    memcpy(&pack->source_port, buf + offset, sizeof(int));
    pack->source_port = ntohs(pack->source_port);
    offset += sizeof(int);
    memcpy(&pack->num_bytes, buf + offset, sizeof(int));
    pack->num_bytes = ntohs(pack->num_bytes);
    offset += sizeof(int);
    memcpy(&pack->ws, buf + offset, sizeof(int));
    pack->ws = ntohs(pack->ws);
    offset += sizeof(int);
    memcpy(&pack->start_time, buf + offset, 31);
    offset += strlen(buf+offset);
    return 0;
}


void sigint_handler(int sig) {
    // Handle SIGINT (Ctrl+C) by killing all child processes
    // This function will be called when Ctrl+C is pressed
    (void)sig; // Suppress unused variable warning
    printf("Terminating child processes...\n");
    kill(0, SIGKILL); // Kill all child processes in the same process group
    exit(EXIT_SUCCESS);
}


 /*Purposed to avoid restructuring emulator. Emulator serializes twice. This is a temporarary fix
  * todo: Fix emulator and deserialize is sufficient. 
  * */
void convert_pack_net_host(Segment* pack){
	pack->pack_id = ntohs(pack->pack_id);
	pack->source_port = ntohs(pack->source_port);
	pack->num_bytes = ntohs(pack->num_bytes);
	pack->ws = ntohs(pack->ws);
}

void set_end_time(Segment* pack){
	char* cur_time = current_unix_timestamp();	
	strcpy(pack->end_time,cur_time);
}
/* Sets rtt time to the endtime - starttime
 * diff is the seconds from send to rec.
 * */
void calc_rtt(Segment* pack){
	double dub_end, dub_start;

	/*Convert to double from string*/
	dub_end = strtod(pack->end_time, NULL);
        dub_start = strtod(pack->start_time, NULL);

	pack->rtt = dub_end - dub_start;
}

void print_all_stats_received(LIST* recd){
	Segment* current;
	int total_time = 0;
	double time_sum = end_time - start_time;
	current=(Segment*)ListFirst(recd);
	while(current != NULL){
		double rtt = current->rtt;
		int src= current->source_port;
		int id = current->pack_id;
		int tp = current->throughput;
		int tp_ind = current->throughput_ind;
		total_time += rtt;
		
		printf("Pack #%d: RTT:%f Throughput(bps, packet size = %d): %d Port: %d, trailing TP: %d\n",id, rtt,PSIZE, tp_ind, src, tp);
		current=(Segment*)ListNext(recd);
	}
	printf("Total transfer time: %f/sec\n",time_sum);
}
double base_TP;
int streak_;
void update_window(Segment* temp){
        /*First update base_rtt*/       
	if(temp->rtt < base_rtt){
        	base_rtt = temp->rtt;
       	}
	
	if(temp->throughput > base_TP){
		base_TP = temp->throughput;
	}
	double extra_data = temp->rtt - base_rtt;

	


	double expected = window_size /(double)base_rtt; //(double)base_TP; //window_size/(double)base_rtt; // dmin;
	double actual =   window_size /(double)(temp->rtt);  //(double)temp->throughput; //window_size/(double)temp->rtt; //temp->rtt;
	double diff = expected - actual;//expected-actual;
	diff = temp->rtt - base_rtt;			
//	diff = diff * window_size; //DIFF = window size * packet latency
	/*New base RTT*/
	//base_rtt = window_size * (extra_data / temp->rtt);

	/*Add new diff to history. Get new Average*/					/*--------------------*/
	update_diff_history(diff);						/*DIFFS_10 ------- DIFFS_50------DIFFS_100*/
	double diff_avg = moving_average(diffs_10,D1_index);			/* Short Term Average. 	Middle Term. 	Longterm       	*/
	printf("\nMoving Avg Diffs %f", diff_avg);

	pthread_mutex_lock(&window_mtx);
	if(temp->ws == window_size){/*Received packets window size is equal to current windowside === our analysis is more accurate to current env*/
		streak_ +=1;
	}

	/*Update Window Size After we observe the effect of the last change*/

	if(streak_ < window_size*100){
		/*Wait until we see the effects of our changes*/
		
	}else{
		if(diff_avg < alpha){
			window_size = window_size + 1;
			streak_ = 0;
		}else if (diff_avg > beta) {
			window_size = window_size -1;
			streak_ = 0;
			if(window_size<1){
				window_size = 1;
			}
		}else{
			;/*Keep windowsize the same*/
		}
	}

	printf("\nPack#: %d,Size: %d, RTT: %f (ED: %f), WS: %d (NEW: %d), diff: %f, Ind TP: %d - Avg TP: %d - End-Time: %s ",temp->pack_id, PSIZE,temp->rtt,extra_data, temp->ws,window_size,diff, temp->throughput_ind, temp->throughput,temp->end_time);
	printf("\n^---^ Base_RTT: %f --- Base_TP: %f --- CUR OUT: %d, expected: %f, actual %f, Alpha: %f, Beta: %f", base_rtt, base_TP, cur_out, expected, actual,alpha, beta);
	pthread_mutex_unlock(&window_mtx);

}

void update_throughput(Segment* temp){
	// Update historical data with packet's RTT
        total_bytes += PSIZE;

	int tp_ind = PSIZE / (temp->rtt);
	temp->throughput_ind = tp_ind;

	if (history->count < MAX_HISTORY_SIZE) {
        	// If history is not full, add the RTT to the next available slot
        	history->rtt[history->count] = temp->rtt;
		
		history->throughput[history->count] = tp_ind;
		history->count +=1;
	} else {
        	// If history is full, shift the existing data and add the new RTT at the end
        	for (int i = 0; i < MAX_HISTORY_SIZE - 1; ++i) {
            		history->rtt[i] = history->rtt[i + 1];
			history->throughput[i] = history->throughput[i+1];
        	}
        	history->rtt[MAX_HISTORY_SIZE - 1] = temp->rtt;
    		history->throughput[MAX_HISTORY_SIZE - 1] = temp->throughput_ind;
	}
	total_transit_time += temp->rtt;

	avg_tp = total_bytes / total_transit_time;
	temp->throughput = avg_tp;/*Total Average*/
	char cur_time_x[30];
	strcpy(cur_time_x, current_unix_timestamp());
	double cur_time_y = strtod(cur_time_x, NULL);
	double tot_time = cur_time_y - start_time;
	
	double TP_TOT = total_bytes / tot_time;
	printf("Current Total TP: %f",TP_TOT);	
}



typedef struct {
    int num_send;
} ThreadArgs;

void* rec_background_routine(void* arg){
	ThreadArgs *args = (ThreadArgs *)arg;
	int num_send = args->num_send;
	char data[512] = {'\0'};
	for(int i = 0; i<num_send;i++){
		memset(data, '\0', sizeof(data));

        	Segment* temp = (Segment*)malloc(sizeof(Segment));
		socklen_t addr_len = sizeof(client_addr);
		int bytes_received = recvfrom(sock_fd_s, data, sizeof(data), 0,
                                      (struct sockaddr *)&client_addr,
                                      &addr_len);
        	if (bytes_received == -1) {
        	    perror("recvfrom");
        	    exit(EXIT_FAILURE);
        	}
		/*Update number of packets un-acked*/
	        pthread_mutex_lock(&cur_out_mutex);
		cur_out -=1;
		pthread_mutex_unlock(&cur_out_mutex);
		
		/*Process received packet */
        	deserialize(data, temp);
		convert_pack_net_host(temp);

		set_end_time(temp);	
		calc_rtt(temp);
		
		
		update_throughput(temp);
		
		//calculate_adaptive_thresholds();


		ListAppend(received_packets, temp);	
		printf("\n");
		update_window(temp);	
	}
	return NULL;	
}
void launch_rec_background(pthread_t *receiver_thread, int num_send) {
	ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
    	if (args == NULL) {
        	fprintf(stderr, "Error allocating memory\n");
        	exit(EXIT_FAILURE);
    	}
    	
	args->num_send = num_send;
	    
        if (pthread_create(receiver_thread, NULL, rec_background_routine, args) != 0) {
            	fprintf(stderr, "Error creating thread\n");
        	exit(EXIT_FAILURE);
    	}
}




int main(int argc, char *argv[]) {
        if (argc != 5) {
                fprintf(stderr, "Usage: %s <Number of packets to send>"
                                "<Packet Size(Bytes)>,<myport>,<emulator port>\n",
                                argv[0]);
                exit(EXIT_FAILURE);
    
	}
	/*List of all packets weve received. All tracking metrics should be set when appended to*/
	received_packets = ListCreate();
	
	/*CMD Args + Declare locals*/
	
	Segment* toSend;
        int num_send = atof(argv[1]);
	PSIZE = atoi(argv[2]);
	myport = atoi(argv[3]);
	int server_port = atoi(argv[4]);
        
	char data[512] = {'\0'};
        char buffer_out[512] = {'\0'};


	/* Set globals */
	base_TP = -1;
	streak_ = 0;
	base_rtt = 100000.0; /* Keep base RTT at the smallest rtt recorded*/
	window_size = 10;
	alpha = 1;
	beta = 3;
	avg_tp = 0;
	total_bytes = 0;
	start_time_str = current_unix_timestamp();
	start_time = strtod(start_time_str, NULL);
	

	adapt_timeout = 0;
	total_transit_time = 0;
	
	/*init history*/
	history = (HistoryData*)malloc(sizeof(HistoryData)); 
   
	if (history == NULL) {
        	// Handle failure to allocate memory
        	fprintf(stderr, "Failed to allocate memory for history\n");
        	return 1; // Return a non-zero value to indicate failure
    	}
	for (int i = 0; i < MAX_HISTORY_SIZE; ++i) {
        	history->throughput[i] = 0.0;
        	history->rtt[i] = 0.0;
    	}
	// Initialize the diffs_10 array to 0.00
	for (int i = 0; i < 10; i++) {
    		diffs_10[i] = 0.00;
	}


		
	/*Create sockets + bind*/
	toSend = (Segment*)malloc(sizeof(Segment));
	toSend->source_port = htons(myport);
	if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	perror("Socket creation failed");
        	exit(EXIT_FAILURE);
    	}
    	if ((sock_fd_s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	perror("Socket creation failed");
        	exit(EXIT_FAILURE);
    	}
    	dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        dest_addr.sin_port = htons(server_port);

    	client_addr.sin_family = AF_INET;
    	client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    	client_addr.sin_port = htons(myport);
    
    	if(bind(sock_fd_s, (struct sockaddr *)&client_addr, sizeof(client_addr))
        	            == -1) {
        	perror("Bind failed");
        	close(sock_fd);
        	printf("\nRec and proc failed\n");
        	exit(-1);
    	}
    	/*Launch receiver background thread*/
	pthread_t receiver_thread;
    	launch_rec_background(&receiver_thread, num_send);
	
/*Start sending loop*/
    int bytes;
    cur_out = 0;
    int num_sent = 0;
    while(num_sent < num_send) {
	/* Vegas tracks RTT of every single packet sent*/    	    
	if(cur_out < window_size){
		toSend->pack_id = htons(num_sent+1);
        	toSend->num_bytes = htons(PSIZE);
		toSend->ws = htons(window_size);
		bytes = serialize(toSend, buffer_out);

	//	nano_sleep_seconds(calculate_adaptive_timeout());
		/* sending speed regulation */
		nano_sleep_seconds(.01);	
		sendto(sock_fd, buffer_out,bytes+1, 0,
	       		(struct sockaddr *)&dest_addr, sizeof(dest_addr));
		memset(data, '\0', sizeof(data));
		
		num_sent +=1;
		pthread_mutex_lock(&cur_out_mutex);
		cur_out +=1;
		pthread_mutex_unlock(&cur_out_mutex);
    	}
	pthread_mutex_unlock(&cur_out_mutex);

    }
	if (pthread_join(receiver_thread, NULL) != 0) {
        	fprintf(stderr, "Error joining thread\n");
        	exit(EXIT_FAILURE);
    }

	end_time_str = current_unix_timestamp();
	end_time = strtod(end_time_str, NULL);
	printf("\n Received all expected threads back... Here are the stats in the order they were sent: \n\n");
	print_all_stats_received(received_packets);


    return 0;
    
}


/*New Features*/
// Function to calculate moving average
double moving_average(double data[], int n) {
    if (n == 0) {
        return 0.0; // Return a default value or handle it appropriately
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    return (double)sum / n;
}
void update_diff_history(double diff){	
	if (D1_index < 10) {
                // If history is not full, add the RTT to the next available slot
                diffs_10[D1_index] = diff;
              	D1_index +=1; 
        } else {
                // If history is full, shift the existing data and add the new RTT at the end
                for (int i = 0; i < 10 - 1; ++i) {
                        diffs_10[i] = diffs_10[i + 1];
               }
              	diffs_10[10 - 1] = diff;
        }

}

// Function to calculate adaptive thresholds
void calculate_adaptive_thresholds() {
    // Compute moving averages for throughput and RTT
    double avg_throughput = moving_average(history->throughput, history->count);
    double avg_rtt = moving_average(history->rtt, history->count);
    // Adjust thresholds based on historical data
    //alpha = (avg_throughput / window_size)*.005;
    alpha =  1/avg_throughput;
    beta = (avg_rtt / window_size)*1; // Example adjustment
	printf("\nHISTORICAL ---  AVG TP:%f, AVG RTT: %f --- NEW A: %f, NEW B: %f", avg_throughput, avg_rtt, alpha, beta);
}

double calculate_adaptive_timeout() {
    double dynamic_timeout;
    if (history->count > 0) {
        // Calculate moving average RTT from historical data
        double avg_rtt = moving_average(history->rtt, history->count);
        // Adjust the timeout based on the average RTT
        dynamic_timeout = avg_rtt/10;
    } else {
        // If no historical data available, use a default timeout value
        dynamic_timeout = .001;
    }

    // Ensure the adaptive timeout falls within the specified range
    dynamic_timeout = fmax(min_adapt_timeout, fmin(max_adapt_timeout, dynamic_timeout));
    return dynamic_timeout;
} 

