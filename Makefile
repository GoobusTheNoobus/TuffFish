CXX = clang++

COMMON_FLAGS = -std=c++20 -Wall -Wextra -MMD -MP

RELEASE_FLAGS = -O3 -march=native -mbmi2 -DNDEBUG
DEBUG_FLAGS = -O0 -g

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)

TARGET = tufffish_test

-include $(DEP)

.DEFAULT_GOAL := release

# Release build
release: CXXFLAGS = $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: clean $(TARGET)

# Debug
debug: CXXFLAGS = $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: clean $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: release debug clean

clean:
	rm -f $(TARGET) $(OBJ) $(DEP)