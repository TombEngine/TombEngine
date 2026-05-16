#pragma once

#include "Game/control/control.h"

// This might not be the exact amount of time that has passed, but giving it a
// value of 1/30 keeps it in lock-step with the rest of the game logic,
// which assumes 30 iterations per second.
constexpr auto FPS		  = 30;
constexpr auto DELTA_TIME = 1.0f / FPS;

class HighFramerateSynchronizer
{
private:
	double _controlDelay = 0.0;
	double _frameTime    = 0.0;
	bool   _locked       = false;

	unsigned long long _lastCounter    = 0;
	unsigned long long _currentCounter = 0;
	unsigned long long _frequency      = 0; 

public:
	void Init();
	void Sync();
	void Step();
	bool Synced();
	bool Locked();
	float GetInterpolationFactor();
};

int	 TimeSync();
bool TimeInit();
bool TimeReset();

bool TestGlobalTimeInterval(unsigned int intervalGameFrames, unsigned int offsetGameFrames = 0);

unsigned int SecToGameFrames(float sec);
float		 GameFramesToSec(unsigned int gameFrames);

extern HighFramerateSynchronizer g_Synchronizer;
