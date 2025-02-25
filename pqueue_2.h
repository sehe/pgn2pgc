#ifndef J_PRIORITYQUEUE_H
#define J_PRIORITYQUEUE_H
///////////////////////////////////////////////////////////////////////////////
//
//	A little singly-linked FIFO list template class.
//
// 	Revised to be a priority queue.  non-decreasing order. (small to big)
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////


// HEADER FILES

#include "joshdefs.h"


// DATA TYPES

//-----------------------------------------------------------------------------
//
// PriorityQueue Class (template)
//
template <class T>
class PriorityQueue
	{
	public:
		// default constructor
		PriorityQueue();
		// copy constructor
		PriorityQueue(const PriorityQueue<T>&);
		// assigment operator
		PriorityQueue<T>& operator = (const PriorityQueue<T>&);

		// add an object onto the end of the list.
		void add(const T& addValue);

		// set's object so that next() will return the first object in the list
		void gotoFirst();

		// assigns the given memory location with the value of the next item
		// in the list.
		// returns false if at the end of the list true otherwise
		bool next(T*);

		// returns if the list is empty or not
		bool isEmpty() const;

		size_t size() const {return fSize;} // returns the number of elements
		T& operator[](size_t index); // 0 to size - 1
		void remove(size_t index); // removes the element: does NOT search for the value and remove it (if T == size_t)

		// destructor
		virtual ~PriorityQueue();

	private:

		// a node
		struct Node
		{
			Node(const T& a): fValue(a), fNext(0) {}
			T fValue;
			Node* fNext;
		};

		Node* fFirst;   // the first node in the list
										// Revision: Last node is removed: this adds complexity to addition
		Node* fCurrent; // the node that will be returned by next()
		size_t fSize;
	};


// INLINE TEMPLATE DEFINITIONS

//---------------------------------------------------------------------------
//	Function: PriorityQueue<T>::isEmpty
//  Author:		Joshua Allen
//
//	Purpose:	to return wether the list is empty or not
//
//	Returns: true if the list is empty,
//						false otherwise
//
//---------------------------------------------------------------------------
template<class T>
inline
bool PriorityQueue<T>::isEmpty() const
	{
		return !fFirst;
	}


// TEMPLATE DEFINITIONS

//---------------------------------------------------------------------------
//	Function: PriorityQueue<T> constructor
//  Author:		Joshua Allen
//
//	Purpose: to creat a valid list
//
//---------------------------------------------------------------------------
template<class T>
PriorityQueue<T>::PriorityQueue() :
	fFirst(0),
	fCurrent(0),
	fSize(0)
	{}
//---------------------------------------------------------------------------
// copy constructor
//
//---------------------------------------------------------------------------
template<class T>
PriorityQueue<T>::PriorityQueue(const PriorityQueue<T>& b) :
	fFirst(0),
	fCurrent(0),
	fSize(0)
	{
		// re-use code
		this->operator=(b);
	}

//---------------------------------------------------------------------------
// assignment operator
//
//---------------------------------------------------------------------------
template<class T>
PriorityQueue<T>& PriorityQueue<T>::operator=(const PriorityQueue<T>& b)
	{
		if(this != &b)
			{
				// destroy current contents
				while(fFirst)
					{
						Node* tempNode = fFirst;

						fFirst = fFirst->fNext;

						delete  tempNode;
						tempNode = 0;
					}

				// copy values of previous object
				if(b.fFirst) // if source isn't empty
					{
						// set up fFirst
						fFirst = new Node(b.fFirst->fValue);
						CHECK_POINTER(fFirst);

						// copy positon of fCurrent
						if(b.fCurrent == b.fFirst)
								fCurrent = fFirst;

						Node* target = fFirst;
						const Node* source = b.fFirst->fNext;

						while(source)
							{
								target->fNext = new Node(source->fValue);
								CHECK_POINTER(target);

								target = target->fNext;
								// copy positon of fCurrent
								if(b.fCurrent == source)
										fCurrent = target;

								source = source->fNext;
							}
						fSize = b.fSize;
					}
			}

			return *this;
	}

//---------------------------------------------------------------------------
//	Function: PriorityQueue<T>::add
//  Author:		Joshua Allen
//
//	Purpose:	To add an item to the list
//
//	Arguments: addValue, the item to be added to the list
//
//---------------------------------------------------------------------------
template<class T>
void PriorityQueue<T>::add (const T& addValue)
	{
		if(isEmpty()) // first call
			{
				fFirst = new Node(addValue);

				CHECK_POINTER(fFirst);

				fCurrent = fFirst;
				++fSize;
				return;
			}

		// REVISION
		// find the appropriate spot to place it in
		assert(fFirst);

		// special case if it is the first one
		// avoids keeping track of two pointers in the loop
		if(addValue < fFirst->fValue)
			{
				Node* temp = fFirst;
				// set fCurrent to the same value, so that next() will always return
				// the first item (it's common to forget to call gotoFirst())
				fCurrent = fFirst = new Node(addValue);
				CHECK_POINTER(fFirst);
				fFirst->fNext = temp;
				++fSize;
				return;
			}


		Node* current = fFirst;

		while(current->fNext && addValue > current->fNext->fValue)
			{
				current = current->fNext;
			}

		// stick it between the two nodes
		Node*  temp = current->fNext;

		current->fNext = new Node(addValue);
		CHECK_POINTER(current->fNext);
		current = current->fNext;
		current->fNext = temp;
		++fSize;
		fCurrent = fFirst; // ??!
	}

//---------------------------------------------------------------------------
//	PriorityQueue<T>::gotoFirst
//
// 	sets the object so that next() will return the first item in the list.
// 	next() will then return the second, then third and so on.
//
//---------------------------------------------------------------------------
template<class T>
void PriorityQueue<T>::gotoFirst()
	{
		fCurrent = fFirst;
	}

//---------------------------------------------------------------------------
//	Function:	PriorityQueue<T>::next
//  Author:		Joshua Allen
//
//	Purpose: to return the next object in the list.
//
// gotoFirst sets next() to return the first item in the list the next time it
// is called.  Subsequent calls to next will return the 2nd, 3rd and so on.
//
// returns false at the end of the list (valueHolder is not changed)
//
//---------------------------------------------------------------------------
template<class T>
bool PriorityQueue<T>::next(T* valueHolder)
	{
		assert (valueHolder);

		if(!fCurrent)
				return false;

		*valueHolder = fCurrent->fValue;
		fCurrent = fCurrent->fNext;

		return true;
	}

template <class T>
T& PriorityQueue<T>::operator[](size_t index)
{
	assert(index >= 0 && index < size()); // index out of range

	Node* current = fFirst;
	while(index && current)
	{
		current = current->fNext;
		--index;
	}

	return current->fValue;
}

template <class T>
void PriorityQueue<T>::remove(size_t index)
{
	assert(index >= 0 && index < size()); // index out of range
	assert(size());
	assert(fFirst);

	//!!? fully understand before changing this code, and carefully test any changes

	// special case for remove(0) greatly simplifies logic
	if(!index)
	{
		Node* tempNode = fFirst;
		fFirst = fFirst->fNext;
		delete tempNode;
		tempNode = 0;

	} else {

		Node* current = fFirst;
		int i = index;

		while(--i > 0 && current->fNext)
		{
			current = current->fNext;
		}

		// we're at the one before the one we want to delete
		Node* tempNode = current->fNext;
		current->fNext = current->fNext->fNext;

		delete tempNode;
		tempNode = 0;
	}

	--fSize;
	fCurrent = fFirst;

}

//---------------------------------------------------------------------------
//	Function: PriorityQueue<T> destructor
//  Author:		Joshua Allen
//
//	Purpose: to release any memory back to the system that the list is
//						still responsible for
//
//---------------------------------------------------------------------------
template<class T>
PriorityQueue<T>::~PriorityQueue()
	{
		while(fFirst)
			{
				Node* tempNode = fFirst;

				fFirst = fFirst->fNext;

				delete  tempNode;
				tempNode = 0;
				--fSize;
			}
		assert(!fSize);
	}

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test Test
//		The code is included here, but if you want to compile it you probably
//			have to put it in a seperate .cpp file.
//#define TEST
#ifdef TEST
#undef TEST

#include <iostream.h>
#include <stdlib.h>

int main()
	{
		PriorityQueue<int> l;

		assert(l.isEmpty());
		for(int i = 200; i > 1; --i)
			{
				l.add(rand());
				assert(!l.isEmpty());
			}

		int val = -2;
		cout << endl << endl;
		while(l.next(&val))
			{
				cout << "\t" << val;
			}

		PriorityQueue<int> l2(l);

		l.gotoFirst();
		assert(!l2.next(&val));

		cout << endl << endl;
		while(l.next(&val))
			{
				cout << "\t" << val;
			}

		l.add(-1);

		l.gotoFirst();
		l2.gotoFirst();
		l.next(&val);
		cout << "\nFirst L: " << val;

		l2.next(&val);
		cout << "\nFirst L2: " << val;

		l2 = l;
		l2.next(&val);
		cout << "\nNext L2: " << val;
		l.next(&val);
		cout << "\nNext L: " << val;

		return 0;
	}
#endif // test

#endif // pqueue.h
