#include "AIController.h"
#include "Timer.h"
#include "GameState.h"
#include <iostream>
#include <fstream>
#include <Windows.h>

#include "GameController.h"
#include <bitset>

using namespace std;

#define EXPORT false

#define TOURNAMENT_SELECTION 0
#define ROULETTE_SELECTION 1

#define ONE_POINT_CROSSOVER 0
#define UNIFORM_CROSSOVER 1

#define PARENT_COUNT 4
#define GENE_COUNT 12
#define MAX_TOWER_ATTEMPTS 200

#define SELECTION TOURNAMENT_SELECTION
#define CROSSOVER ONE_POINT_CROSSOVER

AIController::AIController()
{
	srand(time(NULL));
	m_gameController = nullptr;
	m_gameBoard = nullptr;
	m_Timer = nullptr;
	m_gameState = nullptr;
	_elapsedSeconds = 0;
	_currentScore = 0;

#if !EXPORT
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
#else

	std::ofstream o("export.csv");

	_currentGenerationNum = 0;
	o << ",0,1,2,3,4,5,6,7,8,9,10,11" << std::endl;

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
#endif
}

AIController::~AIController()
{

}

void AIController::gameOver()
{
	_currentGeneration["gene_" + to_string(_currentGene)]["score"] = _currentScore;
	_currentScore = 0;
	_currentGene++;
	_elapsedSeconds = 0;

	SaveCurrentGeneration();

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
}

void AIController::CreateNewGeneration()
{
	_currentGene = 0;
	_currentGenerationNum++;

	// Parent genes for next generation
	json winners[PARENT_COUNT];
	string winningGenes[PARENT_COUNT];

	// Selection
#if SELECTION == TOURNAMENT_SELECTION
		// Run tournaments until PARENT_COUNT have been chosen
	for (int round = 0; round < PARENT_COUNT; round++)
	{
		int max = round * (GENE_COUNT / PARENT_COUNT);
		// Compare the genes next to each other - does not need to select random genes to compare as they already have no impact on eachother
		// Start at 1 because 0 is already max by default
		for (int i = 1; i < GENE_COUNT / PARENT_COUNT; i++)
		{
			if (_currentGeneration["gene_" + to_string(i + round * (GENE_COUNT / PARENT_COUNT))]["score"]
		> _currentGeneration["gene_" + to_string(max)]["score"])
				max = i + round * (GENE_COUNT / PARENT_COUNT);
		}

		winners[round] = _currentGeneration["gene_" + to_string(max)];
		for (json tower : winners[round]["towers"])
		{
			winningGenes[round] += std::bitset<2>(tower["type"]).to_string();
			winningGenes[round] += std::bitset<5>(tower["x"]).to_string();
			winningGenes[round] += std::bitset<5>(tower["y"]).to_string();
		}
		OutputDebugStringA(to_string(max).c_str());
	}
	OutputDebugStringA("\n");
	_currentGeneration.clear();
#elif SELECTION == ROULETTE_SELECTION
#endif

	int gene = 0;
	for (int first = 0; first < PARENT_COUNT; first++)
		for (int second = 0; second < PARENT_COUNT; second++)
		{
			if (first == second)
				continue;
			std::string geneA = winningGenes[first];
			std::string geneB = winningGenes[second];
			std::string child;
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
			// Mutation
			for (int i = 0; i < child.size(); i++)
			{
				if (rand() % 100 < 1)
					child.replace(i, 1, child.at(i) == '0' ? "1" : "0");
			}

			while (!child.empty())
			{
				json tower;
				tower["type"] = stoi(child.substr(0, 2), nullptr, 2);
				tower["x"] = stoi(child.substr(2, 5), nullptr, 2) % 25;
				tower["y"] = stoi(child.substr(7, 5), nullptr, 2) % 17;
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
	double seconds = floor(m_Timer->elapsedSeconds());
	if (seconds > _elapsedSeconds)
	{
		_elapsedSeconds = seconds;

		int index = (int)seconds - 1;
		if (seconds <= MAX_TOWER_ATTEMPTS)
			addTower((TowerType)_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["type"],
				_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["x"],
				_currentGeneration["gene_" + to_string(_currentGene)]["towers"].at(index)["y"]);
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

void AIController::addTower(TowerType type, int gridx, int gridy)
{
	// grid position can be from 0,0 to 25,17
	/*
	enum class TowerType {
	empty, slammer, swinger, thrower };
	*/

	bool towerAdded = m_gameBoard->addTower(type, gridx, gridy);

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

	static int iteration = 0;

	if (iteration == 0)
		cout << "iteration" << "," << "wave" << "," << "kills" << "," << "score" << endl;

	cout << iteration << "," << m_gameState->getCurrentWave() << "," << m_gameState->getMonsterEliminated() << "," << score << endl;
	iteration++;

	m_gameState->setScore(score);

	return score;
}