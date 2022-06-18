#pragma once

#include <Windows.h>

#include "Scroller.h"

namespace chiralscroll
{

class WinScroller : public Scroller
{
public:
	enum class Direction { kVertical, kHorizontal };

	WinScroller(Direction dir) : dir_(dir), hookHandle_(nullptr) {}
	virtual ~WinScroller() { StopScrolling(); }

	void StartScrolling() override;
	void Scroll(int amt) override;
	void StopScrolling() override;

private:
	const Direction dir_;
	HHOOK hookHandle_;
};

}  // namespace chiralscroll
