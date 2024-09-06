#ifndef MODULES_HPP
#define MODULES_HPP

#include <systemc>
#include <cmath>
#include <vector>
#include <list>
#include <unordered_map>
#include <iostream>

using namespace sc_core;

extern "C" {

    struct Request {
        uint32_t addr;
        uint32_t data;
        int we;  
    };

    struct Result {
        size_t cycles;
        size_t misses;
        size_t hits;
        size_t primitiveGateCount;
    };f

    Result run_simulation(int cycles, int directMapped, unsigned cacheLines,
                          unsigned cacheLineSize, unsigned cacheLatency,
                          unsigned memoryLatency, size_t numRequests,
                          Request requests[], const char* tracefile);
};

struct CacheLine {
    bool valid;
    uint32_t tag;
    std::vector<uint8_t> data;
    CacheLine() : valid(false), tag(0), data() {}
    CacheLine(unsigned cacheLineSize) : valid(false), tag(0), data(cacheLineSize) {}
};

SC_MODULE(DirectMappedCache) {
    sc_in<bool> clk;
    sc_in<bool> enable;

    sc_in<uint32_t> address;
    sc_in<uint32_t> Wdata;
    sc_in<int> we;
    sc_out<uint32_t> Rdata;
    sc_out<size_t> cycles;
    sc_out<size_t> misses;
    sc_out<size_t> hits;
    sc_out<size_t> primitiveGateCount;
    sc_out<int> rq;

    std::unordered_map<uint32_t, uint8_t> memory;
    std::vector<CacheLine> cache;
    unsigned cacheLines;
    unsigned cacheLineSize;
    unsigned cacheLatency;
    unsigned memoryLatency;

    SC_CTOR(DirectMappedCache) {
        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void initialize(unsigned cacheLines, unsigned cacheLineSize, unsigned cacheLatency, unsigned memoryLatency) {
        this->cacheLines = cacheLines;
        this->cacheLineSize = cacheLineSize;
        this->cacheLatency = cacheLatency;
        this->memoryLatency = memoryLatency;
        cache.resize(cacheLines);  // Initialize cache lines
        for (auto& line : cache) { 
            line.data.resize(cacheLineSize); // Initialize the size of each line in the cache 
        }
    }

    void exec() {
        while (true) {

        wait();

        if (!enable.read()) continue; //to stop the execution in case of Fully Associative Cache 

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
                bool inMemory = readDataInMemory(addr, data);
                if (inMemory) {
                
                    importMemoryBlockToCache(addr, data);
                    Rdata.write(data);

                } else {
                    
                    std::cerr << "Data is not in Cache and Memory" << std::endl;
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
        //hit
        if (cache[index].valid && cache[index].tag == tag) { 
            data = cache[index].data[offset];
            primitiveGateCount.write(primitiveGateCount.read() + 10);
            return true;
        }
        return false; //miss
    }

    void writeDataInCache(uint32_t addr, uint32_t data) {
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        unsigned index = (addr / cacheLineSize) % cacheLines;
        uint32_t tag = calcTagOfDirectMapped(addr);
        uint32_t offset = addr % cacheLineSize;
        //case when the data size is bigger than 1 Byte 
        int entered = 0; //  Will be used in Write data when its 0 
        while (data > 0) {
            // Cacheline is full, we should move to the next line 
            if (offset > (cacheLineSize - 1)) {
                offset = 0;
                index++;
                primitiveGateCount.write(primitiveGateCount.read() + 2);
            }
            // Cache is full while writing the data 
            if (index > (cacheLines-1)) {
                primitiveGateCount.write(primitiveGateCount.read() + 1);
                std::cout << "Can't write the whole data. Cache is full" << std::endl;
                return;
            }
            entered++;
            cache[index].valid = true;
            cache[index].tag = tag;
            cache[index].data[offset] = data & 255; //Writes only the first byte of the data
            offset++;
            data = data >> 8; //Get rid of the first Byte 
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        // Writes the data when it = 0 
        if (entered == 0 && data == 0){
            cache[index].valid = true;
            cache[index].tag = tag;
            cache[index].data[offset] = data;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }     
    }

    void importMemoryBlockToCache (uint32_t addr, uint32_t &data){
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        unsigned index = (addr / cacheLineSize) % cacheLines;
        uint32_t tag = calcTagOfDirectMapped(addr);
        uint32_t offset = addr % cacheLineSize;
        uint32_t startAddress = addr - (addr % cacheLineSize);
        // Writing every Memory Adress Data in the corresponding offset
        for (int i = 0; i < cacheLineSize; i++) {
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
        int entered = 0; //  Will be used in Write data when its 0 
        while (data > 0){
            memory[addr + entered] = data & 255; //Writes only the first byte of the data
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
        return addr >> (indexBits+offsetBits);
    }
};

SC_MODULE(FullyAssociativeCache) {
    sc_in<bool> clk;
    sc_in<bool> enable;

    sc_in<uint32_t> address;
    sc_in<uint32_t> Wdata;
    sc_in<int> we;
    sc_out<uint32_t> Rdata;
    sc_out<size_t> cycles;
    sc_out<size_t> misses;
    sc_out<size_t> hits;
    sc_out<size_t> primitiveGateCount;
    sc_out<int> rq;
    
    
    std::list<uint32_t> tracker_lru;
    std::unordered_map<uint32_t,std::list<uint32_t>::iterator> map_with_tags;
    std::unordered_map<uint32_t, uint8_t> memory;
    std::vector<CacheLine> cache;
    unsigned cacheLines;
    unsigned cacheLineSize;
    unsigned cacheLatency;
    unsigned memoryLatency;

    SC_CTOR(FullyAssociativeCache) {
        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void initialize(unsigned cacheLines, unsigned cacheLineSize, unsigned cacheLatency, unsigned memoryLatency) {
        this->cacheLines = cacheLines;
        this->cacheLineSize = cacheLineSize;
        this->cacheLatency = cacheLatency;
        this->memoryLatency = memoryLatency;
        cache.resize(cacheLines);  // Initialize cache lines
        for (auto& line : cache) {
            line.data.resize(cacheLineSize);
        }
    }

    
    void exec() {
        while (true) {

            wait();
            if (!enable.read()) break; //to stop the execution in case of Direct Mapped Cache 
            rq.write(rq.read() + 1);
            uint32_t addr = address.read();
            uint32_t data;

            if (we.read() == 1) {
                //Write operation
                data = Wdata.read();
                writeDataInCache(addr, data);
                writeDataInMemory(addr, data);

            } else {
                // Read operation
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
                
                        std::cerr << "Data is not in Cache and Memory" << std::endl;
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
                tracker_lru.erase(map_with_tags[tag]);  //update lru when read
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
        uint32_t temp = data; //calculate the number of Bytes in the data given 
        while (temp > 0) {
            temp >>= 8;
            numBytes++;
        }
        if (numBytes == 0) numBytes = 1; //avoid the oversight of the case where the data given is 0
        while (numBytes>0){
            uint32_t offset = addr % cacheLineSize;
            uint32_t tag=calctagFullAssociative(addr);
            if (map_with_tags.find(tag) != map_with_tags.end()) { //to check if tag in cache since before
                for (CacheLine &line : cache) { //in case the tag already exists and we overwrite the old data only
                    if (line.valid && line.tag==tag) {
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        line.data[offset] = data & 255;
                        tracker_lru.erase(map_with_tags[tag]); //delete the case in the list that the iterator connnected to that tag in the map is pointing to
                        LRU_first_update(tag);    
                        primitiveGateCount.write(primitiveGateCount.read() + 1);
                        break;
                    }
                }
            } else if (tracker_lru.size() == cacheLines) { //in case cache is full and we need LRU
                uint32_t tag_to_delete = tracker_lru.back(); //get the least recently used  tag
                tracker_lru.pop_back();   //delete it from the tracker
                map_with_tags.erase(tag_to_delete);   //delete the tag to delete from the map along with its pointer to iterator position 
                primitiveGateCount.write(primitiveGateCount.read() + 1);
                for (CacheLine &line:cache){
                    if (line.valid && line.tag == tag_to_delete){ //looping to find the deleted line to replace it with new data and tag
                        line.tag=tag;
                        line.data[offset]= data & 255;
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        LRU_first_update(tag);
                        primitiveGateCount.write(primitiveGateCount.read() + 2);
                        break;
                    }
                }
            } else {
                for (CacheLine &line:cache){
                    //in case cache not full 
                    if(!line.valid){
                        line.valid=1;
                        line.tag=tag;
                        line.data[offset]= data & 255;
                        primitiveGateCount.write(primitiveGateCount.read() + 10);
                        LRU_first_update(tag);
                        primitiveGateCount.write(primitiveGateCount.read() + 2);
                        break;
                    }
                }
            }
            data = data >> 8;
            numBytes--;
            addr = addr + 1;  //add 1 to get the next chunk in the next offset or potentially another line when line is full
            primitiveGateCount.write(primitiveGateCount.read() + 3);
        }
    }

    void importMemoryBlockToCache (uint32_t addr, uint32_t &data){
        primitiveGateCount.write(primitiveGateCount.read() + 4);
        uint32_t startAddress = address - (address % cacheLineSize);
        uint32_t tag = calctagFullAssociative(addr);
        uint32_t offset = address % cacheLineSize;
        uint32_t tag_to_delete = tracker_lru.back();
        tracker_lru.pop_back();  
        map_with_tags.erase(tag_to_delete);
        for (CacheLine &line:cache){
            if (line.tag == tag_to_delete){
                for (int i = 0; i < cacheLineSize; i++) {
                    line.data[i] = memory[startAddress + i];
                    primitiveGateCount.write(primitiveGateCount.read() + 1);
                }
            primitiveGateCount.write(primitiveGateCount.read() + 10);
            line.tag = tag;
            tracker_lru.push_front(tag);
            primitiveGateCount.write(primitiveGateCount.read() + 20);
            map_with_tags[tag]=tracker_lru.begin();
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
        while (data > 0){
            memory[addr+entered] = data & 255;
            entered++;
            data = data >> 8;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
        if (entered == 0 && data == 0) {
            memory[addr] = data;
            primitiveGateCount.write(primitiveGateCount.read() + 10);
        }
    }
    
    void LRU_first_update(uint32_t tag){
        tracker_lru.push_front(tag); //new tag added as most recently used 
        primitiveGateCount.write(primitiveGateCount.read() + 20);
        map_with_tags[tag]=tracker_lru.begin(); //map updated for that tag so that it has a pointer pointing to the tag's position in the lru_tracker
    }
    uint32_t calctagFullAssociative(uint32_t addr) {
        primitiveGateCount.write(primitiveGateCount.read() + 2);
        uint32_t offsetBits = log2(cacheLineSize);
        return addr >> offsetBits;
    }
};

SC_MODULE(Simulation) {
    sc_in<bool> clk;
    sc_signal<bool> enable1;  
    sc_signal<bool> enable2;  

    sc_signal<uint32_t> address1;
    sc_signal<uint32_t> Wdata1;
    sc_signal<uint32_t> Rdata1;
    sc_signal<int> we1;
    sc_signal<size_t> cycles1;
    sc_signal<size_t> misses1;
    sc_signal<size_t> hits1;
    sc_signal<size_t> primitiveGateCount1;
    sc_signal<int> rq1;

    sc_signal<uint32_t> address2;
    sc_signal<uint32_t> Wdata2;
    sc_signal<uint32_t> Rdata2;
    sc_signal<int> we2;
    sc_signal<size_t> cycles2;
    sc_signal<size_t> misses2;
    sc_signal<size_t> hits2;
    sc_signal<size_t> primitiveGateCount2;
    sc_signal<int> rq2;

    size_t numRequests;
    struct Request* requests;
    const char* tracefile;
    int directMapped;

    DirectMappedCache& directMappedCache;
    FullyAssociativeCache& fullyAssociativeCache;

    void initialize(size_t numRequests, struct Request requests[], const char* tracefile, int directMapped) {
        this->numRequests = numRequests;
        this->requests = requests;
        this->tracefile = tracefile;
        this->directMapped = directMapped;
    }
    SC_CTOR(Simulation);
    Simulation(sc_module_name name, DirectMappedCache& directMappedCache, FullyAssociativeCache& fullyAssociativeCache) : directMappedCache(directMappedCache), fullyAssociativeCache(fullyAssociativeCache) {
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

        cycles1.write(0);
        misses1.write(0);
        hits1.write(0);
        rq1.write(0);

        cycles2.write(0);
        misses2.write(0);
        hits2.write(0);
        rq2.write(0);

        SC_THREAD(exec);
        sensitive << clk.pos();
    }

    void exec() {
        size_t request_index = 0;
        enable1.write(directMapped == 1);
        enable2.write(directMapped == 0);

        sc_trace_file* traceDirectMapped = NULL;
        sc_trace_file* traceFullyAssociative = NULL;


        if (directMapped && tracefile != NULL) {
        if (traceDirectMapped == NULL) {
            traceDirectMapped = sc_create_vcd_trace_file(tracefile);
            if (traceDirectMapped == NULL) {
                std::cerr << "Failed to create trace file for direct-mapped: " << tracefile << std::endl;
            } else {
                std::cout << "Direct-mapped trace file created: " << tracefile << std::endl;
                sc_trace(traceDirectMapped, clk, "clk");
                sc_trace(traceDirectMapped, address1, "address");
                sc_trace(traceDirectMapped, Wdata1, "Wdata");
                sc_trace(traceDirectMapped, Rdata1, "Rdata");
                sc_trace(traceDirectMapped, we1, "we");
                sc_trace(traceDirectMapped, cycles1, "cycles");
                sc_trace(traceDirectMapped, misses1, "misses");
                sc_trace(traceDirectMapped, hits1, "hits");
                sc_trace(traceDirectMapped, primitiveGateCount1, "primitiveGateCount");
                sc_trace(traceDirectMapped, rq1, "rq");
            }
        } else {
            std::cerr << "Direct-mapped trace file already initialized." << std::endl;
        }
    } else if (!directMapped && tracefile != NULL) {
        if (traceFullyAssociative == NULL) {
            traceFullyAssociative = sc_create_vcd_trace_file(tracefile);
            if (traceFullyAssociative == NULL) {
                std::cerr << "Failed to create trace file for fully associative: " << tracefile << std::endl;
            } else {
                std::cout << "Fully associative trace file created: " << tracefile << std::endl;
                sc_trace(traceFullyAssociative, clk, "clk");
                sc_trace(traceFullyAssociative, address2, "address");
                sc_trace(traceFullyAssociative, Wdata2, "Wdata");
                sc_trace(traceFullyAssociative, Rdata2, "Rdata");
                sc_trace(traceFullyAssociative, we2, "we");
                sc_trace(traceFullyAssociative, cycles2, "cycles");
                sc_trace(traceFullyAssociative, misses2, "misses");
                sc_trace(traceFullyAssociative, hits2, "hits");
                sc_trace(traceFullyAssociative, primitiveGateCount2, "primitiveGateCount");
                sc_trace(traceFullyAssociative, rq2, "rq");
            }
        } else {
            std::cerr << "Fully associative trace file already initialized." << std::endl;
        }
    }

        while (true) {

            if (request_index < numRequests) {
                Request request = requests[request_index];
                if (directMapped) {
                    address1.write(request.addr);
                    Wdata1.write(request.data);
                    we1.write(request.we);
                } else {
                    address2.write(request.addr);
                    Wdata2.write(request.data);
                    we2.write(request.we);
                }
            } else {
                if (directMapped && tracefile != NULL && traceDirectMapped != NULL) {
                    sc_close_vcd_trace_file(traceDirectMapped);
                }
                if (!directMapped && tracefile != NULL && traceFullyAssociative != NULL) {
                    sc_close_vcd_trace_file(traceFullyAssociative);
                }
                sc_stop();
                break;
            }
            request_index++;
            wait();
            
        }
    }
};

#endif // MODULES_HPP