/*
Modified by Adam Rumpf 2017 to work in C++.
We also modify the code to accept an argument for number of interdependencies, which must be less than the number of demand nodes.
Two additional values are added to the objective line: the number of interdependencies, and 'a' or 'n' to signify either arcs or nodes as parents.
The interdependencies are reported at the end of the .min file, using the syntax "i PARENT CHILD", where PARENT is a node/arc ID and CHILD is an arc number.

Edit: If we are using nodes as parents, then instead of reporting the node ID, we generate a new arc and report its ID.
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


/*** netgen - C version of the standard NETGEN network generator
***          This program is a functional equivalent of the
***          standard network generator NETGEN described in:
***		Klingman, D., A. Napier, and J. Stutz, "NETGEN:  A Program
***		  for Generating Large Scale Capacitated Assignment,
***		  Transportation, and Minimum Cost Flow Network Problems",
***		  Management Science 20, 5, 814-821 (1974)
***
***	      This software provides a number of interfaces for use by
***	      network solvers.  Standard call interfaces are supplied for
***	      use by (Unix) C and Fortran solvers, with generation parameters
***	      passed into the generator and the flow network passed back to
***	      the solver via large external (COMMON in Fortran) arrays.
***	      For the DIMACS challenge, this code will produce output files
***	      in the appropriate format for later reading by solvers.
***          Undefine the symbol DIMACS when using the call interface.
***
***          The generator produces exact duplicates of the networks
***          made by the Fortran code (even though that means bugs
***          are being perpetuated). It is faster by orders of magnitude
***          in generating large networks, primarily by imposing the
***          notion of the abstract data type INDEX_LIST and implementing
***          the type operations in a reasonably efficient fashion.
***/

/*** Generates transportation problems if:
***	SOURCES+SINKS == NODES && TSOURCES == TSINKS == 0
***
*** Generates assignment problems if above conditions are satisfied, and:
***	SOURCES == SINKS && SUPPLY == SOURCES
***
*** Generates maximum flow problems if not an assignment problem and:
***	MINCOST == MAXCOST == 1

*** Implementation notes:
***
***	This file contains both a Fortran and a C interface. The
***	Fortran interface is suffixed with an underscore to make it
***	callable in the normal fashion from Fortran (a Unix convention).
***
***    Because Fortran has no facility for pointers, the common arrays
***    are statically allocated.  Static allocation has nothing to recommend
***    it except for the need for a Fortran interface.
***
***    This software expects input parameters to be long integers
***    (in the sense of C); that means no INTEGER*2 from Fortran callers.
***
***	Compiling with -DDIMACS produces a program that reads problem
***	parameters, generates the appropriate problem, and prints it.
***
***	Compiling with -DDEBUG produces code with externally visible
***	procedure names, useful for debugging and profiling.
***/

/*** netgen.h
*** Prototype code for inclusion into network generation routines
***/

#include <iostream>
#include <string>
#include <fstream>
//#include <vector>
#include "NetgenRandom.h"
#include "NetgenIndex.h"
using namespace std;

// parameters
string file_name; // output file
long seed; // RNG seed
long NODES; // number of nodes
long SOURCES; // number of sources (including transshipment)
long SINKS; // number of sinks (including transshipment)
long DENSITY; // number of (requested) arcs
long MINCOST; // minimum cost of arcs
long MAXCOST; // maximum cost of arcs
long SUPPLY; // total supply
long TSOURCES; // transshipment sources
long TSINKS; // transshipment sinks
long HICOST; // percent of skeleton arcs given maximum cost
long CAPACITATED; // percent of arcs to be capacitated
long MINCAP; // minimum capacity for capacitated arcs
long MAXCAP; // maximum capacity for capacitated arcs
int PARENT; // 0 if parents are sink nodes, 1 if parents are arcs
long INTER; // number of interdependencies
// maximum problem sizes
#define MAXNODES 5000
#define MAXARCS 60000
// error indicators
#define BAD_SEED -1
#define TOO_BIG -2
#define BAD_PARMS -3
#define ALLOCATION_FAILURE -4
// max/min methods
#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))
// types
typedef unsigned long NODE; // node number
typedef unsigned long ARC; // arc number
typedef long CAPACITY; // arc capacity
typedef long COST; // arc cost
typedef unsigned long INDEX; // index element
typedef int INDEX_LIST; // index list handle
// methods
void create_supply(NODE, CAPACITY); //create supply nodes
void sort_skeleton(int); // sort skeleton chains
void pick_head(NetgenIndex*, NODE); // pick destination for rubbish arcs
void error_exit(int); // print error message and exits
// variables
NODE nodes_left;
ARC arc_count;
/*vector<NODE> pred; // predecessors in the linked list representation of the skeleton arcs
vector<NODE> head; // skeleton arc heads
vector<NODE> tail; // skeleton arc tails
vector<NODE> FROM; // arc origins
vector<NODE> TO; // arc destinations
vector<CAPACITY> U; // capacity
vector<COST> C; // cost
vector<CAPACITY> B; // node supply (demand) values
vector<ARC> child; // child arcs
vector<long> parent; // parent nodes/arcs*/
NODE pred[MAXNODES];
NODE head[MAXARCS];
NODE tail[MAXARCS];
NODE FROM[MAXARCS];
NODE TO[MAXARCS];
CAPACITY U[MAXARCS];
COST C[MAXARCS];
CAPACITY B[MAXNODES];
ARC child[MAXARCS];
long parent[MAXARCS];
const int delivery_cost = -100; // "reward" for delivering to a parent node
// RNG
//NetgenRandom * rando = new NetgenRandom();
NetgenRandom rando;
int netgen(); // try to build network; output arc count if it worked, or an error code if not
void printout(); // print the network to a specified file name
void node_parents(); // transform parent nodes into transshipment nodes, and add auxiliary arcs

int main(int argc, char* argv[])
{


	if (argc != 18)
	{
		cout << "Expecting the following 17 arguments:\n";
		cout << "[file name] [seed] [NODES] [SOURCES] [SINKS] [DENSITY] [MINCOST] [MAXCOST] [SUPPLY]\n";
		cout << "[TSOURCES] [TSINKS] [HICOST] [CAPACITATED] [MINCAP] [MAXCAP] [PARENT] [INTER]\n";
		return -1;
	}
	else
	{
		// Save parameters as global varialbes
		file_name = argv[1];
		seed = stoi(argv[2]);
		rando.set_random(seed);
		NODES = stoi(argv[3]);
		SOURCES = stoi(argv[4]);
		SINKS = stoi(argv[5]);
		DENSITY = stoi(argv[6]);
		MINCOST = stoi(argv[7]);
		MAXCOST = stoi(argv[8]);
		SUPPLY = stoi(argv[9]);
		TSOURCES = stoi(argv[10]);
		TSINKS = stoi(argv[11]);
		HICOST = stoi(argv[12]);
		CAPACITATED = stoi(argv[13]);
		MINCAP = stoi(argv[14]);
		MAXCAP = stoi(argv[15]);
		PARENT = stoi(argv[16]);
		INTER = stoi(argv[17]);

		printout();
		return 0;
	}

	/*seed = 477021979; // To add: read in custom parameters; generate interdependencies; try looping or calling .exe from another program
	NODES = 256;
	SOURCES = 52;
	SINKS = 52;
	DENSITY = 1024;
	MINCOST = 1;
	MAXCOST = 100;
	SUPPLY = 20000;
	TSOURCES = 0;
	TSINKS = 0;
	HICOST = 100;
	CAPACITATED = 100;
	MINCAP = 100;
	MAXCAP = 500;*/

	//rando->set_random(seed);
	//rando.set_random(seed);

	//printout("test.min");

	//return 0;
}

/*Netgen::~Netgen()
{
delete rando;
}*/

// The main method
int netgen()
{
	NODE i, j, k, source, node, sinks_per_source, it;
	int chain_length, supply_per_sink, partial_supply, sort_count;
	COST cost;
	CAPACITY cap;

	// Perform sanity checks on the input
	if (seed <= 0)
		return BAD_SEED;
	if (NODES > MAXNODES || DENSITY > MAXARCS)
		return TOO_BIG;
	if (NODES <= 0 ||
		NODES > DENSITY ||
		SOURCES <= 0 ||
		SINKS <= 0 ||
		SOURCES + SINKS > NODES ||
		MINCOST > MAXCOST ||
		SUPPLY < SOURCES ||
		TSOURCES > SOURCES ||
		TSINKS > SINKS ||
		HICOST < 0 || HICOST > 100 ||
		CAPACITATED < 0 || CAPACITATED > 100 ||
		MINCAP > MAXCAP ||
		PARENT < 0 || PARENT > 1 ||
		(PARENT == 0 && INTER > SINKS) ||
		(PARENT == 1 && INTER > DENSITY / 2))
		return BAD_PARMS;

	// Setting up
	/*pred.resize(ceil(1.1 * DENSITY)); // giving the main arrays a bit of extra room
	head.resize(ceil(1.1 * DENSITY));
	tail.resize(ceil(1.1 * DENSITY));
	FROM.resize(ceil(1.1 * DENSITY));
	TO.resize(ceil(1.1 * DENSITY));
	U.resize(ceil(1.1 * DENSITY));
	C.resize(ceil(1.1 * DENSITY));
	B.resize(ceil(1.1 * NODES));
	parent.resize(INTER);
	child.resize(INTER);*/
	arc_count = 0; // running tally of arcs generated
	nodes_left = NODES - SINKS + TSINKS; // running tally of nodes left to generate
	create_supply(SOURCES, SUPPLY);

	/*** Form most of the network skeleton.  First, 60% of the transshipment
	*** nodes are divided evenly among the various sources; the remainder
	*** are chained onto the end of the chains belonging to random sources.
	***/
	// This is accomplished via a linked list, where eah transshipment node points back to its predecessor in the chain,
	// and the source will eventually point to the end of the chain.
	for (i = 1; i <= SOURCES; i++)
		pred[i] = i; // pointing the sources to themselves to begin with
	NetgenIndex * indie = new NetgenIndex(SOURCES + 1, NODES - SINKS);
	source = 1;
	for (i = NODES - SOURCES - SINKS; i > (4 * (NODES - SOURCES - SINKS) + 9) / 10; i--) // distribute the first 60% of transshipment nodes evenly
	{
		//node = indie->choose_index(rando->random(1, indie->index_size())); // choose a random as-of-yet unchosen transshipment node to add to the chain
		node = indie->choose_index(rando.random(1, indie->index_size()));
		pred[node] = pred[source];
		pred[source] = node;
		if (++source > SOURCES) // increment the source number, wrapping around if necessary, to evenly add to each chain
			source = 1;
	}
	for (; i > 0; --i) // distribute the remaining transshipment nodes randomly
	{
		//node = indie->choose_index(rando->random(1, indie->index_size()));
		node = indie->choose_index(rando.random(1, indie->index_size()));
		//source = rando->random(1, SOURCES);
		source = rando.random(1, SOURCES);
		pred[node] = pred[source];
		pred[source] = node;
	}
	delete indie;

	/*** For each source chain, hook it to an "appropriate" number of sinks,
	*** place capacities and costs on the skeleton edges, and then call
	*** pick_head to add a bunch of rubbish edges at each node on the chain.
	***/
	for (source = 1; source <= SOURCES; source++)
	{
		sort_count = 0; // tally of the number of nodes we've visited in the current chain (will end up equalling the total length)
		node = pred[source]; // transshipment node at the end of the current chain
		while (node != source) // go backwards up the chain
		{
			sort_count++;
			head[sort_count] = node; // record the sequence of heads and tails we encounter
			node = tail[sort_count] = pred[node];
		}
		if (NODES - SOURCES - SINKS == 0) // choosing how many sinks to associate with this chain
			sinks_per_source = SINKS / SOURCES + 1;
		else
			sinks_per_source = 2 * sort_count * SINKS / (NODES - SOURCES - SINKS); // scale with length of chain (longer means more)
		sinks_per_source = MAX(2, MIN(sinks_per_source, SINKS)); // restricting to reasonable bounds
		//vector<NODE> sinks(sinks_per_source + 1); // list of the sinks we'll be connecting
		NODE sinks[MAXNODES];
		NetgenIndex * indie = new NetgenIndex(NODES - SINKS, NODES - 1);
		for (i = 0; i < sinks_per_source; i++)
			//sinks[i] = indie->choose_index(rando->random(1, indie->index_size())); // choose sinks for the list without repetition
			sinks[i] = indie->choose_index(rando.random(1, indie->index_size()));
		if (source == SOURCES && indie->index_size() > 0) // on the last source, if there are still sinks left that can be chosen
		{
			// re-form the list to contain all unselected sinks
			//for (i = 0; i < sinks_per_source + 1; i++)
			//	sinks[i] = 0;
			while (indie->index_size() > 0)
			{
				j = indie->choose_index(1);
				if (B[j] == 0)
					sinks[sinks_per_source++] = j;
			}
		}
		delete indie;

		chain_length = sort_count;
		supply_per_sink = B[source - 1] / sinks_per_source; // start by evenly distributing the chain's supply among all of the selected sinks
		k = pred[source]; // go to the end of the chain
		for (i = 0; i < sinks_per_source; i++)
		{
			sort_count++; // sort_count is now tallying the total number of nodes involved in the current chain, including sinks
						  //partial_supply = rando->random(1, supply_per_sink); // choose how much supply to give to this sink
			partial_supply = rando.random(1, supply_per_sink);
			//j = rando->random(0, sinks_per_source - 1); // choose how many sinks to look at
			j = rando.random(0, sinks_per_source - 1);
			tail[sort_count] = k; // creating the arc linking to the sink
			head[sort_count] = sinks[i] + 1;
			B[sinks[i]] -= partial_supply;
			B[sinks[j]] -= supply_per_sink - partial_supply;
			k = source;
			//for (j = rando->random(1, chain_length); j > 0; j--) // move up the chain a random amount
			for (j = rando.random(1, chain_length); j > 0; j--)
				k = pred[k];
		}
		B[sinks[0]] -= B[source - 1] % sinks_per_source;
		//sinks.clear();
		//sinks.shrink_to_fit();

		sort_skeleton(sort_count);
		tail[sort_count + 1] = 0;
		for (i = 1; i <= sort_count; )
		{
			NetgenIndex * indie = new NetgenIndex(SOURCES - TSOURCES + 1, NODES);
			indie->remove_index(tail[i]);
			it = tail[i];
			while (it == tail[i]) // across all iterations of this next block, we will process all skeleton arcs
			{
				indie->remove_index(head[i]);
				cap = SUPPLY;
				//if (rando->random(1, 100) <= CAPACITATED) // roll to see if this arc is capacitated
				if (rando.random(1, 100) <= CAPACITATED)
					cap = MAX(B[source - 1], MINCAP); // make sure its capacity still accommodates enough supply
				cost = MAXCOST;
				//if (rando->random(1, 100) > HICOST) // roll to see if this arc's cost is maxed out
				if (rando.random(1, 100) > HICOST)
					//cost = rando->random(MINCOST, MAXCOST); // if not, just randomly roll for cost
					cost = rando.random(MINCOST, MAXCOST);
				//SAVE_ARC(it, head[i], cost, cap)
				FROM[arc_count] = it;
				TO[arc_count] = head[i];
				C[arc_count] = cost;
				U[arc_count] = cap;
				arc_count++;
				i++;
			}
			pick_head(indie, it);
			delete indie;
		}
	}

	/*** Add more rubbish edges out of the transshipment sinks. */
	for (i = NODES - SINKS + 1; i <= NODES - SINKS + TSINKS; i++) // process each transshipment sink
	{
		NetgenIndex * indie = new NetgenIndex(SOURCES - TSOURCES + 1, NODES);
		indie->remove_index(i);
		pick_head(indie, it);
		delete indie;
	}

	// Generate the interdependencies
	NetgenIndex * arc_id = new NetgenIndex(1, arc_count);
	for (i = 0; i < INTER; i++)
		child[i] = arc_id->choose_index(rando.random(1, arc_id->index_size()));
	if (PARENT == 0)
	{
		// parents are nodes
		NetgenIndex * node_id = new NetgenIndex(NODES - SINKS + 1, NODES); // sinks are numbered from [NODES - SINKS + 1, NODES]
		for (i = 0; i < INTER; i++)
			parent[i] = node_id->choose_index(rando.random(1, node_id->index_size()));
		delete node_id;
	}
	else
	{
		// parents are arcs
		for (i = 0; i < INTER; i++)
			parent[i] = arc_id->choose_index(rando.random(1, arc_id->index_size()));
	}
	delete arc_id;

	// Conduct special transformations if we are using nodes as parents
	if (PARENT == 0)
		node_parents();

	return arc_count;
}

// set up supply values (B) for the supply nodes
void create_supply(NODE sources, CAPACITY supply)
{
	CAPACITY supply_per_source = supply / sources;
	CAPACITY partial_supply;
	NODE i;

	for (i = 0; i < sources; i++)
	{
		//partial_supply = rando->random(1, supply_per_source);
		partial_supply = rando.random(1, supply_per_source);
		B[i] += partial_supply;
		//B[rando->random(0, sources - 1)] += supply_per_source - partial_supply;
		B[rando.random(0, sources - 1)] += supply_per_source - partial_supply;
	}
	//B[rando->random(0, sources - 1)] += supply % sources;
	B[rando.random(0, sources - 1)] += supply % sources;
}

void sort_skeleton(int sort_count) 		/* Shell sort */
{
	int m, i, j, k, temp;

	m = sort_count;
	while ((m /= 2) != 0)
	{
		k = sort_count - m;
		for (j = 1; j <= k; j++)
		{
			i = j;
			while (i >= 1 && tail[i] > tail[i + m])
			{
				temp = tail[i];
				tail[i] = tail[i + m];
				tail[i + m] = temp;
				temp = head[i];
				head[i] = head[i + m];
				head[i + m] = temp;
				i -= m;
			}
		}
	}
}

void pick_head(NetgenIndex*indie, NODE desired_tail)
{
	NODE non_sources = NODES - SOURCES + TSOURCES;
	ARC remaining_arcs;
	if (arc_count >= DENSITY) // preventing wraparound from unsigned int values
		remaining_arcs = 0;
	else
		remaining_arcs = DENSITY - arc_count;
	INDEX index;
	int limit;
	long upper_bound;
	CAPACITY cap;

	nodes_left--;
	if (2 * nodes_left >= remaining_arcs)
		return;

	if ((remaining_arcs + non_sources - indie->get_pseudo_size() - 1) / (nodes_left + 1) >= non_sources - 1)
		limit = non_sources;
	else
	{
		upper_bound = 2 * (remaining_arcs / (nodes_left + 1) - 1);
		do
		{
			//limit = rando->random(1, upper_bound);
			limit = rando.random(1, upper_bound);
			if (nodes_left == 0)
				limit = remaining_arcs;
		} while (nodes_left * (non_sources - 1) < remaining_arcs - limit);
	}

	for (; limit > 0; limit--)
	{
		//index = indie->choose_index(rando->random(1, indie->index_size()));
		index = indie->choose_index(rando.random(1, indie->get_pseudo_size()));
		cap = SUPPLY;
		//if (rando->random(1, 100) <= CAPACITATED)
		if (rando.random(1, 100) <= CAPACITATED)
			//cap = rando->random(MINCAP, MAXCAP);
			cap = rando.random(MINCAP, MAXCAP);
		//SAVE_ARC(desired_tail, index, random(MINCOST, MAXCOST), cap);
		FROM[arc_count] = desired_tail;
		TO[arc_count] = index;
		//C[arc_count] = rando->random(MINCOST, MAXCOST);
		C[arc_count] = rando.random(MINCOST, MAXCOST);
		U[arc_count] = cap;
		arc_count++;
	}
}

// Conduct transformations to rewrite parent nodes as parent arcs, which involves adding auxiliary arcs.
void node_parents()
{
	// process each parent node
	for (int i = 0; i < INTER; i++)
	{
		FROM[arc_count] = parent[i]; // new arc tail
		TO[arc_count] = 0; // new arc head doesn't matter
		U[arc_count] = -B[parent[i] - 1]; // new arc's capacity is old node's demand
		C[arc_count] = delivery_cost; // new arc's cost is the delivery reward
		B[parent[i] - 1] = 0; // turn parent node into transshipment
		parent[i] = arc_count + 1; // new arc ID
		arc_count++;
	}
}

/*** Print an appropriate error message and then exit with a nonzero code. */
void error_exit(int rc)
{
	switch (rc)
	{
	case BAD_SEED: // for the same reason that we can't use variables to define array sizes, we can't use them in switches
		cout << "NETGEN requires a positive random seed\n";
		break;
	case TOO_BIG:
		cout << "Problem too large for generator\n";
		break;
	case BAD_PARMS:
		cout << "Inconsistent parameter settings - check the input\n";
		break;
	case ALLOCATION_FAILURE:
		cout << "Memory allocation failure\n";
		break;
	default:
		cout << "Internal error\n";
		break;
	}
	exit(1000 - rc);
}

// Printing the network to a specified file.
void printout()
{
	ofstream outfile;
	outfile.open(file_name);
	if (outfile.is_open() == true)
	{
		int i;

		outfile << "c NETGEN flow network generator (C++ version)\n";
		outfile << "c Modified to generate interdependent networks\n";
		outfile << "c  ---------------------------\n";
		outfile << "c   Random seed:          " << seed << "\n";
		outfile << "c   Number of nodes:      " << NODES << "\n";
		outfile << "c   Source nodes:         " << SOURCES << "\n";
		outfile << "c   Sink nodes:           " << SINKS << "\n";
		outfile << "c   Number of arcs:       " << DENSITY << "\n";
		outfile << "c   Minimum arc cost:     " << MINCOST << "\n";
		outfile << "c   Maximum arc cost:     " << MAXCOST << "\n";
		outfile << "c   Total supply:         " << SUPPLY << "\n";
		outfile << "c   Transshipment -\n";
		outfile << "c     Sources:            " << TSOURCES << "\n";
		outfile << "c     Sinks:              " << TSINKS << "\n";
		outfile << "c   Skeleton arcs -\n";
		outfile << "c     With max cost:      " << HICOST << "%\n";
		outfile << "c     Capacitated:        " << CAPACITATED << "%\n";
		outfile << "c   Minimum arc capacity: " << MINCAP << "\n";
		outfile << "c   Maximum arc capacity: " << MAXCAP << "\n";
		outfile << "c   Interdependencies -\n";
		if (PARENT == 0)
			outfile << "c     Parents:            Sink Nodes\n";
		else
			outfile << "c     Parents:            Arcs\n";
		outfile << "c     Number:             " << INTER << "\n";

		// actually run NETGEN, and get the number of arcs from the output (negative output indicates an error)
		int arcs = netgen();
		if (arcs < 0)
			error_exit(arcs);

		if (MINCOST == 1 && MAXCOST == 1)
		{
			outfile << "c\n";
			outfile << "c  *** Maximum flow ***\n";
			outfile << "c\n";
			outfile << "p max " << NODES << ' ' << arcs << '\n';
			for (i = 0; i < NODES; i++)
			{
				if (B[i] > 0)
					outfile << "n " << i + 1 << " s\n";
				else
				{
					if (B[i] < 0)
						outfile << "n " << i + 1 << " t\n";
				}
			}
			for (i = 0; i < arcs; i++)
				outfile << "a " << FROM[i] << ' ' << TO[i] << ' ' << U[i] << '\n';
		}
		else
		{
			outfile << "c\n";
			outfile << "c  *** Minimum cost flow ***\n";
			outfile << "c\n";
			outfile << "p min " << NODES << ' ' << arcs << ' ' << INTER;
			if (PARENT == 0)
				outfile << " n\n";
			else
				outfile << " a\n";
			for (i = 0; i < NODES; i++)
			{
				if (B[i] != 0)
					outfile << "n " << i + 1 << ' ' << B[i] << '\n';
			}
			for (i = 0; i < arcs; i++)
				outfile << "a " << FROM[i] << ' ' << TO[i] << " 0 " << U[i] << ' ' << C[i] << '\n';
			for (i = 0; i < INTER; i++)
				outfile << "i " << parent[i] << ' ' << child[i] << '\n';
		}

		outfile.close();
	}
	else
		cout << "Unable to write to file " << file_name << ".\n";

	exit(0);
}