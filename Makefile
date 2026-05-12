CXX = clang++

COMMON_FLAGS = -std=c++20 -Wall -Wextra -MMD -MP

RELEASE_FLAGS = -O3 -march=native -mbmi2 -DNDEBUG
DEBUG_FLAGS = -O0 -g

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)

TARGET = tuff

-include $(DEP)

.DEFAULT_GOAL := release

# Release build
release: CXXFLAGS = $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: $(TARGET)

# Debug
debug: CXXFLAGS = $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ) $(DEP)

.PHONY: release debug clean
