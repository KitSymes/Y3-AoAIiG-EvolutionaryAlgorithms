#pragma once

#include "Tower.h"
#include <json.hpp>
using json = nlohmann::json;

class GameController;
class GameBoard;
class Timer;
class GameState;

class AIController
{
public:
	AIController();
	~AIController();

	void setupBoard();
	int  recordScore();

	void setGameController(GameController* gameController) { m_gameController = gameController; }
	void setGameBoard(GameBoard* gameBoard) { m_gameBoard = gameBoard; }
	void setTimer(Timer* timer) { m_Timer = timer; }
	void setGameState(GameState* gameState) { m_gameState = gameState; }
	void update();
	void addTower(TowerType type, int gridx, int gridy);
	void gameOver();

	void CreateNewGeneration();
	void SaveCurrentGeneration();

private:
	GameController* m_gameController;
	GameBoard*		m_gameBoard;
	Timer*			m_Timer;
	GameState*		m_gameState;

	double _elapsedSeconds;
	json _currentGeneration;
	int _currentGenerationNum;
	int _currentGene;
	int _currentScore;
};

