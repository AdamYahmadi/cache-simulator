#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>

// --- Prototypes ---
void parse_commands(int argc, char const *argv[]);
void help();
int readfile(const char *file);

// --- Structures ---
struct Result {
    size_t cycles;
    size_t misses;
    size_t hits;
    size_t primitiveGateCount;
};

struct Request {
    uint32_t addr;
    uint32_t data;
    int we; // Write Enable
};

// --- Simulation Settings (Defaults) ---
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

// External SystemC Simulation Engine
extern struct Result run_simulation(int cycles, int directMapped, unsigned cacheLines,
                                    unsigned cacheLineSize, unsigned cacheLatency,
                                    unsigned memoryLatency, size_t numRequests,
                                    struct Request requests[], const char* tracefile);

int main(int argc, char const *argv[]) {
    // 1. CLI Argument Parsing
    parse_commands(argc, argv);

    // 2. Trace Ingestion
    if (readfile(inputFile) != 0) {
        fprintf(stderr, "Error: Could not process CSV input file.\n");
        return EXIT_FAILURE;
    }

    // 3. Simulation Execution
    struct Result result = run_simulation(
        cycles, directMapped, cacheLines, cacheLineSize, 
        cacheLatency, memoryLatency, numRequests, requests, tracefile
    );

    // 4. Output Results
    printf("--- Simulation Results ---\n");
    printf("Total Cycles: %zu\n", result.cycles);
    printf("Cache Misses: %zu\n", result.misses);
    printf("Cache Hits:   %zu\n", result.hits);
    printf("Logic Gates:  %zu\n", result.primitiveGateCount);

    // 5. Cleanup
    free(requests);
    return EXIT_SUCCESS;
}

void parse_commands(int argc, char const *argv[]) {
    size_t check_cycle = 0;
    size_t check_type = 0;

    for (int i = 1; i < argc; i++) {
        // Cycles Configuration
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cycles") == 0) {
            if (check_cycle) {
                fprintf(stderr, "Error: Duplicate cycle limit options.\n");
                exit(EXIT_FAILURE);
            }
            check_cycle = 1;
            if (i + 1 < argc) {
                char *endptr;
                cycles = (int)strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || cycles < 0) {
                    fprintf(stderr, "Invalid cycle count: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            }
        } 
        // Mapping Type
        else if (strcmp(argv[i], "--directmapped") == 0) {
            if (check_type) {
                fprintf(stderr, "Error: Conflict between mapping type options.\n");
                exit(EXIT_FAILURE);
            }
            check_type = 1;
            directMapped = 1;
        } 
        else if (strcmp(argv[i], "--fullassociative") == 0) {
            if (check_type) {
                fprintf(stderr, "Error: Conflict between mapping type options.\n");
                exit(EXIT_FAILURE);
            }
            check_type = 1;
            directMapped = 0;
        } 
        // Cache Geometry
        else if (strcmp(argv[i], "--cacheLineSize") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                cacheLineSize = (unsigned)strtol(argv[++i], &endptr, 10);
                // Ensure power of two
                if (*endptr != '\0' || !(cacheLineSize > 0 && (cacheLineSize & (cacheLineSize - 1)) == 0)) {
                    fprintf(stderr, "Error: cacheLineSize must be a power of two: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            }
        } 
        else if (strcmp(argv[i], "--cacheLines") == 0) {
            if (i + 1 < argc) {
                char *endptr;
                cacheLines = (unsigned)strtol(argv[++i], &endptr, 10);
                if (*endptr != '\0' || !(cacheLines > 0 && (cacheLines & (cacheLines - 1)) == 0)) {
                    fprintf(stderr, "Error: cacheLines must be a power of two: %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
            }
        } 
        // Latency and Debug
        else if (strcmp(argv[i], "--cacheLatency") == 0) {
            if (i + 1 < argc) cacheLatency = (unsigned)atoi(argv[++i]);
        } 
        else if (strcmp(argv[i], "--memoryLatency") == 0) {
            if (i + 1 < argc) memoryLatency = (unsigned)atoi(argv[++i]);
        } 
        else if (strcmp(argv[i], "--tf") == 0) {
            if (i + 1 < argc) tracefile = argv[++i];
        } 
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help();
            exit(EXIT_SUCCESS);
        } 
        // Input File (Positional Argument)
        else {
            inputFile = argv[i];
        }
    }
}

void help() {
    printf("Usage: cache_simulator [options] <input_file>\n");
    printf("Options:\n");
    printf("  -c, --cycles <n>       Max simulation cycles\n");
    printf("  --directmapped         Model direct-mapped cache (default)\n");
    printf("  --fullassociative      Model fully associative cache\n");
    printf("  --cacheLineSize <n>    Size in bytes (must be power of 2)\n");
    printf("  --cacheLines <n>       Number of lines (must be power of 2)\n");
    printf("  --tf <filename>        VCD tracefile output path\n");
    printf("  -h, --help             Show this help message\n");
}

int readfile(const char *filename) {
    if (!filename) {
        fprintf(stderr, "Error: No input file specified.\n");
        return -1;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("File Open Error");
        return -1;
    }

    char line[1024];
    numRequests = 0;

    // Phase 1: Count requests for allocation
    while (fgets(line, sizeof(line), file)) {
        numRequests++;
    }

    requests = malloc(numRequests * sizeof(struct Request));
    if (!requests) {
        perror("Memory Allocation Error");
        fclose(file);
        return -1;
    }

    rewind(file);
    size_t counter = 0;

    // Phase 2: Parse CSV data
    while (fgets(line, sizeof(line), file) && counter < numRequests) {
        char *pos = strchr(line, '\n');
        if (pos) *pos = '\0';

        char *data[3];
        int data_count = 0;
        char *token = strtok(line, ",");
        
        while (token && data_count < 3) {
            data[data_count++] = token;
            token = strtok(NULL, ",");
        }

        // Parse Operation (W/R)
        if (strcmp(data[0], "W") == 0) {
            requests[counter].we = 1;
        } else if (strcmp(data[0], "R") == 0) {
            requests[counter].we = 0;
        } else {
            fprintf(stderr, "Syntax Error: Line %zu has invalid operation '%s'\n", counter + 1, data[0]);
            continue;
        }

        // Parse Hex Address
        char *endptr;
        requests[counter].addr = (uint32_t)strtoul(data[1], &endptr, 16);
        
        // Parse Data (only for writes)
        if (requests[counter].we == 1 && data_count == 3) {
            requests[counter].data = (uint32_t)strtoul(data[2], &endptr, 10);
        } else {
            requests[counter].data = 0;
        }
        
        counter++;
    }

    numRequests = counter;
    fclose(file);
    return 0;
}