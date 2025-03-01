#pragma once
///////////////////////////////////////////////////////////////////////////////
//
//	A little singly-linked FIFO list template class.
//
// 	Revised to be a priority queue.  non-decreasing order. (small to big)
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to
// joallen@trentu.ca. 		Released to the public domain. 		For more information visit
// www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////

// HEADER FILES
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

// DATA TYPES

//-----------------------------------------------------------------------------
//
// PriorityQueue Class (template)
//
template <class T, typename C = std::vector<T>> class PriorityQueue {
    C    impl_;
    bool sorted_ = false;

    void sort() {
        if (!std::exchange(sorted_, true))
            std::stable_sort(impl_.begin(), impl_.end());
    }

  public:
    // add an object onto the end of the list.
    void add(T addValue) {
        sorted_ = false;
        impl_.push_back(std::move(addValue));
    }

    // returns if the list is empty or not
    bool   isEmpty() const { return impl_.empty(); }
    size_t size() const { return impl_.size(); }

    T& operator[](size_t index) { return sort(), impl_.at(index); }

    void remove(size_t index) {
        sort();
        assert(index < impl_.size());
        impl_.erase(impl_.begin() + index);
    }

    auto        begin() { return sort(), impl_.begin(); }
    auto        end() { return impl_.end(); }
    auto const& front() const { return sort(), impl_.front(); }
};
