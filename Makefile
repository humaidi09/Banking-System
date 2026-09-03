# Banking System — build rules
#
#   make            build the ./bank binary
#   make run        build, then run it
#   make test       build and run the unit test suite
#   make clean      remove build artefacts and test leftovers

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Isrc
LDFLAGS  ?=
TARGET   := bank
TEST_BIN := run_tests

# On Windows, link the C++ runtime statically. GitHub's runners (and many dev
# machines) have several libstdc++-6.dll versions on PATH; a dynamically linked
# binary can load the wrong one and crash at startup. A static link removes the
# external dependency entirely and makes the .exe self-contained.
ifeq ($(OS),Windows_NT)
    LDFLAGS += -static -static-libgcc -static-libstdc++
endif

SOURCES      := $(wildcard src/*.cpp)
LIB_SOURCES  := $(filter-out src/main.cpp,$(SOURCES))
TEST_SOURCES := $(wildcard tests/*.cpp)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

# Tests link every module except main.cpp, which owns its own entry point.
$(TEST_BIN): $(LIB_SOURCES) $(TEST_SOURCES)
	$(CXX) $(CXXFLAGS) -Itests $(LIB_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TARGET) $(TARGET).exe $(TEST_BIN) $(TEST_BIN).exe src/*.o test_bank_*.txt *.tmp
