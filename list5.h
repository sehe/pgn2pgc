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

