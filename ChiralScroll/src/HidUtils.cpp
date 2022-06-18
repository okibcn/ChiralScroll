#include "HidUtils.h"

#include <functional>

#include <absl/strings/string_view.h>
#include <absl/strings/substitute.h>
#include <spdlog/spdlog.h>

#include "StringUtils.h"

namespace chiralscroll
{

namespace {

static constexpr USAGE HID_USAGE_DIGITIZER_CONFIDENCE = 0x47;
static constexpr USAGE HID_USAGE_DIGITIZER_CONTACT_ID = 0x51;
static constexpr USAGE HID_USAGE_DIGITIZER_CONTACT_COUNT = 0x54;
static constexpr USAGE HID_USAGE_DIGITIZER_SCAN_TIME = 0x56;


std::vector<RAWINPUTDEVICELIST> GetRidList()
{
	UINT rid_list_size = 0;
	GetRawInputDeviceList(nullptr, &rid_list_size, sizeof(RAWINPUTDEVICELIST));
	std::vector<RAWINPUTDEVICELIST> rid_list(rid_list_size);
	GetRawInputDeviceList(rid_list.data(), &rid_list_size, sizeof(RAWINPUTDEVICELIST));
	return rid_list;
}

struct MaybeContact
{
	struct MaybeArea
	{
		std::optional<long> top;
		std::optional<long> bottom;
		std::optional<long> left;
		std::optional<long> right;

		std::optional<TouchDevice::ContactInfo::Area> toContactArea() const
		{
			if(top && bottom && left && right)
			{
				return TouchDevice::ContactInfo::Area{*top, *bottom, *left, *right};
			}
			return std::nullopt;
		}
	};
	MaybeArea logicalArea;
	MaybeArea physicalArea;
};

std::vector<TouchDevice::ContactInfo> GetContactInfos(const HidDevice& hidDevice)
{
	absl::flat_hash_map<USHORT, MaybeContact> maybeContacts;
	for(const HIDP_VALUE_CAPS& cap : hidDevice.FindValueCaps({HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y}))
	{
		if(cap.IsAbsolute)
		{
			maybeContacts[cap.LinkCollection].logicalArea.top = cap.LogicalMin;
			maybeContacts[cap.LinkCollection].logicalArea.bottom = cap.LogicalMax;
			maybeContacts[cap.LinkCollection].physicalArea.top = cap.PhysicalMin;
			maybeContacts[cap.LinkCollection].physicalArea.bottom = cap.PhysicalMax;
		}
	}
	for(const HIDP_VALUE_CAPS& cap : hidDevice.FindValueCaps({HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X}))
	{
		if(cap.IsAbsolute)
		{
			maybeContacts[cap.LinkCollection].logicalArea.left = cap.LogicalMin;
			maybeContacts[cap.LinkCollection].logicalArea.right = cap.LogicalMax;
			maybeContacts[cap.LinkCollection].physicalArea.left = cap.PhysicalMin;
			maybeContacts[cap.LinkCollection].physicalArea.right = cap.PhysicalMax;
		}
	}

	std::vector<TouchDevice::ContactInfo> contacts;
	for(const auto& kv : maybeContacts)
	{
		const USHORT link = kv.first;
		const MaybeContact& maybe = kv.second;
		const std::optional<TouchDevice::ContactInfo::Area> logicalArea = maybe.logicalArea.toContactArea();
		const std::optional<TouchDevice::ContactInfo::Area> physicalArea = maybe.physicalArea.toContactArea();
		if(logicalArea && physicalArea)
		{
			SPDLOG_INFO("ContactInfo link={}, top={}, bottom={}, left={}, right={}",
			            link, logicalArea->top, logicalArea->bottom, logicalArea->left, logicalArea->right);
			contacts.push_back({link, *logicalArea, *physicalArea});
		}
	}
	return contacts;
}

std::string GetDeviceName(HANDLE hDevice)
{
	UINT deviceNameSize = 0;
	GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, nullptr, &deviceNameSize);
	std::wstring wName(deviceNameSize, '\n');
	GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, wName.data(), &deviceNameSize);
	wName.resize(deviceNameSize - 1);  // Trim null terminator.

	return WstringToString(wName);
}

std::vector<uint8_t> GetDevicePreparsedData(HANDLE hDevice)
{
	UINT preparsedSize = 0;
	GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, nullptr, &preparsedSize);
	std::vector<uint8_t> preparsedBytes(preparsedSize);
	GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, preparsedBytes.data(), &preparsedSize);
	return preparsedBytes;
}

RID_DEVICE_INFO GetDeviceInfo(HANDLE hDevice)
{
	RID_DEVICE_INFO info;
	UINT device_info_size = sizeof(RID_DEVICE_INFO);
	GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, &info, &device_info_size);
	return info;
}

HIDP_CAPS GetCaps(const RawInputDevice& device)
{
	HIDP_CAPS caps;
	THROW_IF_NTERROR(HidP_GetCaps(device.preparsedData(), &caps),
	                 absl::StrCat("In HidP_GetCaps for device ", ToAbslView(device.name())));
	return caps;
}

std::vector<HIDP_VALUE_CAPS> GetValueCaps(const HIDP_CAPS& caps, const RawInputDevice& device)
{
	USHORT numValueCaps = caps.NumberInputValueCaps;
	if(numValueCaps <= 0)
	{
		return {};
	}
	std::vector<HIDP_VALUE_CAPS> valueCaps(numValueCaps);
	THROW_IF_NTERROR(HidP_GetValueCaps(HidP_Input, valueCaps.data(), &numValueCaps, device.preparsedData()),
	                 absl::StrCat("In HidP_GetValueCaps for device ", ToAbslView(device.name())));
	valueCaps.resize(numValueCaps);
	return valueCaps;
}

std::vector<HIDP_BUTTON_CAPS> GetButtonCaps(const HIDP_CAPS& caps, const RawInputDevice& device)
{
	USHORT numButtonCaps = caps.NumberInputButtonCaps;
	if(numButtonCaps <= 0)
	{
		return {};
	}
	std::vector<HIDP_BUTTON_CAPS> buttonCaps(numButtonCaps);
	THROW_IF_NTERROR(HidP_GetButtonCaps(HidP_Input, buttonCaps.data(), &numButtonCaps, device.preparsedData()),
	                 absl::StrCat("In HidP_GetButtonCaps for device ", ToAbslView(device.name())));
	buttonCaps.resize(numButtonCaps);
	return buttonCaps;
}

template<typename T>
std::vector<std::reference_wrapper<const T>> FindCaps(const std::vector<T>& caps, Usage usage)
{
	std::vector<std::reference_wrapper<const T>> result;
	for(const auto& cap : caps)
	{
		if(cap.UsagePage == usage.page &&
		   ((cap.IsRange && cap.Range.UsageMin <= usage.id && cap.Range.UsageMax) ||
		    (!cap.IsRange && cap.NotRange.Usage == usage.id)))
		{
			result.push_back(std::ref(cap));
		}
	}
	return result;
}

template<typename T>
std::optional<USHORT> FindFirstLink(Usage usage, const std::vector<T>& caps)
{
	const auto& matchingCaps = FindCaps(caps, usage);
	if(matchingCaps.empty())
	{
		return std::nullopt;
	}
	return matchingCaps[0].get().LinkCollection;
}

}  // namespace


RawInputDevice::RawInputDevice(const HANDLE hDevice) :
	name_(GetDeviceName(hDevice)),
	preparsedBytes_(GetDevicePreparsedData(hDevice)),
	info_(GetDeviceInfo(hDevice)) {}


std::optional<HidDevice> HidDevice::FromHandle(const HANDLE hDevice)
{
	RawInputDevice rawDevice(hDevice);
	if(rawDevice.info().dwType != RIM_TYPEHID)
	{
		return std::nullopt;
	}
	return HidDevice(std::move(rawDevice));
}

HidDevice::HidDevice(RawInputDevice rawDevice) :
	RawInputDevice(std::move(rawDevice)),
	caps_(GetCaps(*this)),
	valueCaps_(GetValueCaps(caps_, *this)),
	buttonCaps_(GetButtonCaps(caps_, *this)) {}


std::vector<std::reference_wrapper<const HIDP_VALUE_CAPS>> HidDevice::FindValueCaps(Usage usage) const
{
	return FindCaps(valueCaps_, usage);
}

std::vector<std::reference_wrapper<const HIDP_BUTTON_CAPS>> HidDevice::FindButtonCaps(Usage usage) const
{
	return FindCaps(buttonCaps_, usage);
}

NTSTATUS HidDevice::GetLogicalValue(const HidData& hidData, Usage usage, USHORT link, ULONG* value) const
{
	return HidP_GetUsageValue(
		HidP_Input,
		usage.page,
		link,
		usage.id,
		value,
		preparsedData(),
		reinterpret_cast<PCHAR>(const_cast<uint8_t*>(hidData.rawData.data())),
		hidData.hid.dwSizeHid);
}

ULONG HidDevice::GetLogicalValue(const HidData& hidData, Usage usage, std::optional<USHORT> link) const
{
	if(!link)
	{
		link = FindFirstLink(usage, valueCaps_);
		if(!link)
		{
			throw ChiralScrollException(
				absl::Substitute("No link for usage ($0, $1, $2) found for device $3.",
					usage.page, usage.id, HidP_Input, ToAbslView(name())));
		}
	}
	ULONG value;
	THROW_IF_NTERROR(GetLogicalValue(hidData, usage, *link, &value),
		absl::StrCat("In HidP_GetUsageValue for device ", ToAbslView(name())));
	return value;
}

std::optional<ULONG> HidDevice::GetLogicalValueOrNullopt(const HidData& hidData, Usage usage, std::optional<USHORT> link) const
{
	if(!link)
	{
		link = FindFirstLink(usage, valueCaps_);
		if(!link)
		{
			return std::nullopt;
		}
	}
	ULONG value;
	NTSTATUS status = GetLogicalValue(hidData, usage, *link, &value);
	switch(status)
	{
		case HIDP_STATUS_SUCCESS:
			return value;
		case HIDP_STATUS_USAGE_NOT_FOUND:
			return std::nullopt;
		default:
			throw ChiralScrollException::FromNtstatus(
				status, absl::StrCat("In HidP_GetUsageValue for device ", ToAbslView(name())));
	}
}

NTSTATUS HidDevice::GetPhysicalValue(const HidData& hidData, Usage usage, USHORT link, LONG* value) const
{
	return HidP_GetScaledUsageValue(
		HidP_Input,
		usage.page,
		link,
		usage.id,
		value,
		preparsedData(),
		reinterpret_cast<PCHAR>(const_cast<uint8_t*>(hidData.rawData.data())),
		hidData.hid.dwSizeHid);
}

LONG HidDevice::GetPhysicalValue(const HidData& hidData, Usage usage, std::optional<USHORT> link) const
{
	if(!link)
	{
		link = FindFirstLink(usage, valueCaps_);
		if(!link)
		{
			throw ChiralScrollException(
				absl::Substitute("No link for usage ($0, $1, $2) found for device $3.",
					usage.page, usage.id, HidP_Input, ToAbslView(name())));
		}
	}
	LONG value;
	THROW_IF_NTERROR(GetPhysicalValue(hidData, usage, *link, &value),
		absl::StrCat("In HidP_GetScaledUsageValue for device ", ToAbslView(name())));
	return value;
}

std::optional<LONG> HidDevice::GetPhysicalValueOrNullopt(const HidData& hidData, Usage usage, std::optional<USHORT> link) const
{
	if(!link)
	{
		link = FindFirstLink(usage, valueCaps_);
		if(!link)
		{
			return std::nullopt;
		}
	}
	LONG value;
	NTSTATUS status = GetPhysicalValue(hidData, usage, *link, &value);
	switch(status)
	{
		case HIDP_STATUS_SUCCESS:
			return value;
		case HIDP_STATUS_USAGE_NOT_FOUND:
			return std::nullopt;
		default:
			throw ChiralScrollException::FromNtstatus(
				status, absl::StrCat("In HidP_GetScaledUsafeValue for device ", ToAbslView(name())));
	}
}

NTSTATUS HidDevice::GetUsages(const HidData& hidData, Usage usage, USHORT link, std::vector<USAGE>* usages) const
{
	ULONG numUsages = HidP_MaxUsageListLength(
		HidP_Input,
		usage.page,
		preparsedData());
	usages->resize(numUsages);
	NTSTATUS status = HidP_GetUsages(
		HidP_Input,
		usage.page,
		link,
		usages->data(),
		&numUsages,
		preparsedData(),
		reinterpret_cast<PCHAR>(const_cast<uint8_t*>(hidData.rawData.data())),
		hidData.hid.dwSizeHid);
	usages->resize(numUsages);
	return status;
}

// TODO: Try with HidP_GetUsageValue.
bool HidDevice::GetButton(const HidData& hidData, Usage usage, std::optional<USHORT> link) const
{
	if(!link)
	{
		link = FindFirstLink(usage, buttonCaps_);
		if(!link)
		{
			return false;
		}
	}
	std::vector<USAGE> usages;
	THROW_IF_NTERROR(GetUsages(hidData, usage, *link, &usages),
		absl::StrCat("In HidP_GetUsages for device ", ToAbslView(name())));
	return std::find(usages.begin(), usages.end(), usage.id) != usages.end();
}


std::optional<TouchDevice> TouchDevice::FromHandle(const HANDLE hDevice, bool panicOnUnexpectedInput)
{
	std::optional<HidDevice> hidDevice = HidDevice::FromHandle(hDevice);
	if(!hidDevice)
	{
		return std::nullopt;
	}
	if(hidDevice->info().hid.usUsagePage != HID_USAGE_PAGE_DIGITIZER ||
	   hidDevice->info().hid.usUsage != HID_USAGE_DIGITIZER_TOUCH_PAD)
	{
		return std::nullopt;
	}

	std::vector<ContactInfo> contacts = GetContactInfos(*hidDevice);
	const auto contactCountCaps =
		hidDevice->FindValueCaps({HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_CONTACT_COUNT});
	if(contacts.empty() || contactCountCaps.empty())
	{
		return std::nullopt;
	}
	return TouchDevice(std::move(*hidDevice), std::move(contacts), contactCountCaps[0].get().LinkCollection, panicOnUnexpectedInput);
}

const TouchDevice::ContactInfo& TouchDevice::GetContactInfo(ULONG link) const
{
	return *std::find_if(contactInfo_.begin(), contactInfo_.end(),
		[link](const TouchDevice::ContactInfo& info) {
			return info.link == link;
		});
}

std::optional<std::vector<TouchDevice::Contact>> TouchDevice::GetContacts(const HidData& hidData)
{
	const ULONG contactCount = GetLogicalValue(
		hidData,
		{HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_CONTACT_COUNT},
		linkContactCount_);

	if(!frameBuilder_.InProgress())
	{
		if(contactCount == 0)
		{
			// This can be caused by touchpad buttons clicking or releasing
			// without a touch. We don't want to bother tracking all of this,
			// so we just ignore these reports.
			return std::nullopt;
		}
		frameBuilder_.Start(contactCount);
		SPDLOG_DEBUG("Expecting {} contacts.", contactCount);
	}
	const auto contacts = GetContactsInReport(hidData);
	return frameBuilder_.AddReport(contacts);
}

std::vector<TouchDevice::Contact> TouchDevice::GetContactsInReport(const HidData& hidData)
{
	std::vector<Contact> contacts;
	contacts.reserve(contactInfo_.size());
	for(const auto& contactInfo : contactInfo_)
	{
		const std::optional<ULONG> contactId = GetLogicalValueOrNullopt(
			hidData,
			{HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_CONTACT_ID},
			contactInfo.link);
		if(contactId)
		{
			const bool isTouch = GetButton(
				hidData,
				{HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TIP_SWITCH},
				contactInfo.link);
			const bool confidence = GetButton(
				hidData,
				{HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_CONFIDENCE},
				contactInfo.link);
			const ULONG logicalX = GetLogicalValue(
				hidData,
				{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},
				contactInfo.link);
			const ULONG logicalY = GetLogicalValue(
				hidData,
				{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},
				contactInfo.link);
			const LONG physicalX = GetPhysicalValue(
				hidData,
				{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},
				contactInfo.link);
			const LONG physicalY = GetPhysicalValue(
				hidData,
				{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},
				contactInfo.link);
			contacts.push_back({*contactId, contactInfo.link, isTouch, confidence, logicalX, logicalY, physicalX, physicalY});
		}
	}
	SPDLOG_DEBUG("Report:");
	SPDLOG_TRACE("  button1={}, button2={}, button3={}",
		GetButton(hidData, {0x09, 0x01}), GetButton(hidData, {0x09, 0x02}), GetButton(hidData, {0x09, 0x03}));
	for(const auto& contact : contacts)
	{
		SPDLOG_DEBUG("  id={}, link={}, isTouch={}, confidence={}, x={}, y={}",
			contact.id, contact.contactInfoLink, contact.isTouch, contact.confidence, contact.logicalX, contact.logicalY);
	}
	return contacts;
}

void TouchDevice::FrameBuilder::Start(ULONG expectedContactCount)
{
	expectedContactCount_ = expectedContactCount;
	contacts_.reserve(expectedContactCount_);
}

bool TouchDevice::FrameBuilder::InProgress() const
{
	return expectedContactCount_ != 0;
}

std::optional<std::vector<TouchDevice::Contact>>
TouchDevice::FrameBuilder::AddReport(const std::vector<Contact>& newContacts)
{
	contacts_.insert(contacts_.end(), newContacts.begin(), newContacts.end());
	if(contacts_.size() >= expectedContactCount_)
	{
		return FinishFrame();
	}
	return std::nullopt;
}

// Returns the contacts from the current frame and clears the state in
// preparation for the next frame.
std::vector<TouchDevice::Contact> TouchDevice::FrameBuilder::FinishFrame()
{
	// For each non-touch contact, check for a matching last contact. If none
	// exists, remove this contact as it is bogus (it is not a touch or a lift).
	contacts_.erase(
		std::remove_if(contacts_.begin(), contacts_.end(), [this](const auto& contact) {
			return !contact.isTouch &&
				std::none_of(lastContacts_.begin(), lastContacts_.end(), [&contact](const auto& oldContact) {
					return oldContact.id == contact.id &&
					       oldContact.logicalX == contact.logicalX &&
					       oldContact.logicalY == contact.logicalY;
				});
		}),
		contacts_.end());
	if(contacts_.size() != expectedContactCount_)
	{
		const std::string msg =
			absl::Substitute("Wrong number of contacts in frame. Expected $0, got $1.",
			                 expectedContactCount_, contacts_.size());
		if(panicOnUnexpectedInput_)
		{
			throw ChiralScrollException(msg);
		}
		else
		{
			SPDLOG_WARN(msg);
		}
	}
	expectedContactCount_ = 0;
	lastContacts_ = std::move(contacts_);
	return lastContacts_;
}


std::optional<HidData> HidData::FromRawInput(const HRAWINPUT handle)
{
	UINT size = 0;
	if(GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) < 0) {
		throw ChiralScrollException("Error in GetRawInputData");
	}

	std::vector<uint8_t> raw_input_bytes(size);
	if(GetRawInputData(handle, RID_INPUT, raw_input_bytes.data(), &size, sizeof(RAWINPUTHEADER)) < 0) {
		throw ChiralScrollException("Error in GetRawInputData");
	}

	const auto& rawInput = *reinterpret_cast<RAWINPUT*>(raw_input_bytes.data());
	if(rawInput.header.dwType != RIM_TYPEHID)
	{
		return std::nullopt;
	}	
	return HidData(rawInput);
}


absl::flat_hash_map<HANDLE, TouchDevice> GetTouchDevices(bool panicOnUnexpectedInput)
{
	std::vector<RAWINPUTDEVICELIST> ridList = chiralscroll::GetRidList();
	absl::flat_hash_map<HANDLE, TouchDevice> touchDevices;

	for(const auto& rid : ridList)
	{
		auto touchDevice = TouchDevice::FromHandle(rid.hDevice, panicOnUnexpectedInput);

		if(touchDevice)
		{
			touchDevices.emplace(rid.hDevice, std::move(*touchDevice));
		}
	}
	return touchDevices;
}

}  // namespace chiralscroll
