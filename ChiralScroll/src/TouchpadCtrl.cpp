#include "TouchpadCtrl.h"

#include <algorithm>
#include <wx/colour.h>
#include <wx/dcmemory.h>
#include <wx/utils.h>

namespace chiralscroll
{

namespace
{

wxEventFunction CastEventHandler(void (TouchpadCtrl::*callback)(wxMouseEvent&))
{
	return wxEventFunctionCast(static_cast<wxMouseEventFunction>(callback));
}

}  // namespace

class Grabber : public wxWindow
{
public:
	enum class Direction { VERTICAL, HORIZONTAL };

	Grabber(TouchpadCtrl& parent, Direction direction, wxWindowID id=wxID_ANY)
		: wxWindow(&parent, id, wxDefaultPosition, {kGrabberSize, kGrabberSize}),
		  parent_(parent),
		  direction_(direction),
		  grabbed_(false) 
	{
		SetCursor(direction == Direction::VERTICAL ? wxCURSOR_SIZEWE : wxCURSOR_SIZENS);
	}

private:
	void OnLeftDown(wxMouseEvent& event)
	{
		CaptureMouse();
		grabbed_ = true;
		event.Skip();
	}

	void OnLeftUp(wxMouseEvent& event)
	{
		ReleaseMouse();
		grabbed_ = false;
		event.Skip();
	}

	void OnMove(wxMouseEvent& event)
	{
		if(grabbed_)
		{
			if(direction_ == Direction::VERTICAL)
			{
				parent_.SetVerticalZonePixels(GetPosition().x + event.GetX());
			}
			else
			{
				parent_.SetHorizontalZonePixels(GetPosition().y + event.GetY());
			}
		}
	}

	void OnCaptureLost(wxMouseCaptureLostEvent& event)
	{
	}

	static constexpr int kGrabberSize = 7;
	TouchpadCtrl& parent_;
	const Direction direction_;
	bool grabbed_;

	wxDECLARE_EVENT_TABLE();
};


wxBEGIN_EVENT_TABLE(Grabber, wxWindow)
	EVT_LEFT_DOWN(Grabber::OnLeftDown)
	EVT_LEFT_UP(Grabber::OnLeftUp)
	EVT_MOTION(Grabber::OnMove)
	EVT_MOUSE_CAPTURE_LOST(Grabber::OnCaptureLost)
wxEND_EVENT_TABLE()


TouchpadCtrl::TouchpadCtrl(
	wxWindow* parent,
	wxWindowID id,
	const wxPoint& pos,
	const wxSize& size,
	long style,
	const wxString& name)
	: wxWindow(parent, id, pos, size, style, name),
	  vGrabCursor_(wxCURSOR_SIZEWE),
	  hGrabCursor_(wxCURSOR_SIZENS),
	  vGrabber_(*new Grabber(*this, Grabber::Direction::VERTICAL)),
	  hGrabber_(*new Grabber(*this, Grabber::Direction::HORIZONTAL)),
	  clippingRegion_(GetClippingRegion(GetClientSize())),
	  vScrollZone_(0.0f),
	  hScrollZone_(0.0f),
	  currentGrabber_(nullptr)
{
	Render();
};

void TouchpadCtrl::SetVerticalZone(float vScrollZone)
{
	DoSetVerticalZone(vScrollZone);
	Render();
	StartVerticalEvent();
}

void TouchpadCtrl::SetHorizontalZone(float hScrollZone)
{
	DoSetHorizontalZone(hScrollZone);
	Render();
	StartHorizontalEvent();
}

void TouchpadCtrl::SetValue(float vScrollZone, float hScrollZone)
{
	DoSetVerticalZone(vScrollZone);
	DoSetHorizontalZone(hScrollZone);
	Render();
	StartVerticalEvent();
	StartHorizontalEvent();
}

float TouchpadCtrl::GetVerticalZone() const
{
	return vScrollZone_;
}

float TouchpadCtrl::GetHorizontalZone() const
{
	return hScrollZone_;
}

std::pair<float, float> TouchpadCtrl::GetValue() const
{
	return std::make_pair(vScrollZone_, hScrollZone_);
}

void TouchpadCtrl::DoSetVerticalZone(float vScrollZone)
{
	vScrollZone = std::clamp(vScrollZone, 0.0f, 1.0f);
	vScrollZone_ = vScrollZone;
	vGrabber_.SetPosition(wxPoint{GetVerticalZonePixels(), GetClientSize().GetHeight()/2} - vGrabber_.GetSize()/2);
}

void TouchpadCtrl::DoSetHorizontalZone(float hScrollZone)
{
	hScrollZone = std::clamp(hScrollZone, 0.0f, 1.0f);
	hScrollZone_ = hScrollZone;
	hGrabber_.SetPosition(wxPoint{GetClientSize().GetWidth()/2, GetHorizontalZonePixels()} - hGrabber_.GetSize()/2);
}

void TouchpadCtrl::SetVerticalZonePixels(int vScrollZone)
{
	SetVerticalZone(1 - static_cast<float>(vScrollZone)/GetClientSize().GetWidth());
}

void TouchpadCtrl::SetHorizontalZonePixels(int hScrollZone)
{
	SetHorizontalZone(1 - static_cast<float>(hScrollZone)/GetClientSize().GetHeight());
}

void TouchpadCtrl::StartVerticalEvent()
{
	TouchpadEvent event(vScrollZone_, EVT_TOUCHPAD_VERTICAL, this->GetId());
	ProcessEvent(event);
}

void TouchpadCtrl::StartHorizontalEvent()
{
	TouchpadEvent event(hScrollZone_, EVT_TOUCHPAD_HORIZONTAL, this->GetId());
	ProcessEvent(event);
}

void TouchpadCtrl::OnResize(wxSizeEvent& event)
{
	clippingRegion_ = GetClippingRegion(event.GetSize());
}

void TouchpadCtrl::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc{this};
	Render(dc);
}

void TouchpadCtrl::Render()
{
	wxClientDC dc{this};
	Render(dc);
}

void TouchpadCtrl::Render(wxClientDC& dc)
{
	const wxSize size = GetClientSize();
	const int width = size.GetWidth();
	const int height = size.GetHeight();

	wxBrush touchpadBrush = *wxGREY_BRUSH;
	wxBrush vZoneBrush = *wxGREEN_BRUSH;
	vZoneBrush.SetStyle(wxBRUSHSTYLE_FDIAGONAL_HATCH);
	wxBrush hZoneBrush = *wxRED_BRUSH;
	hZoneBrush.SetStyle(wxBRUSHSTYLE_BDIAGONAL_HATCH);

	// Draw the background.
	dc.SetDeviceClippingRegion(clippingRegion_);

	dc.SetBrush(touchpadBrush);
	dc.SetPen(wxNullPen);
	dc.DrawRoundedRectangle(wxPoint{0, 0}, size, kCornerSize);

	if(IsEnabled())
	{
		vGrabber_.SetBackgroundColour(*wxBLACK);
		hGrabber_.SetBackgroundColour(*wxBLACK);

		int vScrollZone = GetVerticalZonePixels();
		int hScrollZone = GetHorizontalZonePixels();

		wxPoint vScrollTopLeft{vScrollZone, 0};
		wxPoint vScrollBottomRight{width, height};
		wxPoint vScrollBottomLeft{vScrollZone, height};
		wxPoint hScrollTopLeft{0, hScrollZone};
		wxPoint hScrollBottomRight{vScrollZone, height};
		wxPoint hScrollTopRight{vScrollZone, hScrollZone};

		// Draw scroll zone outlines.
		if(hScrollZone_ > 0)
		{
			dc.SetPen(*wxRED_PEN);
			dc.DrawLine(hScrollTopLeft, hScrollTopRight);
		}
		if(vScrollZone_ > 0)
		{
			dc.SetPen(*wxGREEN_PEN);
			dc.DrawLine(vScrollTopLeft, vScrollBottomLeft);
		}
		else
		{
			// We need to extend the horizontal zone line so the floodfill doesn't leak.
			dc.DrawLine(hScrollTopRight, wxPoint{width, hScrollZone});
		}

		// Fill in scroll zones.
		if(hScrollZone_ > 0)
		{
			dc.SetBrush(hZoneBrush);
			dc.FloodFill((hScrollTopLeft + hScrollBottomRight)/2, touchpadBrush.GetColour());
		}
		if(vScrollZone_ > 0)
		{
			dc.SetBrush(vZoneBrush);
			dc.FloodFill((vScrollTopLeft + vScrollBottomRight)/2, touchpadBrush.GetColour());
		}
	}
	else  // !IsEnabled()
	{
		// Blend into background.
		vGrabber_.SetBackgroundColour(touchpadBrush.GetColour());
		hGrabber_.SetBackgroundColour(touchpadBrush.GetColour());
	}
}

wxRegion TouchpadCtrl::GetClippingRegion(wxSize size)
{
	wxBitmap bitmap(size, 1);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);
	dc.SetPen(*wxWHITE_PEN);
	dc.SetBrush(*wxWHITE_BRUSH);
	dc.DrawRoundedRectangle(wxPoint{0, 0}, size, kCornerSize);
	return wxRegion(bitmap, *wxBLACK);
}

int TouchpadCtrl::GetVerticalZonePixels() const
{
	return static_cast<int>(GetClientSize().GetWidth()*(1 - vScrollZone_));
}

int TouchpadCtrl::GetHorizontalZonePixels() const
{
	return static_cast<int>(GetClientSize().GetHeight()*(1 - hScrollZone_));
}

wxBEGIN_EVENT_TABLE(TouchpadCtrl, wxWindow)
	EVT_PAINT(TouchpadCtrl::OnPaint)
	EVT_SIZE(TouchpadCtrl::OnResize)
wxEND_EVENT_TABLE()

wxDEFINE_EVENT(EVT_TOUCHPAD_VERTICAL, TouchpadEvent);
wxDEFINE_EVENT(EVT_TOUCHPAD_HORIZONTAL, TouchpadEvent);

}  // namespace chiralscroll
