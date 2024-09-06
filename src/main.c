#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>

void parse_commands(int argc, char const *argv[]);
void help();
int readfile(const char *file);

struct Result {
    size_t cycles;
    size_t misses;
    size_t hits;
    size_t primitiveGateCount;
    };

struct Request {    
    uint32_t addr;
    uint32_t data;
    int we;  
};

static int cycles = 3000;
static int directMapped = 1;
static unsigned cacheLines = 256;
static unsigned cacheLineSize = 32;
static unsigned cacheLatency = 1;
static unsigned memoryLatency = 5;
static const char *inputFile = NULL;
static const char *tracefile = NULL;

static struct Request* requests;
static size_t numRequests = 0;


extern struct Result run_simulation(int cycles, int directMapped, unsigned cacheLines,
                      unsigned cacheLineSize, unsigned cacheLatency,
                      unsigned memoryLatency, size_t numRequests,
                      struct Request requests[], const char* tracefile);

int main(int argc, char const *argv[]) {

    parse_commands(argc, argv); //for parsing commands

    int check = readfile(inputFile);

    if (check != 0) {
        fprintf(stderr,"Can't read csv-file\n");
    }

    struct Result result = run_simulation(cycles, directMapped, cacheLines, cacheLineSize, cacheLatency, memoryLatency, numRequests, requests, tracefile); //simulation

    printf("Cycles: %zu\n", result.cycles);
    printf("Misses: %zu\n", result.misses);
    printf("Hits: %zu\n", result.hits);
    printf("Gates: %zu\n", result.primitiveGateCount);

    free(requests);

    return 0;
}


void parse_commands(int argc, char const *argv[]) {
    size_t check_cycle = 0;
    size_t check_type = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cycles") == 0) {
            if (check_cycle) {
                fprintf(stderr, "Error: Options -c and --cycles cannot be used together.\n");
                exit(EXIT_FAILURE);
            }
            check_cycle = 1;
            if (i + 1 < argc) {
                char *endptr;
                cycles = strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || cycles < 0) {
                    fprintf(stderr, "Invalid number for cycles: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Option %s requires an argument.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--directmapped") == 0) {
            if (check_type) {
                fprintf(stderr, "Error: Options --directmapped and --fullassociative cannot be used together.\n");
                exit(EXIT_FAILURE);
            }
            check_type = 1;
            directMapped = 1;
        } else if (strcmp(argv[i], "--fullassociative") == 0) {
            if (check_type) {
                fprintf(stderr, "Error: Options --directmapped and --fullassociative cannot be used together.\n");
                exit(EXIT_FAILURE);
            }
            check_type = 1;
            directMapped = 0;
        } else if (strcmp(argv[i], "--cacheLineSize") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                cacheLineSize = strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || !(cacheLineSize > 0 && (cacheLineSize & (cacheLineSize - 1)) == 0)) {
                    fprintf(stderr, "Invalid number for cacheLineSize: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Option %s requires an argument.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--cacheLines") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                cacheLines = strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || !(cacheLines > 0 && (cacheLines & (cacheLines - 1)) == 0)) {
                    fprintf(stderr, "Invalid number for cacheLines: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Option %s requires an argument.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--cacheLatency") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                cacheLatency = strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || cacheLatency < 0) {
                    fprintf(stderr, "Invalid number for cacheLatency: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Option %s requires an argument.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--memoryLatency") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                memoryLatency = strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || memoryLatency < 0) {
                    fprintf(stderr, "Invalid number for memoryLatency: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Option %s requires an argument.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--tf") == 0) {
            if (i + 1 < argc) {
                tracefile = argv[++i];
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help();
            exit(0);
        } else {
            inputFile = argv[i];
        }
        
    }
}

void help() {
    printf("Usage: cache_simulator [options] <input_file>\n");
    printf("Options:\n");
    printf("  -c <number>, --cycles <number>      Number of cycles to simulate\n");
    printf("  --directmapped                      Simulate a direct-mapped cache\n");
    printf("  --fullassociative                   Simulate a fully associative cache\n");
    printf("  --cacheLineSize <number>           Cache line size in bytes\n");
    printf("  --cacheLines <number>               Number of cache lines\n");
    printf("  --cacheLatency <number>            Cache latency in cycles\n");
    printf("  --memoryLatency <number>           Memory latency in cycles\n");
    printf("  --tf <filename>                     Tracefile output (optional)\n");
    printf("  -h, --help                          Display this help message\n");
}


int readfile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Unable to open the file");
        return -1;
    }

    char line[1024];
    numRequests = 0;

    // Count the number of requests
    while (fgets(line, sizeof(line), file)) {
        numRequests++;
    }

    requests = malloc(numRequests * sizeof(struct Request));
    if (requests == NULL) {
        perror("Unable to allocate memory for requests");
        fclose(file);
        return -1;
    }

    rewind(file);
    size_t counter = 0;

    // Process each line
    while (fgets(line, sizeof(line), file)) {
        char *pos;
        if ((pos = strchr(line, '\n')) != NULL) {
            *pos = '\0';
        }

        char *data[3];
        int data_count = 0;
        char *token = strtok(line, ",");
        while (token) {
            data[data_count++] = token;
            token = strtok(NULL, ",");
        }

        /*if (data_count != 3) { // Ensure that we have exactly three data points
            continue;
        }*/

        // Process the we
        if (strcmp(data[0], "W") == 0) {
            requests[counter].we = 1;
        } else if (strcmp(data[0], "R") == 0) {
            requests[counter].we = 0;
        } else {
            fprintf(stderr,"Invalid operation in line %zu: %s\n",counter+1, data[0]);
            return -1;
        }

        // Process address 
        char *endptr;
        requests[counter].addr = strtoul(data[1], &endptr, 16);
        if (*endptr != '\0'){
            fprintf(stderr, "Invalid address in line %zu: %s\n", counter+1, data[1]);
            return -1;
        }

        //Process the data
        if (requests[counter].we == 0) requests[counter].data = 0;
        else {
            requests[counter].data = strtoul(data[2], &endptr, 10);
            if (*endptr != '\0'){
                fprintf(stderr, "Invalid data in line %zu: %s\n", counter+1, data[2]);
                return -1;
            }
        }    
        counter++;
    }

    numRequests = counter; // Update numRequests to the actual count
    fclose(file);
    return 0;
}