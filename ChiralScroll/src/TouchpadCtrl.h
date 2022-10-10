#pragma once

#include <wx/dcclient.h>
#include <wx/cursor.h>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/region.h>
#include <wx/window.h>

namespace chiralscroll
{

class TouchpadEvent : public wxEvent
{
public:
	TouchpadEvent(float value, wxEventType eventType, int id)
		: wxEvent(id, eventType),
		  value_(value) {}

	wxEvent* Clone() const override
	{
		return new TouchpadEvent(*this);
	}

	float GetValue()
	{
		return value_;
	}

private:
	float value_;
};


class TouchpadCtrl : public wxWindow
{
public:
	explicit TouchpadCtrl(
		wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0,
		const wxString& name = wxPanelNameStr);

	bool Enable(bool enable) override;

	void SetVerticalZone(float vScrollZone);
	void SetHorizontalZone(float hScrollZone);
	void SetValue(float vScrollZone, float hScrollZone);
	float GetVerticalZone() const;
	float GetHorizontalZone() const;
	std::pair<float, float> GetValue() const;

private:
	friend class Grabber;

	void DoSetVerticalZone(float vScrollZone);
	void DoSetHorizontalZone(float hScrollZone);

	void SetVerticalZonePixels(int vScrollZone);
	void SetHorizontalZonePixels(int hScrollZone);

	void StartVerticalEvent();
	void StartHorizontalEvent();

	void OnResize(wxSizeEvent& event);
	void OnPaint(wxPaintEvent& event);
	void Render();
	void Render(wxClientDC& dc);

	wxRegion GetClippingRegion(wxSize size);

	int GetVerticalZonePixels() const;
	int GetHorizontalZonePixels() const;

	// This is tuned to match the width of the slider knobs.
	static constexpr int kCornerSize = 13;

	Grabber& hGrabber_;
	Grabber& vGrabber_;

	wxRegion clippingRegion_;
	float vScrollZone_;
	float hScrollZone_;

	wxDECLARE_EVENT_TABLE();
};

wxDECLARE_EVENT(EVT_TOUCHPAD_VERTICAL, TouchpadEvent);
wxDECLARE_EVENT(EVT_TOUCHPAD_HORIZONTAL, TouchpadEvent);

}  // namespace chiralscroll
