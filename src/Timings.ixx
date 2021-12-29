module;

#include <chrono>
#include <spdlog/fmt/fmt.h>

export module Draw.Timings;
import Draw.Window;

// Properties
static std::chrono::high_resolution_clock s_timer;
static auto s_FirstFrame = s_timer.now();
static auto s_FrameStart = s_FirstFrame;
static float s_DeltaTime = 0.0F;
static float s_TimeSinceStart = 0.0F;

static float s_FrameTime { 0.0F };
static uint16_t s_FrameCount { 0 };

export namespace Timings
{

	void Update()
	{

		// Update `delta time` and `time since start`
		auto frameEnd = s_timer.now();
		using fdur = std::chrono::duration<float>;
		s_DeltaTime = std::chrono::duration_cast<fdur>(frameEnd - s_FrameStart).count();
		s_TimeSinceStart = std::chrono::duration_cast<fdur>(frameEnd - s_FirstFrame).count();
		s_FrameStart = frameEnd;

		// Performance monitor
		s_FrameCount++;
		s_FrameTime += s_DeltaTime;
		if (s_FrameTime >= 0.5F)
		{

			// Calculate performance values
			float fps = s_FrameCount / s_FrameTime;
			float mspf = 1000.0F / fps;
			
			// Update window title
			auto title = fmt::format("SchiffscheVersenky {:.3f} ms/f ({:.0f} f/s)", mspf, fps);
			Window::SetTitle(title.c_str());

			// Reset values
			s_FrameCount = 0;
			s_FrameTime = 0.0F;

		}


	}

	float DeltaTime() noexcept
	{
		return s_DeltaTime;
	}

	float TimeSinceStart() noexcept
	{
		return s_TimeSinceStart;
	}

}