#pragma once
///////////////////////////////////////////////////////////////////////////////
//
// A list that acts like a growable array.
// Efficient add().  Expensive locate and remove.
// 
// SEHE REVIEW: should be vector, but hard to impact analyze whether
// iterator/reference stability is depended on
//
///////////////////////////////////////////////////////////////////////////////
#include <cassert>
#include <list>

//-----------------------------------------------------------------------------
template <class T> class List {
public:
  void add(T  v) { impl_.push_back(std::move(v)); }
  void remove(size_t index) {
    assert(index < impl_.size());
    auto it = impl_.begin();
    std::advance(it, index);
    impl_.erase(it);
  }
  void makeEmpty() { impl_.clear(); }

  T &operator[](size_t index) {
    assert(index < impl_.size());
    auto it = impl_.begin();
    std::advance(it, index);
    return *it;
  }

  size_t size() const { return impl_.size(); }

private:
  std::list<T> impl_;
};

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test Test
// #define TEST
#ifdef TEST
#undef TEST

#include <iostream>
int main() {
  List<int> l;

  l.add(0);
  l.add(1);
  l.add(2);
  l.remove(1);
  l.add(3);

  for (int i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];

  if (l.size())
    l.remove(l.size() - 1);

  for (int i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];

  l.add(4);
  l.remove(0);

  for (int i = 0; i < l.size(); ++i)
    std::cout << "\n" << l[i];
}

#endif
