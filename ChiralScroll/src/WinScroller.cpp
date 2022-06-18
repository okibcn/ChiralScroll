#include "WinScroller.h"

#include <spdlog/spdlog.h>

#include "ChiralScrollException.h"

namespace chiralscroll
{

namespace
{

LRESULT MouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
	if(nCode == HC_ACTION && wParam == WM_MOUSEMOVE)
	{
		return 1;
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}

void WinScroller::StartScrolling()
{
	hookHandle_ = SetWindowsHookEx(WH_MOUSE_LL, MouseHook, nullptr, 0);
	THROW_IF_FALSE(hookHandle_, "Could not install mouse hook.");
	SPDLOG_INFO("Start scrolling session.");
}

void WinScroller::Scroll(int amt)
{
	INPUT input{};

	input.type = INPUT_MOUSE;
	input.mi.dx = 0;
	input.mi.dy = 0;
	input.mi.mouseData = amt;
	input.mi.dwFlags = dir_ == Direction::kVertical ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
	input.mi.time = 0;  //Windows will do the timestamp
	input.mi.dwExtraInfo = GetMessageExtraInfo();

	THROW_IF_FALSE(SendInput(1, &input, sizeof(INPUT)),
		GetErrorMessage(GetLastError()));
	SPDLOG_DEBUG("Scroll by {} {}.",
		amt, dir_ == Direction::kVertical ? "vertical" : "horizontal");
}

void WinScroller::StopScrolling()
{
	if(hookHandle_)
	{
		UnhookWindowsHookEx(hookHandle_);
		hookHandle_ = nullptr;
		SPDLOG_INFO("Stop scrolling session.");
	}
}

}  // namespace chiralscroll
