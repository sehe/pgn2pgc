#include "pqueue_2.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main() {
    pgn2pgc::support::PriorityQueue<int> l;

    assert(l.isEmpty());
    for (int i = 200; i > 1; --i) {
        auto v = rand();
        l.add(v);
        if ((i % 50) == 0) { // include some duplicates
            l.add(v);
        }
        assert(!l.isEmpty());
    }

    std::cout << "First dump: " << std::endl;
    std::cout << std::endl << std::endl;
    for (auto& val : l)
        std::cout << "\t" << val;

    auto l2 = l;

    std::cout << std::endl << std::endl;
    std::cout << "Second dump: " << std::endl;
    for (auto& val : l)
        std::cout << "\t" << val;

    l.add(-1);

    auto it  = l.begin();
    auto it2 = l2.begin();
    std::cout << "\nFirst L: " << *it++;
    std::cout << "\nFirst L2: " << *it2++;

    l2  = l;
    it2 = l2.begin(); // it2 would be invalidated by the assignment
    // the old behavior of mirroring the fCurrent from `l` to `l2` is not
    // replicated here, as there is not use case for it.

    std::cout << "\nNext L: " << *it++;
}
