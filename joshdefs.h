#ifndef J_JOSHDEFS_H
#define J_JOSHDEFS_H
///////////////////////////////////////////////////////////////////////////////
//
//	JoshDefs.h
//
//	January 1995.
//
//  Contents: Common shared data types and macros
//
//	The fact that every file includes this one can be very useful.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug-
//		reports, bug-fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////
//
//	REVISION LOG:
//
//	Date			By	Revision
//
//	1999			JTA	Removed old complex macros and functions of NewCheck and
//								replaced with CHECK_POINTER macro.  If exceptions are used
//								you can use the NEW_RETURNS_NULL macro to essentially comment
//								out CHECK_POINTER.
//  2000      JTA Added MemCheck.h, with code written mostly by Dr. Domzy.
//
///////////////////////////////////////////////////////////////////////////////

// Conditional compilation macros
//		Don't actually define them here, instead do it from within the IDE or on
//		the command line.
#include <cstdint>
#define NDEBUG			 // turn off extra debugging and checking code
#define NCAST				 // no new style casts: dynamic_cast<>, static_cast<>, etc.
#define NO_BOOL_TYPE // no bool type
//#define NNEW         // cannot overload new[]
//	new returns NULL when it can't allocate enough memory,
//		instead of throwing an exception
#define NEW_RETURNS_NULL
//#define USE_MEMCHECK // use versions of New() and Delete that check memory management //??! no longer fully trust them, as a GPE sometimes occures during heavy usage, need more investigation as to cause
//#define USE_TRACE // TRACE is a macro used during debugging to quickly output the value of an identifier to stderr

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

  void append(char ch, size_t offset = 0, size_t count = 1) {
    assert(offset == 0);
    impl_.append(count, ch);
  }
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

#include <cassert>
#include <cstdlib>
#include <iostream>

// DATA TYPES


// MACROS

//---------------------------------------------------------------------------
//	Macro:	TOBOOL
//  Author:	Joshua Allen
//
//	Arguments: x, an expression that should be either 0 or
//							non-zero (usually 1)
//
//	Purpose: Converts an expression to boolean;
//						avoids "initializing bool with int" warnings and errors
//
//---------------------------------------------------------------------------
#ifdef NO_BOOL_TYPE
#define TOBOOL(x) ((x)?true:false)
#else
#define TOBOOL(x) (x)
#endif

//---------------------------------------------------------------------------
//	Function:	CHECK_POINTER
//  Author:	Joshua Allen
//
//	Arguments:	p, a pointer
//
//	Purpose:	to assert that a pointer is not null;
//						 used after operator new
//
//	Behavior: if pointer is == null then an error message is printed to cerr
//						and the program exits;
//						does nothing otherwise
//
//	Special:	assert(0) is used in case we're in debug mode so that we can
//							see line & file info, also why a macro is used instead of a
//							function
//
//---------------------------------------------------------------------------
#ifdef NEW_RETURNS_NULL
#define CHECK_POINTER(p)                                                       \
  {                                                                            \
    if (!(p)) {                                                                \
      std::cerr << "\nError: out of dynamic memory, exiting..\n";              \
      assert(0);                                                               \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }
#else
#define CHECK_POINTER(p) ((void)0)
#endif

//-----------------------------------------------------------------------------
#ifdef USE_TRACE
#define TRACE(x) cerr<<"\n" #x ": '"<<( x )<<"'"
#else
#define TRACE(x) ((void)0)
#endif

// include at bottom so that memcheck.h will have the necessary definitions
#ifndef J_MEMCHECK_H
#include "memcheck.h" // New(), Delete()
#endif

//-----------------------------------------------------------------------------
// got idea from std::auto_ptr
template <class T>
class AdoptArray
{
public:
	AdoptArray(T* ptr) : fPtr(ptr) {}
	~AdoptArray() { DeleteArray(fPtr); fPtr = 0;}

private:
	AdoptArray(const AdoptArray&); // cannot copy
	AdoptArray& operator=(const AdoptArray&);
	T* fPtr;
};


#endif // JoshDefs.h
