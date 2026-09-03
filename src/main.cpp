#include <iostream>
#include "memory_graph.hpp"

int main() {
    std::cout << "MeMo skeleton executable\n";
    // Create a graph with cold storage under ./memo_cold
    om::MemoryGraph graph("./memo_cold");
    std::cout << "MemoryGraph created with cold root ./memo_cold\n";
    return 0;
}
