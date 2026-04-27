#ifndef MODULES_HPP
#define MODULES_HPP

#include <systemc>
#include <cmath>
#include <vector>
#include <list>
#include <unordered_map>
#include <iostream>

using namespace sc_core;

// --- C-Linkage Interface ---
extern "C" {
    struct Request {
        uint32_t addr;
        uint32_t data;
        int we; // Write Enable
    };

    struct Result {
        size_t cycles;
        size_t misses;
        size_t hits;
        size_t primitiveGateCount;
    };

    Result run_simulation(int cycles, int directMapped, unsigned cacheLines,
                          unsigned cacheLineSize, unsigned cacheLatency,
                          unsigned memoryLatency, size_t numRequests,
                          Request requests[], const char* tracefile);
}

// --- Data Structures ---
struct CacheLine {
    bool valid;
    uint32_t tag;
    std::vector<uint8_t> data;
    
    CacheLine() : valid(false), tag(0), data() {}
    CacheLine(unsigned cacheLineSize) : valid(false), tag(0), data(cacheLineSize) {}
};

// --- Direct Mapped Cache Module ---
SC_MODULE(DirectMappedCache) {
    sc_in<bool> clk;
    sc_in<bool> enable;

    // Ports
    sc_in<uint32_t> address;
    sc_in<uint32_t> Wdata;
    sc_in<int> we;
    sc_out<uint32_t> Rdata;
    
    // Statistics
    sc_out<size_t> cycles;
    sc_out<size_t> misses;
    sc_out<size_t> hits;
    sc_out<size_t> primitiveGateCount;
    sc_out<int> rq;

    std::unordered_map<uint32_t, uint8_t> memory;
    std::vector<CacheLine> cache;
    unsigned cacheLines, cacheLineSize, cacheLatency, memoryLatency;

    SC_CTOR(DirectMappedCache) {
        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void initialize(unsigned cacheLines, unsigned cacheLineSize, unsigned cacheLatency, unsigned memoryLatency) {
        this->cacheLines = cacheLines;
        this->cacheLineSize = cacheLineSize;
        this->cacheLatency = cacheLatency;
        this->memoryLatency = memoryLatency;
        cache.resize(cacheLines);
        for (auto& line : cache) { 
            line.data.resize(cacheLineSize); 
        }
    }

    void exec() {
        while (true) {
            wait();
            if (!enable.read()) continue; // Skip if this mode is disabled

            rq.write(rq.read() + 1);
            uint32_t addr = address.read();
            uint32_t data;
            
            if (we.read() == 1) {
                // Write Operation
                data = Wdata.read();
                writeDataInCache(addr, data);
                writeDataInMemory(addr, data);
            } else {
                // Read Operation
                bool hit = readDataInCache(addr, data);
                if (hit) {
                    Rdata.write(data);
                    hits.write(hits.read() + 1);
                } else {
                    bool inMemory = readDataInMemory(addr, data);
                    if (inMemory) {
                        importMemoryBlockToCache(addr, data);
                        Rdata.write(data);
                    } else {
                        std::cerr << "Data fault: Address not in Cache or Memory" << std::endl;
                        Rdata.write(static_cast<uint32_t>(-1));
                    }
                    misses.write(misses.read() + 1);
                }
            }
            cycles.write(cycles.read() + 1);
        }
    }

    bool readDataInCache(uint32_t addr, uint32_t &data) {
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        unsigned index = (addr / cacheLineSize) % cacheLines;
        uint32_t offset = addr % cacheLineSize;
        uint32_t tag = calcTagOfDirectMapped(addr);

        if (cache[index].valid && cache[index].tag == tag) { 
            data = cache[index].data[offset];
            primitiveGateCount.write(primitiveGateCount.read() + 10);
            return true;
        }
        return false;
    }

    void writeDataInCache(uint32_t addr, uint32_t data) {
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        unsigned index = (addr / cacheLineSize) % cacheLines;
        uint32_t tag = calcTagOfDirectMapped(addr);
        uint32_t offset = addr % cacheLineSize;
        
        int entered = 0; 
        while (data > 0) {
            if (offset > (cacheLineSize - 1)) {
                offset = 0;
                index++;
                primitiveGateCount.write(primitiveGateCount.read() + 2);
            }
            if (index > (cacheLines - 1)) {
                primitiveGateCount.write(primitiveGateCount.read() + 1);
                std::cout << "Cache Full: Writing truncated." << std::endl;
                return;
            }
            entered++;
            cache[index].valid = true;
            cache[index].tag = tag;
            cache[index].data[offset] = data & 255;
            offset++;
            data = data >> 8; 
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        if (entered == 0 && data == 0) {
            cache[index].valid = true;
            cache[index].tag = tag;
            cache[index].data[offset] = data;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }     
    }

    void importMemoryBlockToCache(uint32_t addr, uint32_t &data) {
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        unsigned index = (addr / cacheLineSize) % cacheLines;
        uint32_t tag = calcTagOfDirectMapped(addr);
        uint32_t offset = addr % cacheLineSize;
        uint32_t startAddress = addr - (addr % cacheLineSize);

        for (int i = 0; i < (int)cacheLineSize; i++) {
            cache[index].data[i] = memory[startAddress + i];
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        cache[index].tag = tag;
        cache[index].valid = true;
        data = cache[index].data[offset];
    }

    bool readDataInMemory(uint32_t addr, uint32_t &data) {
        if (memory.find(addr) != memory.end()) {
            data = memory[addr];
            return true;
        }
        return false;
    }

    void writeDataInMemory(uint32_t addr, uint32_t data) {
        int entered = 0; 
        while (data > 0) {
            memory[addr + entered] = data & 255;
            entered++;
            data = data >> 8;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        if (entered == 0 && data == 0) {
            memory[addr] = data;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
    }

    uint32_t calcTagOfDirectMapped(uint32_t addr) {
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        uint32_t offsetBits = std::log2(cacheLineSize);
        uint32_t indexBits = std::log2(cacheLines);
        return addr >> (indexBits + offsetBits);
    }
};

// --- Fully Associative Cache Module ---
SC_MODULE(FullyAssociativeCache) {
    sc_in<bool> clk;
    sc_in<bool> enable;

    sc_in<uint32_t> address, Wdata;
    sc_in<int> we;
    sc_out<uint32_t> Rdata;
    sc_out<size_t> cycles, misses, hits, primitiveGateCount;
    sc_out<int> rq;
    
    std::list<uint32_t> tracker_lru;
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> map_with_tags;
    std::unordered_map<uint32_t, uint8_t> memory;
    std::vector<CacheLine> cache;
    unsigned cacheLines, cacheLineSize, cacheLatency, memoryLatency;

    SC_CTOR(FullyAssociativeCache) {
        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void initialize(unsigned cacheLines, unsigned cacheLineSize, unsigned cacheLatency, unsigned memoryLatency) {
        this->cacheLines = cacheLines;
        this->cacheLineSize = cacheLineSize;
        this->cacheLatency = cacheLatency;
        this->memoryLatency = memoryLatency;
        cache.resize(cacheLines);
        for (auto& line : cache) { line.data.resize(cacheLineSize); }
    }

    void exec() {
        while (true) {
            wait();
            if (!enable.read()) break; 

            rq.write(rq.read() + 1);
            uint32_t addr = address.read();
            uint32_t data;

            if (we.read() == 1) {
                data = Wdata.read();
                writeDataInCache(addr, data);
                writeDataInMemory(addr, data);
            } else {
                bool hit = readDataInCache(addr, data);
                if (hit) {
                    Rdata.write(data);
                    hits.write(hits.read() + 1);
                } else {
                    if (readDataInMemory(addr, data)) {
                        importMemoryBlockToCache(addr, data);
                        Rdata.write(data);
                    } else {
                        std::cerr << "Data fault: Address not in Cache or Memory" << std::endl;
                        Rdata.write(static_cast<uint32_t>(-1));
                    }
                    misses.write(misses.read() + 1);
                }
            }
            cycles.write(cycles.read() + 1);
        }
    }

    bool readDataInCache(uint32_t addr, uint32_t &data) {
        uint32_t tag = calctagFullAssociative(addr);
        uint32_t offset = addr % cacheLineSize;
        for (CacheLine &line : cache) {
            primitiveGateCount.write(primitiveGateCount.read() + 20);
            if (line.valid && line.tag == tag) {
                data = line.data[offset];
                primitiveGateCount.write(primitiveGateCount.read() + 10);
                tracker_lru.erase(map_with_tags[tag]); 
                LRU_first_update(tag);
                primitiveGateCount.write(primitiveGateCount.read() + 20);
                return true;
            }
        }
        return false;
    }

    void writeDataInCache(uint32_t addr, uint32_t data) {
        primitiveGateCount.write(primitiveGateCount.read() + 2);
        unsigned numBytes = 0;
        uint32_t temp = data; 
        while (temp > 0) { temp >>= 8; numBytes++; }
        if (numBytes == 0) numBytes = 1;

        while (numBytes > 0) {
            uint32_t offset = addr % cacheLineSize;
            uint32_t tag = calctagFullAssociative(addr);
            
            if (map_with_tags.find(tag) != map_with_tags.end()) {
                for (CacheLine &line : cache) {
                    if (line.valid && line.tag == tag) {
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        line.data[offset] = data & 255;
                        tracker_lru.erase(map_with_tags[tag]);
                        LRU_first_update(tag);    
                        primitiveGateCount.write(primitiveGateCount.read() + 1);
                        break;
                    }
                }
            } else if (tracker_lru.size() == cacheLines) {
                uint32_t tag_to_delete = tracker_lru.back();
                tracker_lru.pop_back();   
                map_with_tags.erase(tag_to_delete);   
                primitiveGateCount.write(primitiveGateCount.read() + 1);
                for (CacheLine &line : cache) {
                    if (line.valid && line.tag == tag_to_delete) {
                        line.tag = tag;
                        line.data[offset] = data & 255;
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        LRU_first_update(tag);
                        primitiveGateCount.write(primitiveGateCount.read() + 2);
                        break;
                    }
                }
            } else {
                for (CacheLine &line : cache) {
                    if (!line.valid) {
                        line.valid = 1;
                        line.tag = tag;
                        line.data[offset] = data & 255;
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        LRU_first_update(tag);
                        primitiveGateCount.write(primitiveGateCount.read() + 2);
                        break;
                    }
                }
            }
            data >>= 8;
            numBytes--;
            addr++;
            primitiveGateCount.write(primitiveGateCount.read() + 3);
        }
    }

    void importMemoryBlockToCache(uint32_t addr, uint32_t &data) {
        primitiveGateCount.write(primitiveGateCount.read() + 4);
        uint32_t startAddress = addr - (addr % cacheLineSize);
        uint32_t tag = calctagFullAssociative(addr);
        uint32_t offset = addr % cacheLineSize;
        uint32_t tag_to_delete = tracker_lru.back();
        
        tracker_lru.pop_back();  
        map_with_tags.erase(tag_to_delete);
        
        for (CacheLine &line : cache) {
            if (line.tag == tag_to_delete) {
                for (int i = 0; i < (int)cacheLineSize; i++) {
                    line.data[i] = memory[startAddress + i];
                    primitiveGateCount.write(primitiveGateCount.read() + 1);
                }
                primitiveGateCount.write(primitiveGateCount.read() + 10);
                line.tag = tag;
                tracker_lru.push_front(tag);
                primitiveGateCount.write(primitiveGateCount.read() + 20);
                map_with_tags[tag] = tracker_lru.begin();
                line.valid = true;
                data = line.data[offset];
            }
        }
    }
        
    bool readDataInMemory(uint32_t addr, uint32_t &data) {
        if (memory.find(addr) != memory.end()) {
            data = memory[addr];
            return true;
        }
        return false;
    }

    void writeDataInMemory(uint32_t addr, uint32_t data) {
        int entered = 0;
        while (data > 0) {
            memory[addr + entered] = data & 255;
            entered++;
            data >>= 8;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        if (entered == 0 && data == 0) {
            memory[addr] = data;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
    }
    
    void LRU_first_update(uint32_t tag) {
        tracker_lru.push_front(tag); 
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        map_with_tags[tag] = tracker_lru.begin(); 
    }

    uint32_t calctagFullAssociative(uint32_t addr) {
        primitiveGateCount.write(primitiveGateCount.read() + 2);
        uint32_t offsetBits = log2(cacheLineSize);
        return addr >> offsetBits;
    }
};

// --- Top-Level Simulation Wrapper ---
SC_MODULE(Simulation) {
    sc_in<bool> clk;
    sc_signal<bool> enable1, enable2;  

    sc_signal<uint32_t> address1, Wdata1, Rdata1, address2, Wdata2, Rdata2;
    sc_signal<int> we1, rq1, we2, rq2;
    sc_signal<size_t> cycles1, misses1, hits1, primitiveGateCount1;
    sc_signal<size_t> cycles2, misses2, hits2, primitiveGateCount2;

    size_t numRequests;
    struct Request* requests;
    const char* tracefile;
    int directMapped;

    DirectMappedCache& directMappedCache;
    FullyAssociativeCache& fullyAssociativeCache;

    void initialize(size_t n, struct Request r[], const char* tf, int dm) {
        numRequests = n; requests = r; tracefile = tf; directMapped = dm;
    }

    SC_CTOR(Simulation);
    Simulation(sc_module_name name, DirectMappedCache& dmC, FullyAssociativeCache& faC) 
        : directMappedCache(dmC), fullyAssociativeCache(faC) {
        
        // Port Binding - Direct Mapped
        directMappedCache.clk(clk);
        directMappedCache.address(address1);
        directMappedCache.Wdata(Wdata1);
        directMappedCache.Rdata(Rdata1);
        directMappedCache.we(we1);
        directMappedCache.cycles(cycles1);
        directMappedCache.misses(misses1);
        directMappedCache.hits(hits1);
        directMappedCache.primitiveGateCount(primitiveGateCount1);
        directMappedCache.enable(enable1);
        directMappedCache.rq(rq1);

        // Port Binding - Fully Associative
        fullyAssociativeCache.clk(clk);
        fullyAssociativeCache.address(address2);
        fullyAssociativeCache.Wdata(Wdata2);
        fullyAssociativeCache.Rdata(Rdata2);
        fullyAssociativeCache.we(we2);
        fullyAssociativeCache.cycles(cycles2);
        fullyAssociativeCache.misses(misses2);
        fullyAssociativeCache.hits(hits2);
        fullyAssociativeCache.primitiveGateCount(primitiveGateCount2);
        fullyAssociativeCache.enable(enable2);
        fullyAssociativeCache.rq(rq2);

        // Signal Initialization
        cycles1.write(0); misses1.write(0); hits1.write(0); rq1.write(0);
        cycles2.write(0); misses2.write(0); hits2.write(0); rq2.write(0);

        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void exec() {
        size_t request_index = 0;
        enable1.write(directMapped == 1);
        enable2.write(directMapped == 0);

        sc_trace_file* tfDM = NULL;
        sc_trace_file* tfFA = NULL;

        // VCD Tracing Logic
        if (directMapped && tracefile) {
            tfDM = sc_create_vcd_trace_file(tracefile);
            sc_trace(tfDM, clk, "clk");
            sc_trace(tfDM, address1, "address");
            sc_trace(tfDM, cycles1, "cycles");
            sc_trace(tfDM, misses1, "misses");
            sc_trace(tfDM, hits1, "hits");
        } else if (!directMapped && tracefile) {
            tfFA = sc_create_vcd_trace_file(tracefile);
            sc_trace(tfFA, clk, "clk");
            sc_trace(tfFA, address2, "address");
            sc_trace(tfFA, cycles2, "cycles");
            sc_trace(tfFA, misses2, "misses");
            sc_trace(tfFA, hits2, "hits");
        }

        while (true) {
            if (request_index < numRequests) {
                Request req = requests[request_index];
                if (directMapped) {
                    address1.write(req.addr); Wdata1.write(req.data); we1.write(req.we);
                } else {
                    address2.write(req.addr); Wdata2.write(req.data); we2.write(req.we);
                }
            } else {
                if (tfDM) sc_close_vcd_trace_file(tfDM);
                if (tfFA) sc_close_vcd_trace_file(tfFA);
                sc_stop();
                break;
            }
            request_index++;
            wait();
        }
    }
};

#endif