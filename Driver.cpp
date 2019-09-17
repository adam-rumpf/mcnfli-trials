/*
The main driver for generating MCNFLI computational trials. We use the portable RNG included in NETGEN to generate the seeds for our random networks.
*/

#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include "NetgenRandom.h"
using namespace std;

// Global values
const string netgen_file_name = "temp_network.min";
const int cutoff = 500; // cutoff for RR tries
const string temp_file_name = "temp_results.txt";
const string parent_flow_name = "temp_parent_flow.txt";
const string child_flow_name = "temp_child_flow.txt";
int netgen_restarts = 0; // number of times we had to restart NETGEN
int infeasible_milps = 0; // number of infeasible MILPs generated

// Constant NETGEN parameters
const int MINCOST = 1;
const int MAXCOST = 100;
const int SUPPLY = 10000;
const int TSOURCES = 0;
const int TSINKS = 0;
const int HICOST = 100;
const int CAPACITATED = 100;
const int MINCAP = 100;
const int MAXCAP = 500;
const int repeats = 60;

// Prototypes
int call_netgen(long, int, int, double, int);
int call_milp();
int call_lp();
int call_rr(long, int);

int main()
{
	// Output statistics
	int rrc0_tries, rrc1_tries, rrc5_tries, rrp0_tries, rrp1_tries, rrp5_tries, rrf_tries;
	double milp_cost, milp_time, lp_cost, lp_time, rrc0_cost, rrc1_cost, rrc5_cost, rrp0_cost, rrp1_cost, rrp5_cost, rrf_cost;

	NetgenRandom * rand_main = new NetgenRandom(time(NULL)); // random number to use as the NETGEN seed

	const int node_set[] = { 256, 512, 1024 }; // m
	const int multi_set[] = { 4, 8, 12 }; // multi
	const double node_frac_set[] = { 0.02, 0.05, 0.1, 0.15 }; // fractions of interdependencies for parent nodes
	const double arc_frac_set[] = { 0.01, 0.02, 0.05, 0.1 }; // fractions of interdependencies for parent arcs
	int type = 1; // 0 for node parents, 1 for arc parents

	// Arc parent trials
	for (int m : node_set) // { 256, 512, 1024 }
	{
		for (int multi : multi_set) // {4, 8, 12 }
		{
			for (double fraction : arc_frac_set) // { 0.01, 0.02, 0.05, 0.1 }
			{
				string result_file_name = "results" + to_string(rand_main->random(10000000, 99999999)) + ".txt";
				ofstream outfile;
				outfile.open(result_file_name);
				outfile << fixed;

				if (outfile.is_open())
				{
					for (int i = 0; i < repeats; i++) // 60
					{
						long seed = rand_main->random(1, 99999999); // choose RNG seed for this instance
						cout << "\n\n\n============================================================\n";
						cout << "m = " << m << ", multi = " << multi << ", fraction = " << fraction << ", arc parents\n";
						cout << "Iteration " << i + 1 << '/' << repeats << ", seed = " << seed << '\n';
						cout << "============================================================\n";
						cout << "\nCalling NETGEN... ";
						if (call_netgen(seed, m, multi, fraction, type) == 0) // call NETGEN, and move on only if it worked
						{
							cout << "Succsessful!\n\nSolving MILP... ";
							if (call_milp() == 0) // call MILP solver, and move on only if it worked
							{
								// Read in MILP results
								ifstream milpin;
								milpin.open(temp_file_name);
								if (milpin.is_open())
								{
									string line;
									getline(milpin, line);
									milp_cost = stod(line);
									getline(milpin, line);
									milp_time = stod(line);
									milpin.close();

									cout << "Successful!\n\nSolving LP... ";
									call_lp(); // call LP solver

									// Read in LP results
									ifstream lpin;
									lpin.open(temp_file_name);
									if (lpin.is_open())
									{
										string line;
										getline(lpin, line);
										lp_cost = stod(line);
										getline(lpin, line);
										lp_time = stod(line);
										lpin.close();

										cout << "Successful!\n\nSolving RRC0... ";

										int latest_tries; // number of tries reported by the RR methods
										
										// RRC Trials

										// RRC0
										latest_tries = call_rr(seed, 1, 0);
										if (latest_tries > 0)
										{
											// Read in RRC0 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc0_cost = stod(line);
												getline(rrin, line);
												rrc0_time = stod(line);
												rrin.close();

												rrc0_tries = latest_tries;

												cout << "Successful!\n\nSolving RRC1... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC0 failed
											rrc0_cost = rrc0_tries = -999;
											cout << "Timed out.\n\nSolving RRC1... ";
										}

										// RRC1
										latest_tries = call_rr(seed, 1, 0.01);
										if (latest_tries > 0)
										{
											// Read in RRC1 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc1_cost = stod(line);
												getline(rrin, line);
												rrc1_time = stod(line);
												rrin.close();

												rrc1_tries = latest_tries;

												cout << "Successful!\n\nSolving RRC5... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC1 failed
											rrc1_cost = rrc1_tries = -999;
											cout << "Timed out.\n\nSolving RRC5... ";
										}

										// RRC5
										latest_tries = call_rr(seed, 1, 0.05);
										if (latest_tries > 0)
										{
											// Read in RRC5 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc5_cost = stod(line);
												getline(rrin, line);
												rrc5_time = stod(line);
												rrin.close();

												rrc5_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP0... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC5 failed
											rrc5_cost = rrc5_tries = -999;
											cout << "Timed out.\n\nSolving RRP0... ";
										}

										// RRP0
										latest_tries = call_rr(seed, 2, 0);
										if (latest_tries > 0)
										{
											// Read in RRP0 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp0_cost = stod(line);
												getline(rrin, line);
												rrp0_time = stod(line);
												rrin.close();

												rrp0_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP1... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP0 failed
											rrp0_cost = rrp0_tries = -999;
											cout << "Timed out.\n\nSolving RRP1... ";
										}

										// RRP1
										latest_tries = call_rr(seed, 2, 0.01);
										if (latest_tries > 0)
										{
											// Read in RRP1 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp1_cost = stod(line);
												getline(rrin, line);
												rrp1_time = stod(line);
												rrin.close();

												rrp1_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP5... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP1 failed
											rrp1_cost = rrp1_tries = -999;
											cout << "Timed out.\n\nSolving RRP5... ";
										}

										// RRP5
										latest_tries = call_rr(seed, 2, 0.05);
										if (latest_tries > 0)
										{
											// Read in RRP5 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp5_cost = stod(line);
												getline(rrin, line);
												rrp5_time = stod(line);
												rrin.close();

												rrp5_tries = latest_tries;

												cout << "Successful!\n\nSolving RRF... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP5 failed
											rrp5_cost = rrp5_tries = -999;
											cout << "Timed out.\n\nSolving RRF... ";
										}

										// RRF
										latest_tries = call_rr(seed, 3, 0);
										if (latest_tries > 0)
										{
											// Read in RRF results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrf_cost = stod(line);
												getline(rrin, line);
												rrf_time = stod(line);
												rrin.close();

												rrf_tries = latest_tries;

												cout << "Successful!\n";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRF failed
											rrf_cost = rrf_tries = -999;
											cout << "Timed out.\n";
										}
									}
									else
									{
										cout << "Output file failed to open.  Quitting.\n\a";
										return -1;
									}
								}
								else
								{
									cout << "Output file failed to open.  Quitting.\n\a";
									return -1;
								}
							}
							else
							{
								cout << "MILP infeasible.  Creating a different instance.\n";
								infeasible_milps++;
								i--;
								continue;
							}
							// Write row of results to output file
							outfile << seed << '\t' << m << '\t' << multi * m << '\t' << ceil(fraction * multi * m) << '\t' << type << '\t' << milp_cost << '\t' << milp_time << '\t' << lp_cost << '\t' << lp_time << '\t' << rrc_cost << '\t' << rrc_time << '\t' << rrc_tries << '\t' << rrp_cost << '\t' << rrp_time << '\t' << rrp_tries << '\t' << rrf_cost << '\t' << rrf_time << '\t' << rrf_tries << '\n';
						}
						else
						{
							cout << "NETGEN failed to create output file.  Quitting.\n\a";
							return -1;
						}
					}

					outfile.close();
				}
				else
				{
					cout << "NETGEN failed to create output file.  Quitting.\n\a";
					return -1;
				}
			}
		}
	}

	// Node parent trials
	type = 0;
	for (int m : node_set) // { 256, 512, 1024 }
	{
		for (int multi : multi_set) // {4, 8, 12 }
		{
			for (double fraction : node_frac_set) // { 0.02, 0.05, 0.1, 0.15 }
			{
				string result_file_name = "results" + to_string(rand_main->random(10000000, 99999999)) + ".txt";
				ofstream outfile;
				outfile.open(result_file_name);
				outfile << fixed;

				if (outfile.is_open())
				{
					for (int i = 0; i < repeats; i++) // 60
					{
						long seed = rand_main->random(1, 99999999); // choose RNG seed for this instance
						cout << "\n\n\n============================================================\n";
						cout << "m = " << m << ", multi = " << multi << ", fraction = " << fraction << ", node parents\n";
						cout << "Iteration " << i + 1 << '/' << repeats << ", seed = " << seed << '\n';
						cout << "============================================================\n";
						cout << "\nCalling NETGEN... ";
						if (call_netgen(seed, m, multi, fraction, type) == 0) // call NETGEN, and move on only if it worked
						{
							cout << "Succsessful!\n\nSolving MILP... ";
							if (call_milp() == 0) // call MILP solver, and move on only if it worked
							{
								// Read in MILP results
								ifstream milpin;
								milpin.open(temp_file_name);
								if (milpin.is_open())
								{
									string line;
									getline(milpin, line);
									milp_cost = stod(line);
									getline(milpin, line);
									milp_time = stod(line);
									milpin.close();

									cout << "Successful!\n\nSolving LP... ";
									call_lp(); // call LP solver

									// Read in LP results
									ifstream lpin;
									lpin.open(temp_file_name);
									if (lpin.is_open())
									{
										string line;
										getline(lpin, line);
										lp_cost = stod(line);
										getline(lpin, line);
										lp_time = stod(line);
										lpin.close();

										cout << "Successful!\n\nSolving RRC0... ";

										int latest_tries; // number of tries reported by the RR methods

										// RRC Trials

										// RRC0
										latest_tries = call_rr(seed, 1, 0);
										if (latest_tries > 0)
										{
											// Read in RRC0 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc0_cost = stod(line);
												getline(rrin, line);
												rrc0_time = stod(line);
												rrin.close();

												rrc0_tries = latest_tries;

												cout << "Successful!\n\nSolving RRC1... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC0 failed
											rrc0_cost = rrc0_tries = -999;
											cout << "Timed out.\n\nSolving RRC1... ";
										}

										// RRC1
										latest_tries = call_rr(seed, 1, 0.01);
										if (latest_tries > 0)
										{
											// Read in RRC1 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc1_cost = stod(line);
												getline(rrin, line);
												rrc1_time = stod(line);
												rrin.close();

												rrc1_tries = latest_tries;

												cout << "Successful!\n\nSolving RRC5... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC1 failed
											rrc1_cost = rrc1_tries = -999;
											cout << "Timed out.\n\nSolving RRC5... ";
										}

										// RRC5
										latest_tries = call_rr(seed, 1, 0.05);
										if (latest_tries > 0)
										{
											// Read in RRC5 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrc5_cost = stod(line);
												getline(rrin, line);
												rrc5_time = stod(line);
												rrin.close();

												rrc5_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP0... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRC5 failed
											rrc5_cost = rrc5_tries = -999;
											cout << "Timed out.\n\nSolving RRP0... ";
										}

										// RRP0
										latest_tries = call_rr(seed, 2, 0);
										if (latest_tries > 0)
										{
											// Read in RRP0 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp0_cost = stod(line);
												getline(rrin, line);
												rrp0_time = stod(line);
												rrin.close();

												rrp0_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP1... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP0 failed
											rrp0_cost = rrp0_tries = -999;
											cout << "Timed out.\n\nSolving RRP1... ";
										}

										// RRP1
										latest_tries = call_rr(seed, 2, 0.01);
										if (latest_tries > 0)
										{
											// Read in RRP1 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp1_cost = stod(line);
												getline(rrin, line);
												rrp1_time = stod(line);
												rrin.close();

												rrp1_tries = latest_tries;

												cout << "Successful!\n\nSolving RRP5... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP1 failed
											rrp1_cost = rrp1_tries = -999;
											cout << "Timed out.\n\nSolving RRP5... ";
										}

										// RRP5
										latest_tries = call_rr(seed, 2, 0.05);
										if (latest_tries > 0)
										{
											// Read in RRP5 results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrp5_cost = stod(line);
												getline(rrin, line);
												rrp5_time = stod(line);
												rrin.close();

												rrp5_tries = latest_tries;

												cout << "Successful!\n\nSolving RRF... ";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRP5 failed
											rrp5_cost = rrp5_tries = -999;
											cout << "Timed out.\n\nSolving RRF... ";
										}

										// RRF
										latest_tries = call_rr(seed, 3, 0);
										if (latest_tries > 0)
										{
											// Read in RRF results
											ifstream rrin;
											rrin.open(temp_file_name);
											if (rrin.is_open())
											{
												string line;
												getline(rrin, line);
												rrf_cost = stod(line);
												getline(rrin, line);
												rrf_time = stod(line);
												rrin.close();

												rrf_tries = latest_tries;

												cout << "Successful!\n";
											}
											else
											{
												cout << "Output file failed to open.  Quitting.\n\a";
												return -1;
											}
										}
										else
										{
											// RRF failed
											rrf_cost = rrf_tries = -999;
											cout << "Timed out.\n";
										}
									}
									else
									{
										cout << "Output file failed to open.  Quitting.\n\a";
										return -1;
									}
								}
								else
								{
									cout << "Output file failed to open.  Quitting.\n\a";
									return -1;
								}
							}
							else
							{
								cout << "MILP infeasible.  Creating a different instance.\n";
								infeasible_milps++;
								i--;
								continue;
							}
							// Write row of results to output file
							outfile << seed << '\t' << m << '\t' << multi * m << '\t' << ceil(fraction * ceil(0.2 * m)) << '\t' << type << '\t' << milp_cost << '\t' << milp_time << '\t' << lp_cost << '\t' << lp_time << '\t' << rrc_cost << '\t' << rrc_time << '\t' << rrc_tries << '\t' << rrp_cost << '\t' << rrp_time << '\t' << rrp_tries << '\t' << rrf_cost << '\t' << rrf_time << '\t' << rrf_tries << '\n';
						}
						else
						{
							cout << "NETGEN failed to create output file.  Quitting.\n\a";
							return -1;
						}
					}

					outfile.close();
				}
				else
				{
					cout << "NETGEN failed to create output file.  Quitting.\n\a";
					return -1;
				}
			}
		}
	}

	cout << "\n\n\nAll tests run!\nNETGEN restarted " << netgen_restarts << " times.\n";
	cout << infeasible_milps << " infeasible MILPs generated.\n\nPress[Enter] to close.\n\a";
	cin.get();
	delete rand_main;
	return 0;
}

/*
Calls NETGEN to generate a network with the specified variable parameters.  Returns the output of the NETGEN program (0 if successful). [seed] [node count] [arcs per node] [interdependencies per sink] [parent type]
*/
int call_netgen(long rng_seed, int NODES, int d, double r, int PARENT)
{
	const int max_tries = 500; // maximum number of times to try calling NETGEN
	int counter = 0;
	int return_val;

	int SOURCES = ceil(0.2 * NODES);
	int SINKS = ceil(0.2 * NODES);
	int DENSITY = d * NODES;
	int TOTSUPPLY = SUPPLY * ceil(NODES / 256);
	int INTER;
	if (PARENT == 0)
		INTER = ceil(r * SINKS);
	else
		INTER = ceil(r * DENSITY);

	string netgen_base = "..\\Netgen"; // replace the ".." with the necessary file path

	/*
	NETGEN arguments (17): [file name] [seed] [node count] [source count] [sink count] [arc count] [min arc cost] [max arc cost] [total supply] [trans sources] [trans sinks] [% max cost skeleton arcs] [% capacitated skeleton arcs] [min capacity] [max capacity] [0/1 for parent nodes/arcs] [interdependency count]
	*/
	string netgen_args = ' ' + netgen_file_name + ' ' + to_string(rng_seed) + ' ' + to_string(NODES) + ' ' + to_string(SOURCES) + ' '
		+ to_string(SINKS) + ' ' + to_string(DENSITY) + ' ' + to_string(MINCOST) + ' ' + to_string(MAXCOST) + ' ' + to_string(TOTSUPPLY)
		+ ' ' + to_string(TSOURCES) + ' ' + to_string(TSINKS) + ' ' + to_string(HICOST) + ' ' + to_string(CAPACITATED) + ' '
		+ to_string(MINCAP) + ' ' + to_string(MAXCAP) + ' ' + to_string(PARENT) + ' ' + to_string(INTER);
	string netgen_full = netgen_base + netgen_args;
	const char * n1 = netgen_full.c_str();

	return return_val;
}

/*
Calls program to solve the MILP version of the current .min file.  Returns the output of the program (0 if successful). Requires no input. Outputs objective and time.
*/
int call_milp()
{
	string milp_base = "..\\MilpSolver"; // replace the ".." with the necessary file path
	/*
	MILP solver arguments (2): [input file name] [output file name]
	*/
	string milp_args = ' ' + netgen_file_name + ' ' + temp_file_name;
	string milp_full = milp_base + milp_args;
	const char * n1 = milp_full.c_str();

	return system(n1);
}

/*
Calls program to solve the LP version of the current .min file.  Returns the output of the program (0 if successful). Requires no input. Outputs objective, time, and the fraction of each parents'/childs' capacity used
*/
int call_lp()
{
	string lp_base = "..\\LpSolver"; // replace the ".." with the necessary file path
	/*
	LP solver arguments (4): [input file name] [output file name] [parent flow file name] [child flow file name]
	*/
	string lp_args = ' ' + netgen_file_name + ' ' + temp_file_name + ' ' + parent_flow_name + ' ' + child_flow_name;
	string lp_full = lp_base + lp_args;
	const char * n1 = lp_full.c_str();

	return system(n1);
}

/*
Calls program to solve a specified RR version of the current .min file. Repeatedly attempts to solve the problem until either finding the solution or reaching the cutoff. Returns the number of tries if successful, or a negative value if not. Input is a random seed to use to initialize the randomized selection, the number of the desired RR method (1 for RRC, 2 for RRP, 3 for RRF), and finally a bound used to define the RR rule (the value of epsilon to restrict probabilities to the interval [epsilon,1-epsilon], so for example RRP1 uses a bound of 0.01). Outputs objective, time, and number of tries required
*/
int call_rr(long seed, int mode, double bound)
{
	string rr_base = "..\\RrSolver"; // replace the ".." with the necessary file path
	NetgenRandom * rand_sub = new NetgenRandom(seed); // random number to use in the output file name
	int count = 0;
	int output = -1;
	
	// Loop until solving the problem or reaching the cutoff
	while (output != 0 && count < cutoff)
	{
		cout << "\nAttempt " << count + 1 << '\n';

		/*
		RR solver arguments (5): [input file name] [output file name] [parent flow file name] [child flow file name] [seed] [bound]
		*/
		string rr_args = ' ' + netgen_file_name + ' ' + temp_file_name + ' ' + parent_flow_name + ' ' + child_flow_name + ' '
			+ to_string(rand_sub->random(1, 99999999)) + ' ' + to_string(mode) + ' ' + to_string(bound);
		string rr_full = rr_base + rr_args;
		const char * n1 = rr_full.c_str();

		output = system(n1);
		count++;
	}

	delete rand_sub;

	if (output != 0)
		return -1; // timed out
	else
		return count; // number of tries before success
}