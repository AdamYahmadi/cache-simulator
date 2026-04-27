#include "simulation.hpp"
#include <chrono>
#include <systemc>

Result run_simulation(int cycles, int directMapped, unsigned cacheLines, 
                      unsigned cacheLineSize, unsigned cacheLatency, 
                      unsigned memoryLatency, size_t numRequests, 
                      struct Request requests[], const char* tracefile) {

    auto start = std::chrono::high_resolution_clock::now();
    
    // SystemC Clock and Module Instantiation
    sc_clock clk("clk", 1, SC_NS);
    
    DirectMappedCache directMappedCache("directMappedCache");
    FullyAssociativeCache fullyAssociativeCache("fullyAssociativeCache");

    // Initialize Cache Parameters
    directMappedCache.initialize(cacheLines, cacheLineSize, cacheLatency, memoryLatency);
    fullyAssociativeCache.initialize(cacheLines, cacheLineSize, cacheLatency, memoryLatency);
    
    // Setup Simulation Wrapper
    Simulation simulation("sim", directMappedCache, fullyAssociativeCache);
    simulation.clk(clk);
    simulation.initialize(numRequests, requests, tracefile, directMapped); 

    // Execute Simulation
    sc_start(cycles, SC_NS);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "Simulation duration: " << duration.count() << " seconds" << std::endl;

    Result result;

    // Direct Mapped Branch
    if (directMapped) {
        if (simulation.rq1.read() < numRequests) {
            result.cycles = SIZE_MAX;
            result.misses = simulation.misses1.read();
            result.hits   = simulation.hits1.read();
            result.primitiveGateCount = simulation.primitiveGateCount1.read();
        } else {
            result.cycles = simulation.cycles1.read();
            result.misses = simulation.misses1.read();
            result.hits   = simulation.hits1.read();
            result.primitiveGateCount = simulation.primitiveGateCount1.read();
        }
    } 
    // Fully Associative Branch
    else {
        if (simulation.rq2.read() < numRequests) {
            result.cycles = SIZE_MAX;
            result.misses = simulation.misses2.read();
            result.hits   = simulation.hits2.read();
            result.primitiveGateCount = simulation.primitiveGateCount2.read();
        } else {
            result.cycles = simulation.cycles2.read();
            result.misses = simulation.misses2.read();
            result.hits   = simulation.hits2.read();
            result.primitiveGateCount = simulation.primitiveGateCount2.read();
        }
    }

    return result;
}

int sc_main(int argc, char* argv[]) {  
    // Dummy sc_main to satisfy SystemC linker requirements
    exit(EXIT_FAILURE);
    return 1;
}