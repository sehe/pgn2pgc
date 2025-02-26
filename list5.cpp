#include "list5.h"

#include <iostream>
int main() {
  List<int> l;

  l.add(0);
  l.add(1);
  l.add(2);
  l.remove(1);
  l.add(3);

  for (unsigned i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];

  if (l.size())
    l.remove(l.size() - 1);

  for (unsigned i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];

  l.add(4);
  l.remove(0);

  for (unsigned i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];
}
