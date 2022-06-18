#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <Windows.h>

// Must come after Windows.h
#include <Hidclass.h>
#include <hidsdi.h>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "framework.h"
#include "ChiralScrollException.h"

namespace chiralscroll
{

struct HidData;

struct Usage
{
	USAGE page;
	USAGE id;
	HIDP_REPORT_TYPE type = HidP_Input;
};

class RawInputDevice
{
public:
	explicit RawInputDevice(const HANDLE hDevice);
	RawInputDevice(RawInputDevice&&) = default;

	std::string_view name() const&
	{
		return name_;
	}

	PHIDP_PREPARSED_DATA preparsedData() const&
	{
		return reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(preparsedBytes_.data()));
	}

	const RID_DEVICE_INFO& info() const&
	{
		return info_;
	}

private:
	std::string name_;
	std::vector<uint8_t> preparsedBytes_;
	RID_DEVICE_INFO info_;
};

class HidDevice : public RawInputDevice
{
public:
	static std::optional<HidDevice> FromHandle(const HANDLE hDevice);

	std::vector<std::reference_wrapper<const HIDP_VALUE_CAPS>> FindValueCaps(Usage usage) const;
	std::vector<std::reference_wrapper<const HIDP_BUTTON_CAPS>> FindButtonCaps(Usage usage) const;

	ULONG GetLogicalValue(const HidData& hidData, Usage usage, std::optional<USHORT> link=std::nullopt) const;
	std::optional<ULONG> GetLogicalValueOrNullopt(const HidData& hidData, Usage usage, std::optional<USHORT> link = std::nullopt) const;
	LONG GetPhysicalValue(const HidData& hidData, Usage usage, std::optional<USHORT> link = std::nullopt) const;
	std::optional<LONG> GetPhysicalValueOrNullopt(const HidData& hidData, Usage usage, std::optional<USHORT> link = std::nullopt) const;
	bool GetButton(const HidData& hidData, Usage usage, std::optional<USHORT> link = std::nullopt) const;

private:
	explicit HidDevice(RawInputDevice rawDevice);

	NTSTATUS GetLogicalValue(const HidData& hidData, Usage usage, USHORT link, ULONG* value) const;
	NTSTATUS GetPhysicalValue(const HidData& hidData, Usage usage, USHORT link, LONG* value) const;
	NTSTATUS GetUsages(const HidData& hidData, Usage usage, USHORT link, std::vector<USAGE>* usages) const;

	HIDP_CAPS caps_;
	std::vector<HIDP_VALUE_CAPS> valueCaps_;
	std::vector<HIDP_BUTTON_CAPS> buttonCaps_;
};

class TouchDevice : public HidDevice
{
public:
	struct ContactInfo
	{
		struct Area
		{
			LONG top;
			LONG bottom;
			LONG left;
			LONG right;
		};

		USHORT link;
		Area logicalArea;
		Area physicalArea;
	};

	struct Contact
	{
		ULONG id;
		ULONG contactInfoLink;
		bool isTouch;
		bool confidence;
		ULONG logicalX;
		ULONG logicalY;
		LONG physicalX;
		LONG physicalY;
	};

	static std::optional<TouchDevice> FromHandle(const HANDLE hDevice, bool panicOnUnexpectedInput);

	TouchDevice(TouchDevice&&) = default;
	TouchDevice& operator=(TouchDevice&&) = default;
	TouchDevice(const TouchDevice&) = delete;
	TouchDevice& operator=(const TouchDevice&) = delete;

	const std::vector<ContactInfo>& contactInfo() const
	{
		return contactInfo_;
	}

	const ContactInfo& GetContactInfo(ULONG link) const;

	// Returns nullopt if the frame is not complete. Otherwise returns a list of
	// all contacts in the given frame. Throws an exception if anything goes
	// wrong.
	std::optional<std::vector<Contact>> GetContacts(const HidData& hidData);

private:
	struct FrameBuilder
	{
	public:
		FrameBuilder(bool panicOnUnexpectedInput) : 
			expectedContactCount_(0),
			panicOnUnexpectedInput_(panicOnUnexpectedInput) {}

		void Start(ULONG expectedContactCount);

		bool InProgress() const;

		// Adds the given contacts to this frame. If the frame is finished,
		// returns all contacts.
		std::optional<std::vector<Contact>> AddReport(const std::vector<Contact>& newContacts);

		// Returns the contacts from the current frame and clears the state in
		// preparation for the next frame.
		std::vector<Contact> FinishFrame();

	private:
		ULONG expectedContactCount_;
		std::vector<Contact> contacts_;
		std::vector<Contact> lastContacts_;
		bool panicOnUnexpectedInput_;
	};

	explicit TouchDevice(
		HidDevice hidDevice,
		std::vector<ContactInfo> contactInfo, 
		USHORT linkContactCount, 
		bool panicOnUnexpectedInput) :
			HidDevice(std::move(hidDevice)),
			contactInfo_(std::move(contactInfo)),
			linkContactCount_(linkContactCount),
			frameBuilder_(panicOnUnexpectedInput){}


	std::vector<Contact> GetContactsInReport(const HidData& hidData);

	std::vector<ContactInfo> contactInfo_;
	USHORT linkContactCount_;
	FrameBuilder frameBuilder_;
};

struct HidData
{
	// Returns nullopt if the handle is not an HID. Otherwise returns the HidData.
	static std::optional<HidData> FromRawInput(const HRAWINPUT handle);

	const RAWINPUTHEADER header;
	const RAWHID hid;
	const std::vector<uint8_t> rawData;

private:
	HidData(const RAWINPUT& raw_input) :
		header(raw_input.header),
		hid(raw_input.data.hid),
		rawData(raw_input.data.hid.bRawData, raw_input.data.hid.bRawData + static_cast<ptrdiff_t>(hid.dwSizeHid) * hid.dwCount) {}
};

absl::flat_hash_map<HANDLE, TouchDevice> GetTouchDevices(bool panicOnUnexpectedInput);

}  // namespace chiralscroll
