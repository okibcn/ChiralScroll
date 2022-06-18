#pragma once

namespace chiralscroll
{

class Scroller
{
public:
	virtual ~Scroller() = default;

	virtual void StartScrolling() = 0;
	virtual void Scroll(int amt) = 0;
	virtual void StopScrolling() = 0;
};

}  // namespace chiralscroll
