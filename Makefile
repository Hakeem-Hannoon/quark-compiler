# Build with `make`; run the test suite with `make test`.
# CMakeLists.txt is kept in sync for anyone who prefers cmake.

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
INCLUDES  = -Iinclude

SOURCES = src/main.cpp \
          src/Token.cpp \
          src/Lexer.cpp \
          src/Parser.cpp \
          src/Analyzer.cpp \
          src/CodeGen.cpp \
          src/AstView.cpp \
          src/AstRender.cpp

OBJECTS = $(SOURCES:src/%.cpp=build/obj/%.o)
TARGET  = build/quarkc

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET)
	@QUARKC=$(CURDIR)/$(TARGET) tests/run_tests.sh

clean:
	rm -rf build
