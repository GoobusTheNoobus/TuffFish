#include <iostream>

#include "uci.hpp"


using namespace TuffChess;

int main(void) {
    std::cout << "TuffChess Chess Engine made by GoobusTheNoobus\n";

    UCI::loop();

    return 0;
}