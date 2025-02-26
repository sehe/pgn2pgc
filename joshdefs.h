#ifndef J_JOSHDEFS_H
#define J_JOSHDEFS_H
///////////////////////////////////////////////////////////////////////////////
//
//  JoshDefs.h
//
//  January 1995.
//
//  Contents: Common shared data types and macros
//
//  The fact that every file includes this one can be very useful.
//
//  ABOUT THIS FILE: Please send any questions, comments, suggestions, bug-
//      reports, bug-fixes, and useful modifications to joallen@trentu.ca.
//      Released to the public domain.
//      For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////
//
//  REVISION LOG:
//
//  Date  By  Revision
//
//  1999  JTA Removed old complex macros and functions of NewCheck and
//            replaced with CHECK_POINTER macro.  If exceptions are used
//            you can use the NEW_RETURNS_NULL macro to essentially comment
//            out CHECK_POINTER.
//  2000  JTA Added MemCheck.h, with code written mostly by Dr. Domzy.
//  2025  sehe major cleanup, modernization, and simplification
//
///////////////////////////////////////////////////////////////////////////////

// sehe: TODO remove stand-in for non-standard Borland string type
// interface hints from
// https://sourceforge.net/p/owlnext/wiki/Replacing_the_Borland_C++_Class_Libraries/
#include <cassert>
#include <cstddef>
#include <iomanip>
#include <string>
#include <string_view>
struct string {
  string() = default;
  string(char const *sz) : impl_(sz) {}
  string(char ch) : impl_(1ul, ch) {}
  string(std::string s) : impl_(std::move(s)) {}

  size_t length() const { return impl_.length(); }
  bool operator<(string const &rhs) const { return impl_ < rhs.impl_; }
  bool operator>(string const &rhs) const { return impl_ > rhs.impl_; }
  bool operator==(string const &rhs) const { return impl_ == rhs.impl_; }
  bool operator!=(string const &rhs) const { return impl_ != rhs.impl_; }
  char &get_at(size_t i) { return impl_[i]; }
  char const &get_at(size_t i) const { return impl_[i]; }
  bool is_null() const { return impl_.empty(); }
  char const* c_str() const { return impl_.c_str(); }

  void append(char ch) { impl_.append(1ul, ch); }
  void append(string const &s) { impl_.append(s.impl_); }

  void insert(size_t pos, char ch) { impl_.insert(pos, 1, ch); }
  void insert(size_t pos, string const& s) { impl_.insert(pos, s.impl_); }
  //void prepend(string const& s) { impl_.insert(0, s.impl_); }
  //void remove(size_t pos) { impl_.erase(pos); }
  //bool contains(string const& pat) const { return impl_.find(pat.impl_) != std::string::npos; }
  //void to_upper() { for (auto &ch : impl_) ch = std::toupper(static_cast<std::uint8_t>(ch)); }
  //void to_lower() { for (auto &ch : impl_) ch = std::tolower(static_cast<std::uint8_t>(ch)); }
  void strip() {
    static constexpr auto charset = " "; // ?? " \t\n\r";
    auto start = impl_.find_first_not_of(charset);
    auto end = impl_.find_last_not_of(charset);
    if (start == std::string::npos) {
      impl_.clear();
    } else {
      impl_ = impl_.substr(start, end - start + 1);
    }
  }

  operator std::string_view() const { return impl_; }
  operator std::string const &() const { return impl_; }
  std::string const& str() const { return impl_; }

  friend string operator+(string const &lhs, string const &rhs) {
    return lhs.impl_ + rhs.impl_;
  }

private:
  std::string impl_;

  friend std::ostream &operator<<(std::ostream &os, string const &s) { return os << s.impl_; }
  friend std::istream &operator>>(std::istream &is, string &s) { return is >> s.impl_; }
  friend std::istream &getline(std::istream &is, string &s, char delim = '\n') {
    return std::getline(is, s.impl_, delim);
  }
  friend auto quoted(string const &s, char delim = '"') {
    return std::quoted(s.impl_, delim);
  }
};

// HEADER FILES
#include <memory>
template <class T> using AdoptArray = std::unique_ptr<T[]>;

#endif // JoshDefs.h
