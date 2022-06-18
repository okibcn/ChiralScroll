#include "ChiralScroll.h"

#include <string_view>

#include "HidUtils.h"
#include "Vector.h"

namespace chiralscroll
{

void ChiralScroll::SetSettings(const Settings& settings)
{
	settings_ = settings;
}

void ChiralScroll::ProcessTouch(const TouchDevice& device, const std::vector<TouchDevice::Contact>& contacts)
{
	const Settings::DeviceSettings& deviceSettings = settings_.GetDeviceSettings(device.name());
	if(!settings_.GetGlobalSettings().enabled || !deviceSettings.enabled)
	{
		// Settings could have changed during touch session, so we need to clear it.
		if(touchSession_)
		{
			touchSession_.reset();
		}
		return;
	}

	if(touchSession_ && &touchSession_->device() == &device)
	{
		if(!touchSession_->Update(contacts))
		{
			touchSession_.reset();
		}
	}
	else if(ShouldStartScrollingSession(deviceSettings, contacts))
	{
		StartScrollingSession(device, contacts);
	}

	// If there are any other contacts, start a non-scrolling session.
	if(!touchSession_ &&
	   std::any_of(contacts.begin(), contacts.end(), [](const auto& contact) { return contact.isTouch; }))
	{
		touchSession_ = std::make_unique<NonScrollSession>(device);
	}
}

// Only start scrolling if there is exactly one contact, it is the first
// contact, it is a positive contact (not a lift), and we are not within the
// lockout window.
bool ChiralScroll::ShouldStartScrollingSession(const Settings::DeviceSettings& deviceSettings, const std::vector<TouchDevice::Contact>& contacts)
{
	return contacts.size() == 1 &&
		contacts[0].id == 0 &&
		contacts[0].isTouch &&
		absl::ToInt64Milliseconds(absl::Now() - lastKeyboardTime_) > deviceSettings.typingLockoutMs;
}

void ChiralScroll::StartScrollingSession(const TouchDevice& device, const std::vector<TouchDevice::Contact>& contacts)
{
	const Settings::DeviceSettings& deviceSettings = settings_.GetDeviceSettings(device.name());
	const auto pointInScrollZone = [](ULONG point, LONG width, float frac)
	{
		return point > (1.0f - frac)*width;
	};

	const auto& contact = contacts[0];
	const TouchDevice::ContactInfo& contactInfo = device.GetContactInfo(contact.contactInfoLink);
	if(pointInScrollZone(
		contact.logicalX - contactInfo.logicalArea.left,
		contactInfo.logicalArea.right - contactInfo.logicalArea.left,
		deviceSettings.vScrollZone))
	{
		touchSession_ = std::make_unique<ScrollSession>(
			device,
			contact,
			Vector<float>(0.0f, 1.0f),
			-deviceSettings.vSens,
			settings_.GetGlobalSettings(),
			*vScroller_);
	}
	else if(pointInScrollZone(
		contact.logicalY - contactInfo.logicalArea.top,
		contactInfo.logicalArea.bottom - contactInfo.logicalArea.top,
		deviceSettings.hScrollZone))
	{
		touchSession_ = std::make_unique<ScrollSession>(
			device,
			contact,
			Vector<float>(1.0f, 0.0f),
			deviceSettings.hSens,
			settings_.GetGlobalSettings(),
			*hScroller_);
	}
}

void ChiralScroll::ProcessKeyboard()
{
	lastKeyboardTime_ = absl::Now();
	// Cancel any ongoing touch session.
	if(touchSession_)
	{
		touchSession_.reset();
	}
}

}  // namespace chiralscroll
