#ifndef J_LIST_H
#define J_LIST_H
///////////////////////////////////////////////////////////////////////////////
//
//		A list that acts like a growable array.
//		Efficient add().  Expensive locate and remove.
//
///////////////////////////////////////////////////////////////////////////////


#include <stddef.h>
#include "joshdefs.h"

//-----------------------------------------------------------------------------
template <class T>
class List
{
	public:
		List(); // size starts out as 1

		void add(const T&); // ++size
		void remove(size_t index); // removes the element: does NOT search for the value and remove it (if T == size_t)
		void makeEmpty(); // deletes all the nodes in the list

		//!?? returns a pointer to member, cannot be used for longer than
		// 	the list exists
		T& operator[](size_t index); // 0 to size - 1

		size_t size() const;

		virtual ~List();

	private:

		struct Node
		{
			Node(const T& val) : fNext(0), fValue(val) {}
			Node* fNext;
			T fValue;
		};

		// cannot copy
		List(const List<T>&);
		List<T>& operator=(const List<T>&);

		Node *fFirst, *fLast;
		size_t fSize;
};

template <class T>
List<T>::List() :
	fFirst(0),
	fLast(0),
	fSize(0)
{
}

template <class T>
T& List<T>::operator[](size_t index)
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
void List<T>::add(const T& value)
{
	if(!fSize) // first call: populate
	{
		fFirst = New(Node(value));
		CHECK_POINTER(fFirst);
		fLast = fFirst;
	} else
	{
		fLast->fNext = New(Node(value));
		CHECK_POINTER(fLast->fNext);
		fLast = fLast->fNext;
	}
	++fSize;
}

template <class T>
void List<T>::remove(size_t index)
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
		Delete(tempNode);
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

		if(index >= fSize - 1) // if removed the last element adjust fLast
		{
			fLast = current;
		}
		Delete(tempNode);
		tempNode = 0;
	}
	--fSize;

	if(fSize <= 1)
	{
		fLast = fFirst;
	}
}

template <class T>
inline
size_t List<T>::size() const
{
	return fSize;
}

template <class T>
void List<T>::makeEmpty()
{
	while(fFirst)
	{
		Node* tempNode = fFirst;

		fFirst = fFirst->fNext;

		Delete (tempNode);
		tempNode = 0;
		--fSize;
	}

	assert(!fSize);
}

template <class T>
List<T>::~List()
{
	makeEmpty();
}

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test Test
//#define TEST
#ifdef TEST
#undef TEST

#include <iostream.h>
int main()
{
	List<int> l;

	l.add(0);
	l.add(1);
	l.add(2);
	l.remove(1);
	l.add(3);

	if(l.size())
		l.remove(l.size() - 1);

	l.add(4);
	l.remove(0);

	for(int i = 0; i < l.size(); ++i)
		{
			cout << "\n" << l[i];
		}

	return 0;
}

#endif   // test

#endif 	// list.h