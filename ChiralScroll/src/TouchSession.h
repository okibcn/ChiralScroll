#pragma once

#include <vector>
#include <Windows.h>

#include "HidUtils.h"
#include "Scroller.h"
#include "Settings.h"
#include "Vector.h"

namespace chiralscroll
{

class TouchSession
{
public:
	TouchSession(const TouchDevice& device) : device_(device) {}
	virtual ~TouchSession() = default;

	// Returns true if the touch session continues, false if it ends.
	virtual bool Update(const std::vector<TouchDevice::Contact>& contacts) = 0;

	const TouchDevice& device() const
	{
		return device_;
	}

private:
	const TouchDevice& device_;
};

class NonScrollSession : public TouchSession
{
public:
	NonScrollSession(const TouchDevice& device) : TouchSession(device) {}

	bool Update(const std::vector<TouchDevice::Contact>& contacts) override;
};

class ScrollSession : public TouchSession
{
public:
	ScrollSession(
		const TouchDevice& device,
		const TouchDevice::Contact& initialContact,
		Vector<float> initialDirection,
		float sens,
		const Settings::GlobalSettings& settings,
		Scroller& scroller);
	~ScrollSession();

	bool Update(const std::vector<TouchDevice::Contact>& contacts) override;

private:
	// Handles update when scrolling has not yet started, direction has not yet
	// been determined.
	void StartScrolling(const TouchDevice::Contact& contact);

	// Handles update after scrolling has started, direction has been
	// determined.
	void ContinueScrolling(const TouchDevice::Contact& contact);

	// Performs a scroll action.
	void Scroll(Vector<float> newDir, Vector<float> newPos);

	// Scale a vector by the contact area height so that different resolutions
	// will not affect sensitivity.
	Vector<float> ScaleVector(Vector<LONG> vector) const;

	ULONG contactId_;
	TouchDevice::ContactInfo contactInfo_;
	Vector<float> direction_;
	Vector<float> position_;
	float scrollDirection_;
	float sens_;
	Settings::GlobalSettings settings_;
	Scroller& scroller_;
};

}  // namespace chiralscroll
