# Der systemc Pfad ist in der Environment variable SYSTEMC_HOME gesetzt.
# ---------------------------------------
# CONFIGURATION START
# ---------------------------------------

# Entry point for the program and target name
MAIN := src/main.c

# Additional source files
SOURCES := src/simulation.cpp

# Header files
HEADERS := src/simulation.hpp

# Target name
TARGET := systemcc

# Path to your systemc installation from the environment variable
SCPATH := $(SYSTEMC_HOME)

# Additional flags for the compiler
CXXFLAGS := -std=c++14 -I$(SCPATH)/include
LDFLAGS := -L$(SCPATH)/lib -lsystemc -lm

# ---------------------------------------
# CONFIGURATION END
# ---------------------------------------

# Determine if clang or gcc is available
CXX := $(shell command -v g++ || command -v clang++)
ifeq ($(strip $(CXX)),)
    $(error Neither clang++ nor g++ is available. Exiting.)
endif

CC := $(shell command -v gcc || command -v clang)
ifeq ($(strip $(CC)),)
    $(error Neither clang nor gcc is available. Exiting.)
endif

# Add rpath except for MacOS
UNAME_S := $(shell uname -s)

ifneq ($(UNAME_S), Darwin)
    LDFLAGS += -Wl,-rpath=$(SCPATH)/lib
endif

# Default to release build for both app and library
all: debug

# Debug build
debug: CXXFLAGS += -g
debug: $(TARGET)

# Release build
release: CXXFLAGS += -O2
release: $(TARGET)

# Recipe for building the program
$(TARGET): $(MAIN:.c=.o) $(SOURCES:.cpp=.o)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Clean up
clean:
	rm -f $(TARGET) $(SOURCES:.cpp:.o) $(MAIN:.c:.o)

# Rule to compile .cpp files to .o files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the simulation
.PHONY: all debug release clean run
