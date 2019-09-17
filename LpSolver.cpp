/*
Reads in a specified .min file generated by NETGEN, interpreted as an LP. Feeds the problem to CPLEX and writes the results to three specified files: one for cost/time, one for parent flows, and one for child flows (for use in RR schemes). We expect exactly four arguments: the name of the .min file, the name of the main output file, the name of the parent flow file, and the name of the child flow file.
*/

#include <iostream>
#include <string>
#include <fstream>
#include "ilcplex\cplex.h"
#include "ilcplex\ilocplex.h"
using namespace std;

// Global variables and structures
string input_name;
string output_name;
string parent_out_name;
string child_out_name;
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
const double delivery_cost = -100; // unit delivery "reward" for relaxed sinks
double sol_objective;
double sol_time;
double sol_load;

// Prototypes
int readin();
int to_cplex();

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		cout << "Expecting the following 4 arguments: [input file] [output file] [parent flow file] [child flow file]\n";
		return -1;
	}
	else
	{
		// Save parameters as global variables
		input_name = argv[1];
		output_name = argv[2];
		parent_out_name = argv[3];
		child_out_name = argv[4];

		// Try to read in the problem
		if (readin() == 0)
		{
			// Try to solve the problem
			if (to_cplex() == 0)
			{
				// If the solution is found, output the results to a file

				// Main results
				ofstream outfile;
				outfile.open(output_name);
				if (outfile.is_open())
				{
					outfile << fixed;
					outfile << sol_objective << '\n' << sol_time << '\n' << sol_load;
					outfile.close();


					// Parent flows
					ofstream parentfile;
					parentfile.open(parent_out_name);
					if (parentfile.is_open())
					{
						parentfile << fixed;
						for (int i = 0; i < INTER; i++)
							parentfile << (1.0 * parent_flow[i]) / u[parent[i]] << '\n';
						parentfile.close();
						
						// Child flows
						ofstream childfile;
						childfile.open(child_out_name);
						if (childfile.is_open())
						{
							childfile << fixed;
							for (int i = 0; i < INTER; i++)
								childfile << (1.0 * child_flow[i]) / u[child[i]] << '\n';
							childfile.close();

							return 0;
						}
						else
						{
							cout << "Childflow output file " << child_out_name << " failed to open.\n";
							return -1;
						}
					}
					else
					{
						cout << "Parent flow output file " << parent_out_name << " failed to open.\n";
						return -1;
					}
				}
				else
				{
					cout << "Output file " << output_name << " failed to open.\n";
					return -1;
				}
			}
			else
			{
				cout << "LP solution not found.\n";
				return -1;
			}

		}
		else
		{
			cout << "LP solver failed to read in problem file " << input_name << '\n';
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
	IloRangeArray con2(env);
	for (int i = 0; i < INTER; i++)
		con2.add(0 <= (1.0 / u[parent[i]]) * x[parent[i]] - (1.0 / u[child[i]]) * x[child[i]]); // fraction of child usage cannot exceed fraction of parent usage
	model.add(con2);

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
	{
		sol_objective = cplex.getObjValue();
		for (int i = 0; i < INTER; i++)
		{
			// record parent/child flow values
			parent_flow[i] = cplex.getValue(x[parent[i]]);
			child_flow[i] = cplex.getValue(x[child[i]]);
		}
		// calculate average fullness of all arc flows
		sol_load = 0;
		for (int i = 0; i < DENSITY; i++)
			sol_load += cplex.getValue(x[i]) / u[i];
		sol_load /= DENSITY;
	}
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