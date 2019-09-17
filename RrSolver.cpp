/*
Reads in a specified .min file generated by NETGEN, as well as parent/child flow values, and applies a randomized rounding rule to obtain a feasible solution. Feeds the problem to CPLEX and writes the results to a specified file. We expect exactly six arguments: the name of the .min file, the name of the main output file, the name of the parent flow file, the name of the child flow file, a random seed, and a number specifying which randomized rounding scheme to use (1 for RRC, 2 for RRP, 3 for RRF).
*/

#include <iostream>
#include <string>
#include <fstream>
#include "ilcplex\cplex.h"
#include "ilcplex\ilocplex.h"
#include "NetgenRandom.h"
using namespace std;

// Global variables and structures
string input_name;
string output_name;
string parent_out_name;
string child_out_name;
long seed;
int mode;
double bound;
long NODES;
long SOURCES;
long SINKS;
long DENSITY;
long DENSITY_INIT; // initial density, before artificial arcs are added
int PARENT;
long INTER;
#define MAXNODES 5000
#define MAXARCS 60000
long b[MAXNODES];
long u[MAXARCS];
long c[MAXARCS];
unsigned long tail[MAXARCS];
long head[MAXARCS]; // negative head means we're ignoring it
unsigned long parent[MAXARCS];
unsigned long child[MAXARCS];
double parent_flow[MAXARCS];
double child_flow[MAXARCS];
const double delivery_cost = -1; // unit delivery "reward" for relaxed sinks
double sol_objective;
double sol_time;

// Prototypes
int readin();
int read_parent();
int read_child();
int to_cplex();

int main(int argc, char* argv[])
{
	if (argc != 7 && argc != 8)
	{
		cout << "Expecting the following 6 (7) arguments: [input file] [output file] [parent flow file] [child flow file] [seed] [mode] ([bound])\n";
		return -1;
	}
	else
	{
		// Save parameters as global variables
		input_name = argv[1];
		output_name = argv[2];
		parent_out_name = argv[3];
		child_out_name = argv[4];
		seed = stoi(argv[5]);
		mode = stoi(argv[6]);
		if (argc == 7)
			bound = 0;
		else
			bound = stod(argv[7]);

		// Check variable validity
		if (seed <= 0 || mode < 1 || mode > 3)
		{
			cout << "Mode must be 1, 2, or 3\n";
			return -1;
		}
		if (bound < 0 || bound >= 0.5)
		{
			cout << "Bound must come from [0,0.5)\n";
			return -1;
		}

		// Try to read in the problem
		if (readin() == 0)
		{
			if (mode == 1)
			{
				if (read_child() != 0)
				{
					cout << "RR solver failed to read in child flow file " << child_out_name << '\n';
					return -1;
				}
			}

			if (mode == 2)
			{
				if (read_parent() != 0)
				{
					cout << "RR solver failed to read in parent flow file " << parent_out_name << '\n';
					return -1;
				}
			}

			// Try to solve the problem
			if (to_cplex() == 0)
			{
				// If the solution is found, output the results to a file
				ofstream outfile;
				outfile.open(output_name);
				if (outfile.is_open())
				{
					outfile << fixed;
					outfile << sol_objective << '\n' << sol_time;
					outfile.close();
				}
				else
				{
					cout << "Output file " << output_name << " failed to open.\n";
					return -1;
				}
			}
			else
			{
				cout << "RR solution not found.\n";
				return -1;
			}
		}
		else
		{
			cout << "RR solver failed to read in problem file " << input_name << '\n';
			return -1;
		}
	}
}

// Reads specified input file.  Returns 0 if successful.
int readin()
{
	int phase = 0; // 0 for objective, 1 for sources, 2 for sinks, 3 for arcs, 4 for interdependencies
	ifstream infile;
	infile.open(input_name);
	if (infile.is_open())
	{
		// Read in the .min file line-by-line, filling the vectors as we go
		int counter = 0; // counter for node or arc number
		while (infile.eof() == false)
		{
			string line, piece; // current line and piece of line
			getline(infile, line); // get whole line
			stringstream stream(line); // stream of line
			int id; // numbers read from line

			// Categorize type of line based on the first character
			switch (line[0])
			{
			// Ignore comments (c)
			case 'p': // min NODES DENSITY INTER PARENT
				getline(stream, piece, ' '); // 'p'
				getline(stream, piece, ' '); // "min"
				getline(stream, piece, ' '); // NODES
				NODES = stoi(piece);
				getline(stream, piece, ' '); // DENSITY
				DENSITY_INIT = stoi(piece);
				DENSITY = DENSITY_INIT;
				getline(stream, piece, ' '); // INTER
				INTER = stoi(piece);
				getline(stream, piece, ' '); // PARENTS
				if (piece[0] == 'a')
					PARENT = 1;
				else
					PARENT = 0;
				phase = 1;
				break;
			case 'n': // n ID FLOW
				getline(stream, piece, ' '); // 'n'
				getline(stream, piece, ' '); // ID
				id = stoi(piece);
				getline(stream, piece, ' '); // FLOW
				b[id - 1] = stoi(piece);
				if (b[id - 1] < 0 && phase == 1)
				{
					// we've just hit the first sink
					phase = 2;
					SOURCES = counter;
					counter = 0;
				}
				counter++;
				break;
			case 'a': // a SRC DST LOW CAP COST
				if (phase == 1)
				{
					// there were no sinks
					phase = 2;
					SOURCES = counter;
					counter = 0;
				}
				if (phase == 2)
				{
					// we've just hit the first arc
					phase = 3;
					SINKS = counter;
					counter = 0;
				}
				getline(stream, piece, ' '); // 'a'
				getline(stream, piece, ' '); // SRC
				tail[counter] = stoi(piece) - 1;
				getline(stream, piece, ' '); // DST
				head[counter] = stoi(piece) - 1;
				getline(stream, piece, ' '); // LOW
				getline(stream, piece, ' '); // CAP
				u[counter] = stoi(piece);
				getline(stream, piece, ' '); // COST
				c[counter] = stoi(piece);
				counter++;
				break;
			case 'i': // i parent child
				if (phase == 3)
				{
					// we've just hit the first interdependency
					phase = 4;
					counter = 0;
				}
				getline(stream, piece, ' '); // 'i'
				getline(stream, piece, ' '); // parent
				parent[counter] = stoi(piece) - 1;
				getline(stream, piece, ' '); // child
				child[counter] = stoi(piece) - 1;
				counter++;
				break;
			}
		}

		infile.close();
		return 0;
	}
	else
		return -1;
}

// Builds and exports the model defined by the input file.  Outputs 0 if a solution is found.
int to_cplex()
{
	// Prepare CPLEX
	IloEnv env; // environment
	IloModel model(env); // model

	// Variables and bounds
	IloNumVarArray x(env);
	for (int i = 0; i < DENSITY; i++)
		x.add(IloNumVar(env, 0, u[i], ILOFLOAT));

	// Network constraints
	IloRangeArray con1(env);
	for (int i = 0; i < NODES; i++)
	{
		if (PARENT == 0 && i < SOURCES)
			con1.add(IloRange(env, 0, b[i])); // relax source supply values if we're using nodes as parents
		else
			con1.add(IloRange(env, b[i], b[i])); // otherwise, it's an equality constraint
	}
	for (int i = 0; i < DENSITY; i++)
	{
		con1[tail[i]].setLinearCoef(x[i], 1); // tail coefficient
		if (head[i] >= 0)
			con1[head[i]].setLinearCoef(x[i], -1); // head coefficient (only applies to non-auxiliary arcs)
	}
	model.add(con1);

	// Interdependencies
	NetgenRandom * rand_num = new NetgenRandom(seed);
	IloRangeArray con2(env);
	for (int i = 0; i < INTER; i++)
	{
		// Roll to see whether to shut off the child or max out the parent
		double threshold; // probability of choosing to use the child
		switch (mode)
		{
			case 1: // child fullness
				threshold = (1.0 * child_flow[i]) / u[child[i]];
				break;
			case 2: // parent fullness
				threshold = (1.0 * parent_flow[i]) / u[parent[i]];
				break;
			case 3: // 50/50
				threshold = 0.5;
				break;
		}
		// Tighten threshold according to bounds
		if (threshold > 1 - bound)
			threshold = 1 - bound;
		if (threshold < bound)
			threshold = bound;
		double prob = (rand_num->random(1, 1000000) - 1) / (1.0 * 1000000);
		if (prob < threshold)
			con2.add(x[parent[i]] == u[parent[i]]); // using the child, so max out the parent
		else
			con2.add(x[child[i]] == 0); // not using the child, so zero it out
	}
	model.add(con2);
	delete rand_num;

	// Objective
	IloObjective obj = IloMinimize(env);
	for (int i = 0; i < DENSITY; i++)
		obj.setLinearCoef(x[i], c[i]); // arc cost coefficient
	model.add(obj);

	// Extraction and solution
	IloCplex cplex(env); // Cplex object
	cplex.extract(model);
	IloNum start = cplex.getTime(); // starting time
	IloBool solved = cplex.solve();
	sol_time = cplex.getTime() - start; // stop timer

	// Result output
	if (solved == IloTrue)
		sol_objective = cplex.getObjValue();
	else
	{
		sol_objective = -999;
		sol_time = -999;
	}

	// Finalization
	cplex.clear();
	env.end();
	if (solved == IloTrue)
		return 0;
	else
		return -1;
}

// Reads the parent flow values.
int read_parent()
{
	ifstream infile;
	infile.open(parent_out_name);
	if (infile.is_open())
	{
		string line;
		for (int i = 0; i < INTER; i++)
		{
			getline(infile, line);
			parent_flow[i] = stod(line);
		}

		infile.close();
		return 0;
	}
	else
		return -1;
}

// Reads the child flow values.
int read_child()
{
	ifstream infile;
	infile.open(child_out_name);
	if (infile.is_open())
	{
		string line;
		for (int i = 0; i < INTER; i++)
		{
			getline(infile, line);
			child_flow[i] = stod(line);
		}

		infile.close();
		return 0;
	}
	else
		return -1;
}