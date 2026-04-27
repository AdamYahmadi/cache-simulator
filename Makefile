# -------------------------------------------------------------------
# InfraLens: Cache Simulator Build System
# -------------------------------------------------------------------
# SystemC path must be set in the environment variable SYSTEMC_HOME.

# --- Project Configuration ---
TARGET  := systemcc
MAIN    := src/main.c
SOURCES := src/simulation.cpp
HEADERS := src/simulation.hpp
SCPATH  := $(SYSTEMC_HOME)

# --- Compiler & Linker Flags ---
CXXFLAGS := -std=c++14 -I$(SCPATH)/include
LDFLAGS  := -L$(SCPATH)/lib -lsystemc -lm

# --- Environment Detection ---
CXX := $(shell command -v g++ || command -v clang++)
CC  := $(shell command -v gcc || command -v clang)

# Fail-safe check for compilers
ifeq ($(strip $(CXX)),)
    $(error Error: No C++ compiler (g++/clang++) found in PATH.)
endif
ifeq ($(strip $(CC)),)
    $(error Error: No C compiler (gcc/clang) found in PATH.)
endif

# OS-specific linker adjustments (rpath for Linux, excluded for Darwin/macOS)
UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S), Darwin)
    LDFLAGS += -Wl,-rpath=$(SCPATH)/lib
endif

# --- Build Targets ---
.PHONY: all debug release clean run

# Default target
all: debug

# Debug build: includes symbols for GDB
debug: CXXFLAGS += -g
debug: $(TARGET)

# Release build: optimizes for performance
release: CXXFLAGS += -O2
release: $(TARGET)

# --- Linker Recipe ---
$(TARGET): $(MAIN:.c=.o) $(SOURCES:.cpp=.o)
	$(CXX) -o $@ $^ $(LDFLAGS)

# --- Compilation Rules ---

# C++ Source Compilation
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# C Source Compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Maintenance ---
clean:
	@echo "Cleaning project..."
	rm -f $(TARGET) src/*.o