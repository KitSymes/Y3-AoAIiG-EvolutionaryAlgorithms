#include "AIController.h"
#include "Timer.h"
#include "GameState.h"
#include <iostream>
#include <fstream>
#include <Windows.h>

#include "GameController.h"
#include <bitset>

using namespace std;

#define EXPORT true
#define REPLAY false

#define TOURNAMENT_SELECTION 0
#define ROULETTE_SELECTION 1

#define ONE_POINT_CROSSOVER 0
#define UNIFORM_CROSSOVER 1

#define BIT_STRING_MUTATION 0
#define SHIFT_MUTATION 1

#define PARENT_COUNT 4
#define GENE_COUNT 16
#define MAX_TOWER_ATTEMPTS 200

#define MUTATION_RATE 100 // 1 in MUTATION_RATE chance to mutate

#define SELECTION ROULETTE_SELECTION
#define CROSSOVER UNIFORM_CROSSOVER
#define MUTATION BIT_STRING_MUTATION

AIController::AIController()
{
	srand(time(NULL));
	m_gameController = nullptr;
	m_gameBoard = nullptr;
	m_Timer = nullptr;
	m_gameState = nullptr;
	_elapsedSeconds = 0;
	_currentScore = 0;
	_framesPassed = 0;

#if EXPORT

	std::ofstream o("export.csv");

	_currentGenerationNum = 0;
	o << ",0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15" << std::endl;

	while (true)
	{
		std::ifstream f("generation_" + to_string(_currentGenerationNum) + ".json");
		if (!f.good())
			break;
		o << _currentGenerationNum << ",";
		_currentGeneration = json::parse(f);
		for (int gene = 0; gene < GENE_COUNT; gene++)
		{
			if (!_currentGeneration["gene_" + to_string(gene)].contains("score"))
				break;
			o << _currentGeneration["gene_" + to_string(gene)]["score"] << ",";
		}
		o << std::endl;
		_currentGenerationNum++;
	}

	o.close();
	exit(0);
#elif REPLAY
	_currentGenerationNum = 0;
	_currentGene = 0;
	std::ifstream f("generation_" + to_string(_currentGenerationNum) + ".json");
	_currentGeneration = json::parse(f);
#else
	_currentGenerationNum = -1;
	_currentGene = -1;
	while (_currentGene < 0)
	{
		std::ifstream f("generation_" + to_string(_currentGenerationNum + 1) + ".json");
		if (!f.good())
			break;
		_currentGenerationNum++;
		_currentGeneration = json::parse(f);
		for (int gene = 0; gene < GENE_COUNT; gene++)
			if (!_currentGeneration["gene_" + to_string(gene)].contains("score"))
			{
				_currentGene = gene;
				break;
			}
	}

	if (_currentGenerationNum < 0)
	{
		for (int gene = 0; gene < GENE_COUNT; gene++)
			for (int time = 0; time < MAX_TOWER_ATTEMPTS; time++)
			{
				json temp;
				temp["type"] = rand() % 4;
				temp["x"] = rand() % 25;
				temp["y"] = rand() % 17;
				_currentGeneration["gene_" + to_string(gene)]["towers"].push_back(temp);
			}

		_currentGenerationNum = 0;
		_currentGene = 0;

		SaveCurrentGeneration();
	}
	else if (_currentGene < 0)
	{
		// TODO Stopped after completing a generation but didn't create a new one
		CreateNewGeneration();
	}

	OutputDebugStringA(("Starting at " + to_string(_currentGenerationNum) + "\n").c_str());
#endif
}

AIController::~AIController()
{
	
}

void AIController::gameOver()
{
	_currentScore = recordScore();

	Log("[" + to_string(_currentGenerationNum) + "," + to_string(_currentGene) + "] Ended after " + to_string(_elapsedSeconds) +
		" seconds; Waves: " + to_string(m_gameState->getCurrentWave()) +
		", Kills: " + to_string(m_gameState->getMonsterEliminated()) +
		", Score: " + to_string(_currentScore) + "\n");

	_currentGeneration["gene_" + to_string(_currentGene)]["score"] = _currentScore;

	_currentScore = 0;
	_elapsedSeconds = 0;
	_framesPassed = 0;

#if !REPLAY
	SaveCurrentGeneration();
	_currentGene++;

	if (_currentGene >= GENE_COUNT)
	{
		// Winner Check
		{
			int max = 0;
			for (int i = 1; i < GENE_COUNT; i++)
			{
				if (_currentGeneration["gene_" + to_string(i)]["score"] > _currentGeneration["gene_" + to_string(max)]["score"])
					max = i;
			}

			if (_currentGeneration["gene_" + to_string(max)]["score"] >= 200)
			{
				_currentGeneration["_winner"] = max;

				SaveCurrentGeneration();

				exit(0);
				return;
			}
		}

		CreateNewGeneration();
	}
#endif
}

void AIController::CreateNewGeneration()
{
	// Generate a seed so that the results are repeatable
	unsigned int seed = unsigned int(time(NULL));
	if (_currentGeneration.contains("seed"))
		seed = _currentGeneration["seed"];
	else
		_currentGeneration["seed"] = seed;
	srand(seed);
	SaveCurrentGeneration();

	_currentGene = 0;
	_currentGenerationNum++;

	// Parent genes for next generation
	json winners[PARENT_COUNT];
	string winningGenes[PARENT_COUNT];

	// Selection
#if SELECTION == TOURNAMENT_SELECTION
	// Run tournaments until PARENT_COUNT have been chosen
	std::vector<int> temp;
	std::vector<std::vector<int>> tournament;

	// Fill temp with json indexes
	for (int i = 0; i < GENE_COUNT; i++)
		temp.push_back(i);

	// Fill out the tournament - there will be the same number of groups as parents
	for (int groupCount = 0; groupCount < PARENT_COUNT; groupCount++)
	{
		std::vector<int> group;
		Log("Group " + to_string(groupCount) + ": ");
		for (int i = 0; i < (GENE_COUNT / PARENT_COUNT); i++)
		{
			// Get a random index of temp
			int random = rand() % temp.size();
			// Add the json index at that temp index to the current group
			group.push_back(temp[random]);
			Log(to_string(temp[random]) + " (" + to_string(_currentGeneration["gene_" + to_string(temp[random])]["score"]) + ")");
			if (i < (GENE_COUNT / PARENT_COUNT) - 1)
				Log(", ");
			// Erase the temp index to prevent the json index from being chosen more than once
			temp.erase(temp.begin() + random);
		}
		// Add the group to the tournament
		tournament.push_back(group);
		Log("\n");
	}

	Log("Parents are: ");
	for (int group = 0; group < PARENT_COUNT; group++)
	{
		int max = tournament[group][0];
		// Compare the genes
		// Start at 1 because 0 is already max by default
		for (int i = 1; i < GENE_COUNT / PARENT_COUNT; i++)
		{
			int current = tournament[group][i];
			if (_currentGeneration["gene_" + to_string(current)]["score"]
		> _currentGeneration["gene_" + to_string(max)]["score"])
				max = current;
		}

		winners[group] = _currentGeneration["gene_" + to_string(max)];
		Log(to_string(max));
		if (group < PARENT_COUNT - 1)
			Log(", ");
	}
	Log("\n");
#elif SELECTION == ROULETTE_SELECTION

	int scoreTotal = 0;
	for (int i = 0; i < GENE_COUNT; i++)
		scoreTotal += _currentGeneration["gene_" + to_string(i)]["score"];

	for (int round = 0; round < PARENT_COUNT; round++)
	{
		int random = rand() % scoreTotal;
		int buffer = 0;
		for (int i = 0; i < GENE_COUNT; i++)
			// Less than because rand is between 0 inclusive and scoreTotal exclusive
			if (random < buffer + _currentGeneration["gene_" + to_string(i)]["score"])
			{
				winners[round] = _currentGeneration["gene_" + to_string(i)];
				break;
			}
			else
				buffer += _currentGeneration["gene_" + to_string(i)]["score"];
	}
#endif
	_currentGeneration.clear();

	for (int round = 0; round < PARENT_COUNT; round++)
		for (json tower : winners[round]["towers"])
		{
			winningGenes[round] += std::bitset<2>(tower["type"]).to_string();
			winningGenes[round] += std::bitset<5>(tower["x"]).to_string();
			winningGenes[round] += std::bitset<5>(tower["y"]).to_string();
		}

	int gene = 0;
	for (int first = 0; first < PARENT_COUNT; first++)
		for (int second = 0; second < PARENT_COUNT; second++)
		{
			std::string child;
			if (first == second)
				child = winningGenes[first];
			else
			{
				std::string geneA = winningGenes[first];
				std::string geneB = winningGenes[second];
				// Crossover
#if CROSSOVER == ONE_POINT_CROSSOVER
			// Gene sizes are the same
				child = geneA.substr(0, geneA.size() / 2);
				child += geneB.substr(geneA.size() / 2, string::npos);
#elif CROSSOVER == UNIFORM_CROSSOVER
			// A tower placement is 12 bits
				for (int i = 0; i < geneA.size() / 12; i++)
				{
					if (i % 2 == 0)
						child += geneA.substr(i * 12, 12);
					else
						child += geneB.substr(i * 12, 12);
				}
#endif
			}
			// Mutation
#if MUTATION == BIT_STRING_MUTATION
			for (int i = 0; i < child.size(); i++)
			{
				if (rand() % MUTATION_RATE < 1)
					child.replace(i, 1, child.at(i) == '0' ? "1" : "0");
			}
#endif

			while (!child.empty())
			{
				json tower;
				int type = stoi(child.substr(0, 2), nullptr, 2);
				int x = stoi(child.substr(2, 5), nullptr, 2);
				int y = stoi(child.substr(7, 5), nullptr, 2);
#if MUTATION == SHIFT_MUTATION
				if (rand() % MUTATION_RATE < 1)
				{
					int shift = (rand() % 3) - 1;
					type += shift;
				}
				if (rand() % MUTATION_RATE < 1)
				{
					int shift = (rand() % 3) - 1;
					x += shift;
				}
				if (rand() % MUTATION_RATE < 1)
				{
					int shift = (rand() % 3) - 1;
					y += shift;
				}
#endif
				tower["type"] = type % 4;
				tower["x"] = x % 25;
				tower["y"] = y % 17;
				_currentGeneration["gene_" + to_string(gene)]["towers"].push_back(tower);
				child.erase(0, 12);
			}
			gene++;
		}


	SaveCurrentGeneration();
}

void AIController::SaveCurrentGeneration()
{
	std::ofstream o("generation_" + to_string(_currentGenerationNum) + ".json");
	o << std::setw(4) << _currentGeneration << std::endl;
	o.close();
}

void AIController::update()
{
	if (m_Timer == nullptr)
		return;

	// HINT
	// a second has elapsed - your GA manager (GA Code) may decide to do something at this time...
	// static double elapsedSeconds = 0; // Who made this a static inline double? What a mistake
	//double seconds = floor(m_Timer->elapsedSeconds());

	//while (seconds > _elapsedSeconds)
	_framesPassed++;
	while (_framesPassed >= 60.0f)
	{
		_elapsedSeconds++;
		_framesPassed -= 60;

		int index = (int)_elapsedSeconds - 1;
		if (_elapsedSeconds <= MAX_TOWER_ATTEMPTS)
			if (addTower((TowerType)_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["type"],
				_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["x"],
				_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["y"]))
				Log("Added Tower at " + to_string(_elapsedSeconds) + "\n");
		//Log(to_string(_elapsedSeconds)) + "\n");
	}

	//GAManager::Instance()->Update(m_Timer->elapsedSeconds());

	// this might be useful? Monsters killed
	static int monstersKilled = 0;

	if (m_gameState->getMonsterEliminated() > monstersKilled)
	{
		monstersKilled = m_gameState->getMonsterEliminated();
	}

	_currentScore = recordScore();
}

bool AIController::addTower(TowerType type, int gridx, int gridy)
{
	// grid position can be from 0,0 to 25,17
	/*
	enum class TowerType {
	empty, slammer, swinger, thrower };
	*/

	return m_gameBoard->addTower(type, gridx, gridy);

	// NOTE towerAdded might be false if the tower can't be placed in that position, is there isn't enough funds
}

void AIController::setupBoard()
{
	m_Timer->start();
}

int AIController::recordScore()
{
	int currentWave = m_gameState->getCurrentWave();
	int killCount = m_gameState->getMonsterEliminated();
	currentWave *= 10; // living longer is good
	int score = currentWave + killCount;

	//static int iteration = 0;

	//if (iteration == 0)
		//cout << "iteration" << "," << "wave" << "," << "kills" << "," << "score" << endl;

	//cout << iteration << "," << m_gameState->getCurrentWave() << "," << m_gameState->getMonsterEliminated() << "," << score << endl;
	//iteration++;

	m_gameState->setScore(score);

	return score;
}

void AIController::Log(string output)
{
	cout << output;
	ofstream myfile;
	myfile.open("log.txt", ios::out | ios::app);
	myfile << output;
	myfile.close();
}