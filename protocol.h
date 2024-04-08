/*Michael Lammens - XLW945 -11335630*/
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
#include <signal.h>



typedef struct {
        int pack_id;
        int source_port;
        int num_bytes;
	int ws;
        char start_time[30];
}Segment;

unsigned int generate_seed(double additional_randomness) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    // Convert timespec to nanoseconds
    unsigned int seed = (unsigned int)(ts.tv_sec * 1000000000LL + ts.tv_nsec);

    // Add some additional randomness
    seed ^= (unsigned int)(additional_randomness * 1000000000);

    // Mix the bits for additional randomness
    seed ^= (seed >> 12) ^ (seed << 25) ^ (seed >> 27);

    return seed;
}


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


int nano_second_sleep(double seconds) {
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1e9);
    return nanosleep(&req, NULL);
}

double generate_random_value(double max_additional_rtt, double other_value) {
    unsigned int seed = generate_seed(other_value);
    // Seed the random number generator
    srand(seed);
    double random_value;
    random_value = (double)rand() / RAND_MAX;
    random_value *= max_additional_rtt;

    // Truncate to have 7 digits after the decimal point
    random_value = (int)(random_value * 10000000) / 10000000.0;

    return random_value;
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



