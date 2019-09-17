/*
Modified by Adam Rumpf 2017 to work in C++.
It has been completely rewritten to drastically simplify it, at the cost of computational efficiency.
We're just implementing it as a linked list.
It is built just to include the functionality required by the main NETGEN script.
*/

/*** Copyright 1989 Norbert Schlenker.  All rights reserved.

*** This software is distributed subject to the following provisions:
***    - this notice may not be removed;
***    - you may modify the source code, as long as redistributed
***      versions have their modifications clearly marked;
***    - no charge, other than a nominal copying fee, may be made
***      when providing copies of this source code to others;
***    - if this source code is used as part of a product which is
***      distributed only as a binary, a copy of this source code
***      must be included in the distribution.
***
*** Unlike the GNU GPL, use of this code does not obligate you to
*** disclose your own proprietary source code.

*** The author of this software provides no warranty, express or
*** implied, as to its utility or correctness.  That said, reports
*** of bugs or compatibility problems will be gladly received by
*** nfs@princeton.edu, and fixes will be attempted.
***/


/*** index.c - routines to manipulate index lists */

/*** Definition:  An "index list" is an ascending sequence of positive
***              integers that can be operated upon as follows:
***
***                 make_index_list - makes an index list of consecutive
***                    integers from some lower bound through an upper bound.
***                 choose_index - given a number "k", removes the integer
***                    in the k'th position in the index list and returns it.
***                    Requests for positions less than 1 or greater than
***                    the current list length return zero.
***                 remove_index - removes a specified integer from the
***                    index list.  Requests to remove integers not in the
***                    list are ignored, except that they reduce the list's
***                    "pseudo_size" (see below).
***                 index_size - returns the number of integers in the
***                    index list.
***                 pseudo_size - returns the number of integers in the
***                    index list, less the number for which remove_index
***                    failed due to a request to remove a non-existent one.
***			(Note that this is here solely to support an apparent
***			bug in the original definition of the NETGEN program.)

*** Two simple methods of accomplishing these functions are:
***   - allocating an array of flags that indicate whether a particular
***     integer is valid, and searching the array during the choose_index
***     operation for the k'th valid integer.
***   - allocating a linked list for the indices and updating the list
***     during both the choose_index and remove_index operations.
***
*** For small index lists, the first of these methods is quite efficient
*** and is, in fact, implemented in the following code.  Unfortunately,
*** for the uses we have in mind (i.e. NETGEN), the typical access pattern
*** to index lists involves a large list, with both choose_index and
*** remove_index operations occurring at random positions in the list.
***
*** As a result, the code has been extended for the case of large lists.
*** The enclosed implementation makes use of a binary interval tree, which
*** records information regarding the valid intervals from which indices
*** may be chosen.  At a cost of a substantial increase in space requirements,
*** and under rather generous assumptions regarding the randomness of the
*** positions supplied to choose_index, running time becomes logarithmic
*** per choose_index and remove_index operation.
***/

#include "NetgenIndex.h"
using namespace std;

NetgenIndex::NetgenIndex()
{
	index_list = list<int>();
	pseudo_size = 0;
}

NetgenIndex::NetgenIndex(int from, int to)
{
	index_list = list<int>();
	for (int i = from; i <= to; i++)
		index_list.push_back(i);
	pseudo_size = to - from + 1;
}

/*** Make a new index list with a specified range. */
void NetgenIndex::make_index_list(int from, int to)
{
	index_list.clear();
	for (int i = from; i <= to; i++)
		index_list.push_back(i);
	pseudo_size = to - from + 1;
}

/*** Choose the integer at a certain position in an index list.  The
*** integer is then removed from the list so that it won't be chosen
*** again.  Choose_index returns 0 if the position is invalid.
***/
int NetgenIndex::choose_index(unsigned int position)
{
	if (position < 1 || position > index_list.size())
		return 0;
	else
	{
		list<int>::iterator it = index_list.begin();
		advance(it, position - 1);
		int chosen = *it;
		index_list.erase(it);
		pseudo_size--;
		return chosen;
	}
}

/*** Remove a particular integer from an index list.  If the integer
*** does not exist in the list, we reduce the list's pseudo-size,
*** but return no error indication.
***/
void NetgenIndex::remove_index(int index)
{
	pseudo_size--;

	// Search for the specified element
	list<int>::iterator it = index_list.begin();
	while (*it != index && it != index_list.end())
		it++;

	// If we've found the searched element, remove it
	if (*it == index)
		index_list.remove(index);
}

/*** Return actual number of remaining entries in the index list.
***/
int NetgenIndex::index_size()
{
	return index_list.size();
}

int NetgenIndex::get_pseudo_size()
{
	if (pseudo_size > 0)
		return pseudo_size;
	else
		return 0;
}