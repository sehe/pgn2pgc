///////////////////////////////////////////////////////////////////////////////
//
//	Compile and link in this file if USE_MEMCHECK is defined.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////

#include "joshdefs.h"
#include "memcheck.h"

#ifdef USE_MEMCHECK 

const char gStartupInfoFileName[] = {STARTUP_INFO_FILE_NAME};

#include <assert.h>   // assert
#include <ctype.h>    // is...
#include <fstream.h>  // ifstream, ofstream
#include <iomanip.h>  // setfill, setw
#include <iostream.h> // ostream, <<, >>, dec, endl, flush, hex, oct
#include <math.h>     // fmod, sqrt
#include <new.h>      // new
#include <stddef.h>   // size_t
#include <stdlib.h>   // abort
#include <string.h>   // strcat, strcmp, strcpy, strlen, strncpy
#include <time.h>     // CLOCKS_PER_SEC, clock_t, time_t, clock, ctime, time

// Global static variables
static Startup gStartup(gStartupInfoFileName);

// =======
// Address
// =======

class Address
	{

		friend ostream& operator << (ostream&, const Address&);

	public:

		Address (void*);

		// Default copy, destructor, and assignment.
		// Address (const Address&);
		// ~Address ();
		// Address& operator = (const Address&);

		bool operator == (const Address&) const;
		bool operator != (const Address&) const;

		void* operator () () const;

		Address operator + (size_t) const;
		Address operator - (size_t) const;

		bool checkSentinel (size_t userSize, size_t sentinelSize, unsigned long sentinelInit) const;
		void initMemory    (size_t size, unsigned long init, bool isArray, size_t sentinelSize, unsigned long sentinelInit) const;

	private:

		unsigned char* _address;
	};

// -------
// Friends
// -------

ostream& operator << (ostream& lhs, const Address& rhs)
	{

#ifndef NCAST
		lhs << hex << reinterpret_cast<int*>(rhs._address) << dec;
#else
		lhs << hex << (int*)(rhs._address) << dec;
#endif // NCAST

		return lhs;
	}

// ------------
// Constructors
// ------------

Address::Address (void* address) :
#ifndef NCAST
_address (static_cast<unsigned char*>(address))
#else
_address ((unsigned char*)(address))
#endif // NCAST
{}

// ---------
// Operators
// ---------

bool Address::operator == (const Address& rhs) const
	{
		return TOBOOL(_address == rhs._address);
	}

bool Address::operator != (const Address& rhs) const
	{
		return TOBOOL(_address != rhs._address);
	}

void* Address::operator () () const
	{
		return _address;
	}

Address Address::operator + (size_t rhs) const
	{
		return Address(_address + rhs);
	}

Address Address::operator - (size_t rhs) const
	{
		return Address(_address - rhs);
	}

// -------
// Methods
// -------

bool Address::checkSentinel (size_t userSize, size_t sentinelSize, unsigned long sentinelInit) const
	{
		bool                       isCheck         = true;
		const unsigned char* const sentinelAddress = _address + userSize;

		for (size_t i = 0; i < sentinelSize; ++i)
			{

				#ifndef NCAST
				if (sentinelAddress[i] != (reinterpret_cast<unsigned char*>(&sentinelInit))[i % 4])
						isCheck = false;
				#else
				if (sentinelAddress[i] != ((unsigned char*)(&sentinelInit))[i % 4])
						isCheck = false;
				#endif // NCAST
			}

		return isCheck;
	}

void Address::initMemory (size_t userSize, unsigned long init, bool isArray, size_t sentinelSize,
unsigned long sentinelInit) const
	{

		// Initialize the data.
		for (size_t i = 0; i < userSize; ++i)
			{
#ifndef NCAST
				_address[i] = (reinterpret_cast<unsigned char*>(&init))[i % 4];
#else
				_address[i] = ((unsigned char*)(&init))[i % 4];
#endif // NCAST
			}

// Initialize the sentinel.
		if (isArray)
			{
				for (size_t i = userSize; i < (userSize + sentinelSize); ++i)
					{
						#ifndef NCAST
						_address[i] = (reinterpret_cast<unsigned char*>(&sentinelInit))[(i - userSize) % 4];
						#else
						_address[i] = ((unsigned char*)(&sentinelInit))[(i - userSize) % 4];
						#endif // NCAST
					}
			}
	}

// ===========
// CheckerNode
// ===========

class CheckerNode
	{

		friend ostream& operator << (ostream&, const CheckerNode&);

	public:

		CheckerNode (const Address&, size_t userSize, const char* newFile, int newLine, bool isArray, size_t sentinelSize,
						CheckerNode*);

		// Default destructor.
		// ~CheckerNode ();

		bool checkArrayness (bool isArray) const;
		bool checkSentinel  (unsigned long sentinelInit) const;

		size_t userSize   ()    const;

		const CheckerNode* addressEntry (const Address&) const;
		CheckerNode*       entry        (const Address&);
		CheckerNode*       nextEntry    ();

		void setDelete (const char* deleteFile, int deleteLine);

	private:
		static int minimum (int, int);

	private:
		// Disallow copy and assignment.
		CheckerNode (const CheckerNode&);
		CheckerNode& operator = (const CheckerNode&);

	private:
		enum {_fileSize = 13};
		bool _isArray;

		char _deleteFile[_fileSize + 1];
		char _newFile[_fileSize + 1];

		int _deleteLine;
		int _newLine;

		size_t _sentinelSize;
		size_t _userSize;

		Address      _address;
		CheckerNode* _nextEntry;
	};

// -------
// Friends
// -------

ostream& operator << (ostream& lhs, const CheckerNode& rhs)
	{
		lhs << "#                    New                       Delete"                          << endl;
		lhs << "# Address     Size   File               Line   File               Line   Array" << endl;
		lhs << "# ----------  -----  -----------------  -----  -----------------  -----  -----" << endl;

		lhs << "# ";
		lhs << setw(10) << rhs._address;
		lhs << setw(2)  << "  ";
		lhs << setw(5)  << rhs._userSize;
		lhs << setw(2)  << "  ";
		lhs << setw(17) << rhs._newFile;
		lhs << setw(2)  << "  ";
		lhs << setw(5)  << rhs._newLine;
		lhs << setw(2)  << "  ";
		lhs << setw(17) << rhs._deleteFile;
		lhs << setw(2)  << "  ";
		lhs << setw(5)  << rhs._deleteLine;
		lhs << setw(2)  << "  ";
		lhs << setw(5)  << (rhs._isArray ? "  yes" : "   no") << endl;

		return lhs;
}


// ------------
// Constructors
// ------------

CheckerNode::CheckerNode (const Address& address, size_t userSize, const char* newFile, int newLine, bool isArray,
size_t sentinelSize, CheckerNode* nextEntry) :

    _isArray      (isArray),
    _sentinelSize (sentinelSize),
    _userSize     (userSize),
    _address      (address),
    _nextEntry    (nextEntry)
    {

        assert(newFile);
        
        // Set the new file and line.
        const int length = CheckerNode::minimum(strlen(newFile), _fileSize);
        strncpy(_newFile, newFile, length);
        _newFile[length] = '\0';
        _newLine = newLine;
        
        // Set the delete file and line.
        strcpy(_deleteFile, "");
        _deleteLine = 0;
    }

// -------
// Methods
// -------

const CheckerNode* CheckerNode::addressEntry (const Address& address) const
    {
        const CheckerNode* tmpEntry = this;
        
        while (tmpEntry && (tmpEntry->_address != address))
        tmpEntry = tmpEntry->_nextEntry;
        
        return tmpEntry;}

        CheckerNode* CheckerNode::nextEntry () {
        return _nextEntry;
    }

void CheckerNode::setDelete (const char* deleteFile, int deleteLine)
    {
        assert(deleteFile);
        const int length = CheckerNode::minimum(strlen(deleteFile), _fileSize);
        strncpy(_deleteFile, deleteFile, length);
        _deleteFile[length] = '\0';
        _deleteLine = deleteLine;
    }

size_t CheckerNode::userSize () const
    {
        return _userSize;
    }


bool CheckerNode::checkArrayness (bool isArray) const
    {
        return TOBOOL(_isArray == isArray);
    }

bool CheckerNode::checkSentinel (unsigned long sentinelInit) const
		{
        return TOBOOL(!_isArray || _address.checkSentinel(_userSize, _sentinelSize, sentinelInit));
    }

CheckerNode* CheckerNode::entry (const Address& address)
    {
        CheckerNode* tmpEntry = this;
        
        while (tmpEntry && ((tmpEntry->_address != address) || tmpEntry->_deleteLine))
        tmpEntry = tmpEntry->_nextEntry;
        
        return tmpEntry;
    }

int CheckerNode::minimum (int i, int j)
    {
        if (i < j)
        return i;
        else
        return j;
    }

// =======
// Checker
// =======

class Checker {
  public:
    Checker (ofstream& ferr);
    ~Checker ();
    
    void check (const Address&, const char* deleteFile, int deleteLine, bool isArray);
    
    void record (const Address&, size_t userSize, size_t registerSize, const char* newFile, int newLine, bool isArray,
    size_t objectSize);
  
  private:
    // Disallow copy and assignment.
    Checker (const Checker&);
		Checker& operator = (const Checker&);
  
  private:
    static const unsigned long _newInit;
    static const unsigned long _sentinelInit;
    
    private:
    unsigned long _userByteCount;
    unsigned long _maxUserByteCount;
    unsigned long _totalUserByteCount;
    unsigned long _totalRegisterByteCount;
    
    int          _countEntry;
		ofstream&    _ferr;
    CheckerNode* _headEntry;
  };

// -----------
// Static Data
// -----------

const unsigned long Checker::_newInit      = 0xBABECAFEuL;
const unsigned long Checker::_sentinelInit = 0xDEADBEEFuL;

// ------------
// Constructors
// ------------

Checker::Checker (ofstream& ferr) :
_userByteCount          (0),
_maxUserByteCount       (0),
_totalUserByteCount     (0),
_totalRegisterByteCount (0),
_countEntry             (0),
_ferr                   (ferr),
_headEntry              (0)
  {

    // Get the start time.
		time_t currentTimeValue;
    time(&currentTimeValue);
    
    // Convert the start time into a string.
    char* const currentTimeString = ctime(&currentTimeValue);
    currentTimeString[strlen(currentTimeString) - 1] = '\0';

		_ferr << "# Startup v5.1 started on " << currentTimeString << "." << endl;
		_ferr << hex;

		#ifdef __BCPLUSPLUS__
		_ferr << "# __BCPLUSPLUS__ = " << __BCPLUSPLUS__ << endl << endl;
		#endif // __BCPLUSPLUS__

		#ifdef __TCPLUSPLUS__
		_ferr << "# __TCPLUSPLUS__ = " << __TCPLUSPLUS__ << endl << endl;
		#endif // __TCPLUSPLUS__

    #ifdef __GNUG__
    _ferr << "# __GNUG__ = " << __GNUG__ << endl << endl;
    #endif // __GNUG__
    
    #ifdef __IBMCPP__
		_ferr << "# __IBMCPP__ = " << __IBMCPP__ << endl << endl;
    #endif // __IBMCPP__
    
		#ifdef __MSC__
		_ferr << "# __MSC__ = " << __MSC__ << endl << endl;
		#endif // __MSC__

		#ifdef __MWERKS__
		_ferr << "# __MWERKS__ = " << __MWERKS__ << endl << endl;
		#endif // __MWERKS__

		#ifdef __SC__
		_ferr << "# __SC__ = " << __SC__ << endl << endl;
		#endif // __SC__

		#ifdef __sunpro_c
		_ferr << "# __sunpro_c = " << __sunpro_c << endl << endl;
		#endif // __sunpro_c

		_ferr << dec;

    #ifdef NO_BOOL_TYPE
    _ferr << "# NO_BOOL_TYPE   = true"  << endl;
    #else
    _ferr << "# NO_BOOL_TYPE   = false" << endl;
    #endif // NO_BOOL_TYPE
    
    #ifdef NCAST
    _ferr << "# NCAST   = true"  << endl;
		#else
    _ferr << "# NCAST   = false" << endl;
    #endif // NCAST

		#ifdef NDEBUG
	 _ferr << "# NDEBUG  = true"  << endl;
	 #else
	 _ferr << "# NDEBUG  = false" << endl;
	 #endif // NDEBUG

    #ifdef NDELETE
    _ferr << "# NDELETE = true"  << endl << endl;
    #else
		_ferr << "# NDELETE = false" << endl << endl;
    #endif // NDELETE
    
    #ifdef NNEW
		_ferr << "# NNEW = true"  << endl << endl;
    #else
    _ferr << "# NNEW = false" << endl << endl;
    #endif // NNEW
  }

// ----------
// Destructor
// ----------

Checker::~Checker ()
	{
		// Verify that the byte count is zero.
		if (_userByteCount)
			{
				cerr << endl << "# Aborted!  Check file '"
					<< gStartupInfoFileName << "' for details. " << endl;

				_ferr << "# Program terminated with a non-zero byte count of ";
				_ferr << _userByteCount << "." << endl << endl;
				_ferr << "# Aborted!" << endl;

				abort();
			}

		// Print the max user byte count.
		_ferr << "# Max   user     byte count = " << _maxUserByteCount << endl;

		// Print the total user byte count.
		_ferr << "# Total user     byte count = " << _totalUserByteCount << endl;

		// Print the total register byte count.
		_ferr << "# Total register byte count = " << _totalRegisterByteCount << endl << endl;

		const unsigned long totalEntryByteCount = _countEntry * sizeof(CheckerNode);
		const unsigned long totalByteCount      = _totalRegisterByteCount + totalEntryByteCount;

		// Print the entries used.
		_ferr << "# Entries used           = " << _countEntry << endl;

		// Print the sizeof Entry.
		_ferr << "# Size of entry          = " << sizeof(CheckerNode) << endl;

		// Print the total entry byte count.
		_ferr << "# Total entry byte count = " << totalEntryByteCount << endl << endl;

		// Print the total byte count.
		_ferr << "# Total byte count = " << totalByteCount << endl;

		// Destroy the entries.

		while (_headEntry)
			{
				--_countEntry;

				CheckerNode* tmpEntry = _headEntry;
				_headEntry = _headEntry->nextEntry();

				delete tmpEntry;
				tmpEntry = 0;
			}

		_ferr << "# Finished." << endl;
	}

// -------
// Methods
// -------

void Checker::check (const Address& address, const char* deleteFile, int deleteLine, bool isArray)
  {
    assert(deleteFile);
    
    // Check to see if the address is zero.
		if (address())
      {
        // Find the address to be deleted.
        const CheckerNode* const addressEntry = (_headEntry ? _headEntry->addressEntry(address) : 0);

        CheckerNode* const tmpEntry = (_headEntry ? _headEntry->entry(address) : 0);
        
        // Verify that the address is not already deleted.
        if (addressEntry && !tmpEntry)
          {
            cerr << endl << "# Aborted!   Check file '"
                      << gStartupInfoFileName << "' for details. " << endl;
            

            _ferr << *addressEntry << endl;
            _ferr << "# Deleting the already deleted address " << address << "." << endl;
            _ferr << "# Deleted in file " << deleteFile;
						_ferr << " on line "          << deleteLine << "." << endl << endl;
            _ferr << "# Aborted!" << endl;
            abort();
            assert(0); // not_reached
          }
        
        // Verify that the address is legal.
        else if (!tmpEntry)
          {
						cerr << endl << "# Aborted!  Check file '"
                      << gStartupInfoFileName << "' for details. " << endl;
            
            _ferr << "# Deleting the illegal address " << address << "." << endl;
						_ferr << "# Deleted in file " << deleteFile;
            _ferr << " on line "          << deleteLine << "." << endl << endl;
            _ferr << "# Aborted!" << endl;
            abort();
            assert(0); // not_reached
          }
    
        // Verify that an array was deleted with DeleteArray,
        // and that a non-array was deleted with Delete.
				else if (!tmpEntry->checkArrayness(isArray))
          {
            cerr << endl << "# Aborted!  Check file '"
                      << gStartupInfoFileName << "' for details. " << endl;

            _ferr << *tmpEntry << endl;
            
            if (isArray)
            _ferr << "# Using DeleteArray on the non-array address " << address << "." << endl;
            else
            _ferr << "# Using Delete on the array address " << address << "." << endl;
            
            _ferr << "# Deleted in file " << deleteFile;
						_ferr << " on line "          << deleteLine << "." << endl << endl;
            _ferr << "# Aborted!" << endl;
            abort();
          }
				// Verify that the sentinel is undamaged.
        else if (!tmpEntry->checkSentinel(Checker::_sentinelInit))
          {
            cerr << endl << "# Aborted!  Check file '"
                      << gStartupInfoFileName << "' for details. " << endl;
            
            _ferr << *tmpEntry << endl;
            _ferr << "# Damaged sentinel at address " << address << "." << endl;
            _ferr << "# Deleted in file " << deleteFile;
						_ferr << " on line "          << deleteLine << "." << endl << endl;
            _ferr << "# Aborted!" << endl;
            abort();
          }
				else
          {
            // Decrement the byte count.
            _userByteCount -= tmpEntry->userSize();
            
            // Set the filename and line number where delete occurred.
            tmpEntry->setDelete(deleteFile, deleteLine);
          }
      }
	}
        
void Checker::record (const Address& address, size_t userSize, size_t registerSize, const char* newFile, int newLine, bool isArray,
        size_t objectSize)
	{
    
    assert(newFile);
    
    // Increment the user byte count.
    _userByteCount += userSize;
    
    // Increment the max user byte count.
    if (_userByteCount > _maxUserByteCount)
			 _maxUserByteCount = _userByteCount;
    
    // Increment the total user byte count.
    _totalUserByteCount += userSize;

    // Increment the total register byte count.
    _totalRegisterByteCount += registerSize;
    
    // Initialize memory.
    address.initMemory(userSize, Checker::_newInit, isArray, objectSize, Checker::_sentinelInit);
    
    // Create the entry.
    ++_countEntry;
		_headEntry = new CheckerNode(address, userSize, newFile, newLine, isArray, objectSize, _headEntry);
  }

// =======
// Startup
// =======

// -------------------
// Static data members
// -------------------

int       Startup::_count;
ofstream* Startup::_ferr;
Checker*  Startup::_checker;

// ------------
// Constructors
// ------------

// only one startup object can really be created.
Startup::Startup (const char* errFile)
	{
		assert(errFile);

		if (!Startup::_count++)
			{

				// Create the error file stream.
				Startup::_ferr = new ofstream(errFile);

				if (!*Startup::_ferr)
					{
						cerr << "# Error: Could not open '" << errFile << "'." << endl;
						abort();
					}

				// Create the checker.
				assert(*Startup::_ferr);
				Startup::_checker = new Checker(*Startup::_ferr);
			}
	}

// ----------
// Destructor
// ----------

Startup::~Startup ()
  {
    if (!--Startup::_count)
      {
        // Destroy the checker.
        delete Startup::_checker;

        // Destroy the error file stream.
        delete Startup::_ferr;
      }
	}

// --------------
// Static Methods
// --------------

void Startup::check (void* p, const char* deleteFile, int deleteLine, bool isArray)
  {
    assert(deleteFile);
		assert(Startup::_checker);
    Startup::_checker->check(Address(p), deleteFile, deleteLine, isArray);
  }
    
void Startup::record (void* p, size_t userSize, size_t registerSize, const char* newFile, int newLine, bool isArray,
  size_t objectSize)
  {
    assert(p);
    assert(newFile);
    assert(Startup::_checker);
    Startup::_checker->record(Address(p), userSize, registerSize, newFile, newLine, isArray, objectSize);
  }

// =============
// new and new[]
// =============

void* operator new (size_t size, const char* newFile, int newLine, bool isArray, int iCount_, size_t objectSize)
  {
    assert(newFile);
    
    // Create room for the sentinel.
    const size_t newSize = (isArray ? (size + objectSize) : size);
    
    // Invoke the global new operator.
    void* const newMemory = ::operator new(newSize);
		assert(newMemory);
    
    // Increment the address if this is an object array.
    const size_t countSize    = (isArray ? (size - (iCount_ * objectSize)) : 0);
		const size_t registerSize = newSize - countSize;
    const size_t userSize     = size    - countSize;
    
    const Address newAddress(newMemory);
    const Address registerAddress(newAddress + countSize);
    
    // Register the entry.
    Startup::record(registerAddress(), userSize, registerSize, newFile, newLine, isArray, objectSize);
    
		return newAddress();
  }

#ifndef NNEW
void* operator new [] (size_t size, const char* newFile, int newLine, bool, int iCount_, size_t objectSize)
  {
	 assert(newFile);

	 // Create room for the sentinel.
	 const size_t newSize = size + objectSize;

	 // Invoke the global new array operator.
	 void* const newMemory = ::operator new[](newSize);
		assert(newMemory);

	 // Increment the address.
	 const size_t countSize    = size    - (iCount_ * objectSize);
		const size_t registerSize = newSize - countSize;
	 const size_t userSize     = size    - countSize;

	 const Address newAddress(newMemory);
	 const Address registerAddress(newAddress + countSize);

	 // Register the entry.
	 Startup::record(registerAddress(), userSize, registerSize, newFile, newLine, true, objectSize);

		return newAddress();
  }
#endif // NNEW

#endif // USE_MEMCHECK

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test Test
//#define TEST
#ifdef TEST
#undef TEST

#include <stdlib.h>

int main()
	{
		cout << "\n\nTesting memory checking " << time(0) << "\n\n";
#ifdef USE_MEMCHECK
		cout << "Sending output to file: " << gStartupInfoFileName;
#endif

		char* cp = New(char);
		char* before = NewArray(char, 1);
		char* string = NewArray(char, 20);

//		before[1] = 'x';  // bug
	//	string[20] = 'x'; // bug
		//before[4] = 'x'; // bug not detected

//		Delete(before); // bug
		DeleteArray(before);
		DeleteArray(string);
		Delete(cp);
//		Delete(cp); // bug

		cout << "\nNo errors were detected.";
		return EXIT_SUCCESS;
	}

#endif // test
