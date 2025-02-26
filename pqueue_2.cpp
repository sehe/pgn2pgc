#include "pqueue_2.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main() {
  PriorityQueue<int> l;

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
  int val = -2;
  std::cout << std::endl << std::endl;
  while (l.next(&val)) {
    std::cout << "\t" << val;
  }

  PriorityQueue<int> l2(l);

  l.gotoFirst();
  assert(!l2.next(&val));

  std::cout << std::endl << std::endl;
  std::cout << "Second dump: " << std::endl;
  while (l.next(&val)) {
    std::cout << "\t" << val;
  }

  l.add(-1);

  l.gotoFirst();
  l2.gotoFirst();
  l.next(&val);
  std::cout << "\nFirst L: " << val;

  l2.next(&val);
  std::cout << "\nFirst L2: " << val;

  l2 = l;
  l2.next(&val);
  std::cout << "\nNext L2: " << val;
  l.next(&val);
  std::cout << "\nNext L: " << val;
}
