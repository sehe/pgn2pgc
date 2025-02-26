#pragma once
///////////////////////////////////////////////////////////////////////////////
//
//	A little singly-linked FIFO list template class.
//
// 	Revised to be a priority queue.  non-decreasing order. (small to big)
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to
//joallen@trentu.ca. 		Released to the public domain. 		For more information visit
//www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////

// HEADER FILES
#include <set>

// DATA TYPES

//-----------------------------------------------------------------------------
//
// PriorityQueue Class (template)
//
template <class T, typename C = std::multiset<T>> class PriorityQueue {
  C impl_;
  C::const_iterator current_ = impl_.begin();

public:
  PriorityQueue() = default;
  PriorityQueue(PriorityQueue const &rhs) : impl_(rhs.impl_) {
    for (auto l = impl_.begin(), r = rhs.impl_.begin(); l != impl_.end();
         ++l, ++r) {
      if (r == rhs.current_) {
        current_ = l;
        return;
      }
    }

    current_ = impl_.begin();
  }

  PriorityQueue &operator=(PriorityQueue rhs) {
    std::swap(impl_, rhs.impl_);
    std::swap(current_, rhs.current_);
    return *this;
  }
  // add an object onto the end of the list.
  void add(T addValue) {
    auto pos = impl_.lower_bound(addValue);
    if (pos == impl_.begin() && pos != impl_.end()) {
      // SEHE FIXME: the misguided special case for first element equivalence is
      // probably a bug in the original code
      pos = std::next(pos);
    }
    impl_.insert(pos, std::move(addValue));
    gotoFirst(); // ??!
  }

  // sets object so that next() will return the first object in the list
  void gotoFirst() { current_ = impl_.begin(); }

  // assigns the given memory location with the value of the next item
  // in the list.
  // returns false if at the end of the list true otherwise
  bool next(T *p) {
    if (current_ == impl_.end())
      return false;
    *p = *current_++;
    return true;
  }

  // returns if the list is empty or not
  bool isEmpty() const { return impl_.empty(); }
  size_t size() const { return impl_.size(); }

  T &operator[](size_t index) {
    assert(index < this->size());
    auto it = impl_.begin();
    std::advance(it, index);
    return const_cast<T &>(*it); // SEHE REVIEW safe cast?
  }

  void remove(size_t index) {
    assert(index < this->size());
    auto it = impl_.begin();
    std::advance(it, index);
    impl_.erase(it);
  }
};
