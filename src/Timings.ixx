module;

#include <chrono>

export module Draw.Timings;

// Properties
static std::chrono::high_resolution_clock s_timer;
static auto s_firstFrame = s_timer.now();
static auto s_frameStart = s_firstFrame;
static float s_deltaTime = 0.0F;
static float s_time = 0.0F;

export namespace Timings
{

	void update()
	{
		auto frameEnd = s_timer.now();
		using fdur = std::chrono::duration<float>;
		s_deltaTime = std::chrono::duration_cast<fdur>(frameEnd - s_frameStart).count();
		s_time = std::chrono::duration_cast<fdur>(frameEnd - s_firstFrame).count();
		s_frameStart = frameEnd;
	}

	float deltaTime() noexcept
	{
		return s_deltaTime;
	}

	float time() noexcept
	{
		return s_time;
	}

}