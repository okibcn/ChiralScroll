#pragma once

#include <memory>
#include <optional>
#include <Windows.h>

#include <absl/time/time.h>

#include "HidUtils.h"
#include "Scroller.h"
#include "Settings.h"
#include "TouchSession.h"
#include "Vector.h"

namespace chiralscroll
{

class ChiralScroll
{
public:
	ChiralScroll(
		const Settings& settings,
		std::unique_ptr<Scroller> vScroller,
		std::unique_ptr<Scroller> hScroller)
		: settings_(settings),
		  vScroller_(std::move(vScroller)),
		  hScroller_(std::move(hScroller)),
		  touchSession_(nullptr),
		  currentDevice_(nullptr),
		  lastKeyboardTime_(absl::InfinitePast()) {}

	void SetSettings(const Settings& settings);
	void ProcessTouch(const TouchDevice& device, const std::vector<TouchDevice::Contact>& contacts);
	void ProcessKeyboard();

private:
	bool ShouldStartScrollingSession(
		const Settings::DeviceSettings& deviceSettings,
		const std::vector<TouchDevice::Contact>& contacts);
	void StartScrollingSession(const TouchDevice& device, const std::vector<TouchDevice::Contact>& contacts);

	Settings settings_;
	std::unique_ptr<Scroller> vScroller_;
	std::unique_ptr<Scroller> hScroller_;
	std::unique_ptr<TouchSession> touchSession_;
	TouchDevice* currentDevice_;
	absl::Time lastKeyboardTime_;
};

}  // namespace chiralscroll
