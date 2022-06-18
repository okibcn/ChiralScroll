#include "TouchSession.h"

#include <algorithm>

namespace chiralscroll
{

namespace
{

	// Returns the unsigned angle between the given vectors.
	double AngleBetween(Vector<float> a, Vector<float> b)
	{
		double x = acos(a*b/(a.Norm()*b.Norm()));
		return x;
	}

}  // namespace


bool NonScrollSession::Update(const std::vector<TouchDevice::Contact>& contacts)
{
	return std::any_of(contacts.begin(), contacts.end(),
		[](const auto& contact) { return contact.isTouch; });
}

ScrollSession::ScrollSession(
	const TouchDevice& device,
	const TouchDevice::Contact& initialContact,
	Vector<float> initialDirection,
	float sens,
	const Settings::GlobalSettings& globalSettings,
	Scroller& scroller)
	: TouchSession(device),
	  contactId_(initialContact.id),
	  contactInfo_(device.GetContactInfo(initialContact.contactInfoLink)),
	  direction_(initialDirection),
	  position_(ScaleVector(Vector<LONG>(initialContact.logicalX, initialContact.logicalY))),
	  scrollDirection_(0.0f),
	  sens_(sens),
	  settings_(globalSettings),
	  scroller_(scroller)
{
}

ScrollSession::~ScrollSession()
{
	scroller_.StopScrolling();
}

bool ScrollSession::Update(const std::vector<TouchDevice::Contact>& contacts)
{
	for(const auto& contact : contacts)
	{
		if(contact.id == contactId_)
		{
			if(!contact.isTouch)
			{
				return false;
			}
			if(scrollDirection_ == 0.0f)
			{
				StartScrolling(contact);
			}
			else
			{
				ContinueScrolling(contact);
			}
			return true;
		}
	}
	return false;
}

void ScrollSession::StartScrolling(const TouchDevice::Contact& contact)
{
	const Vector<float> newPos = ScaleVector(Vector<LONG>(contact.logicalX, contact.logicalY));
	const Vector<float> newDir = newPos - position_;
	const float dot = newDir*direction_;

	// Establish the scroll direction once we have moved more than
	// startDeadzone in the initial direction or backwards.
	if(AngleBetween(direction_, newDir) < settings_.startDeadzoneAngle/2 &&
	   dot > settings_.startDeadzone)
	{
		scrollDirection_ = 1.0f;
		scroller_.StartScrolling();
		Scroll(newDir, newPos);
	}
	else if(AngleBetween(direction_, -newDir) < settings_.startDeadzoneAngle/2 &&
	        dot < -settings_.startDeadzone)
	{
		scrollDirection_ = -1.0f;
		scroller_.StartScrolling();
		Scroll(newDir, newPos);
	}
}

void ScrollSession::ContinueScrolling(const TouchDevice::Contact& contact)
{
	const Vector<float> newPos = ScaleVector(Vector<LONG>(contact.logicalX, contact.logicalY));
	const Vector<float> newDir = newPos - position_;

	if(AngleBetween(direction_, -newDir) < settings_.reverseDeadzoneAngle/2)
	{
		// The distance must also be greater than reverseDeadzone before
		// changing the scroll direction.
		if(newDir.Norm() > settings_.reverseDeadzone)
		{
			scrollDirection_ *= -1.0f;
			Scroll(newDir, newPos);
		}
	}
	// To continue scrolling in the same direction the distance must be greater
	// than moveDeadzone in the current direction (dot product gives the
	// projection of newDir onto direction_) or greater than reverseDeadzone in
	// any other direction.
	else if(newDir.Norm() > settings_.reverseDeadzone || newDir*direction_ > settings_.moveDeadzone)
	{
		Scroll(newDir, newPos);
	}
}

void ScrollSession::Scroll(Vector<float> newDir, Vector<float> newPos)
{
	const double distance = newDir.Norm();
	const LONG contactAreaHeight = contactInfo_.logicalArea.bottom - contactInfo_.logicalArea.top;
	scroller_.Scroll(static_cast<int>(
		scrollDirection_
		* distance
		* sens_
		* settings_.sensScalingFactor
		* contactAreaHeight));
	position_ = newPos;
	direction_ = newDir/static_cast<float>(distance);
}

Vector<float> ScrollSession::ScaleVector(Vector<LONG> vector) const
{
	const LONG contactAreaHeight = contactInfo_.logicalArea.bottom - contactInfo_.logicalArea.top;
	return vector/static_cast<float>(contactInfo_.logicalArea.bottom - contactInfo_.logicalArea.top);
}

}  // namespace chiralscroll
