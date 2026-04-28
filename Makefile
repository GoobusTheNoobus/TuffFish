CXX = g++
CXXFlags = -std=c++20 -g -O3 -march=native -mbmi2 

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

TARGET = tuffchess

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFlags) $(OBJ) -o $(TARGET)
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm $(TARGET) $(OBJ)