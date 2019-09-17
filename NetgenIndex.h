#pragma once
#include <list>
using namespace std;

class NetgenIndex
{
private:
	list<int> index_list;
	int pseudo_size; // a conservative approximation of the list's size to avoid trying to search an empty list
public:
	NetgenIndex();
	NetgenIndex(int, int);
	void make_index_list(int, int);
	int choose_index(unsigned int);
	void remove_index(int);
	int index_size();
	int get_pseudo_size();
	// deleting the pointer to this object is equivalent to free_index_list
};