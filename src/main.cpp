#include <iostream>

#include "uci.hpp"


using namespace TuffFish;

int main(void) {
    
    std::cout << "TuffFish Chess Engine made by GoobusTheNoobus\n";

    UCI::loop();

    return 0;
}