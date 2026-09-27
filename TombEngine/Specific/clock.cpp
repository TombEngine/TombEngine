#include "framework.h"

#include "Specific/clock.h"
#include "Specific/EngineMain.h"

constexpr auto CONTROL_FRAME_TIME = 1000.0f / 30.0f;
constexpr auto DEBUG_SKIP_FRAME_TIME = 10 * CONTROL_FRAME_TIME;

// Globals
double LdFreq = 0.0;
double LdSync = 0.0;

HighFramerateSynchronizer g_Synchronizer;

void HighFramerateSynchronizer::Init()
{
	_controlDelay = 0.0;
	_frameTime = 0.0;
	_locked = false;

	_frequency = SDL_GetPerformanceFrequency();
	_lastCounter = SDL_GetPerformanceCounter();
	_currentCounter = _lastCounter;
}

void HighFramerateSynchronizer::Sync()
{
	if (ResetClock)
	{
		ResetClock = false;
		_lastCounter = SDL_GetPerformanceCounter();
		_currentCounter = _lastCounter;
		_controlDelay = 0.0;
		_frameTime = 0.0;
	}
	else
	{
		_currentCounter = SDL_GetPerformanceCounter();
		auto diff = _currentCounter - _lastCounter;
		_lastCounter = _currentCounter;

		_frameTime = (double)diff * 1000.0 / (double)_frequency;
		_controlDelay += _frameTime;
	}

	_locked = true;
}

bool HighFramerateSynchronizer::Locked()
{
	return _locked;
}

bool HighFramerateSynchronizer::Synced()
{
#if _DEBUG
	if (_controlDelay >= DEBUG_SKIP_FRAME_TIME)
	{
		TENLog("Game loop is running too slow.", LogLevel::Warning);
		ResetClock = true;
		return false;
	}
#endif

	if (_controlDelay > CONTROL_FRAME_TIME &&
		_controlDelay <= CONTROL_FRAME_TIME * 2.0)
	{
		_locked = false;
	}

	return (_controlDelay >= CONTROL_FRAME_TIME);
}

void HighFramerateSynchronizer::Step()
{
	_controlDelay -= CONTROL_FRAME_TIME;
}

float HighFramerateSynchronizer::GetInterpolationFactor()
{
	return std::min((float)(_controlDelay / CONTROL_FRAME_TIME), 1.0f);
}

int TimeSync()
{
	auto counter = SDL_GetPerformanceCounter();
	double dCounter = (double)counter / LdFreq;

	int gameFrames = (int)dCounter - (int)LdSync;
	LdSync = dCounter;

	return gameFrames;
}

bool TimeReset()
{
	auto counter = SDL_GetPerformanceCounter();
	LdSync = (double)counter / LdFreq;
	return true;
}

bool TimeInit()
{
	auto freq = SDL_GetPerformanceFrequency();
	if (!freq)
		return false;

	LdFreq = (double)freq / (double)(FPS * 2);
	TimeReset();
	return true;
}


bool TestGlobalTimeInterval(unsigned int intervalGameFrames, unsigned int offsetGameFrames)
{
	if (offsetGameFrames >= intervalGameFrames)
	{
		TENLog("TestGlobalTimeInterval(): interval must be greater than offset.", LogLevel::Warning);
		return false;
	}

	return ((GlobalCounter % intervalGameFrames) == offsetGameFrames);
}

unsigned int SecToGameFrames(float sec)
{
	return ((unsigned int)round(sec * (float)FPS));
}

float GameFramesToSec(unsigned int gameFrames)
{
	return ((float)gameFrames / (float)FPS);
}
