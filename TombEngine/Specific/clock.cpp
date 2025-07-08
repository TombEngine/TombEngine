#include "framework.h"

#include <chrono>
#include "Specific/clock.h"
#include "winmain.h"

constexpr auto CONTROL_FRAME_TIME = 1000.0f / 30.0f;
constexpr auto DEBUG_SKIP_FRAME_TIME = 10 * CONTROL_FRAME_TIME;

// Globals
double LdFreq = 0.0;
double LdSync = 0.0;

HighFramerateSynchronizer g_Synchronizer;

void HighFramerateSynchronizer::Init()
{
	_controlDelay = 0;
	_frameTime = 0;

	_lastTime.QuadPart = 0;
	_currentTime.QuadPart = 0;
	_frequency.QuadPart = 0;

	QueryPerformanceFrequency(&_frequency);
	QueryPerformanceCounter(&_lastTime);
}

void HighFramerateSynchronizer::Sync()
{
	if (App.ResetClock)
	{
		App.ResetClock = false;
		QueryPerformanceCounter(&_lastTime);
		_currentTime = _lastTime;
		_controlDelay = 0;
		_frameTime = 0;
	}
	else
	{
		QueryPerformanceCounter(&_currentTime);
		_frameTime = (_currentTime.QuadPart - _lastTime.QuadPart) * 1000.0 / _frequency.QuadPart;
		_lastTime = _currentTime;
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
		App.ResetClock = true;
		return false;
	}
#endif

	// If frameskip is in action, lock flag will remain set until synchronizer is 
	// about to break out from it. This flag is later reused in input polling to
	// prevent engine from de-registering input events prematurely.

	if (_controlDelay > CONTROL_FRAME_TIME && _controlDelay <= CONTROL_FRAME_TIME * 2)
		_locked = false;

	return (_controlDelay >= CONTROL_FRAME_TIME);
}

void HighFramerateSynchronizer::Step()
{
	_controlDelay -= CONTROL_FRAME_TIME;
}

float HighFramerateSynchronizer::GetInterpolationFactor()
{
	return std::min((float)_controlDelay / (float)CONTROL_FRAME_TIME, 1.0f);
}

int TimeSync()
{
	auto ct = LARGE_INTEGER{};
	QueryPerformanceCounter(&ct);

	double dCounter = (double)ct.LowPart + (double)ct.HighPart * (double)0xffffffff;
	dCounter /= LdFreq;

	long gameFrames = (long)dCounter - (long)LdSync;
	LdSync = dCounter;
	return gameFrames;
}

bool TimeReset()
{
	auto fq = LARGE_INTEGER{};
	QueryPerformanceCounter(&fq);

	LdSync = (double)fq.LowPart + ((double)fq.HighPart * (double)0xffffffff);
	LdSync /= LdFreq;
	return true;
}

bool TimeInit()
{
	auto fq = LARGE_INTEGER{};
	if (!QueryPerformanceFrequency(&fq))
		return false;

	LdFreq = (double)fq.LowPart + ((double)fq.HighPart * (double)0xffffffff);
	LdFreq /= 60.0;
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
	return ((unsigned int)round(sec / (float)FPS));
}

float GameFramesToSec(unsigned int gameFrames)
{
	return ((float)gameFrames / (float)FPS);
}
