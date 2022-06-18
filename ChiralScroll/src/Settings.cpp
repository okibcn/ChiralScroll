#include "Settings.h"

#include <cmath>
#include <algorithm>
#include <charconv>
#include <exception>
#include <string_view>
#include <Windows.h>

#include <absl/strings/substitute.h>

#include "ChiralScrollException.h"
#include "StringUtils.h"

namespace chiralscroll
{

namespace
{

static constexpr struct
{
	// Global settings.
	// The constant 1784 was based on the resolution of my touchpad, the other
	// values are based on experimentation.
	bool enabled = true;
	float startDeadzone = 10.0f/1784;
	float startDeadzoneAngle = 3.14159f/4.0f;
	float moveDeadzone = 10.0f/1784;
	float reverseDeadzone = 20.0f/1784;
	float reverseDeadzoneAngle = 3.14159f/1.0f;
	float sensScalingFactor = 0.1f;

	// Device settings.
	int typingLockoutMs = 500;
	float vScrollZone = 0.1f;
	float hScrollZone = 0.1f;
	float vSens = 10.0f;
	float hSens = 10.0f;
} kDefaultSettings;

static constexpr DWORD kMaxBuffer = 64;
static constexpr DWORD kMaxSectionsBuffer = 1024;

// Read values from string.
template<typename T>
T FromWstring(const std::wstring& str);

template<>
bool FromWstring<bool>(const std::wstring& str)
{
	return str == L"true";
}
template<>
int FromWstring<int>(const std::wstring& str)
{
	return std::stoi(str);
}
template<>
float FromWstring<float>(const std::wstring& str)
{
	return std::stof(str);
}
template<>
std::string FromWstring<std::string>(const std::wstring& str)
{
	return WstringToString(str);
}
template<>
std::wstring FromWstring<std::wstring>(const std::wstring& str)
{
	return str;
}


// Write values to string.
template<typename T>
std::wstring ToWstring(T value)
{
	return std::to_wstring(value);
}

std::wstring ToWstring(bool value) {
	return value ? L"true" : L"false";
}
std::wstring ToWstring(std::string_view str) {
	return StringToWstring(str);
}
std::wstring ToWstring(std::wstring str) {
	return str;
}


class IniSection
{
public:
	IniSection(std::filesystem::path path, std::wstring section)
		: path_(std::move(path)), section_(std::move(section)) {}

	template<typename T>
	T ReadSetting(const std::wstring& key, const T& def) const
	{
		std::wstring str(kMaxBuffer, '\0');
		const DWORD size = GetPrivateProfileString(
			section_.c_str(),
			key.c_str(),
			ToWstring(def).c_str(),
			str.data(),
			kMaxBuffer,
			path_.c_str());
		str.resize(size);
		try
		{
			return FromWstring<T>(str);
		}
		catch(const std::exception& e)
		{
			throw ChiralScrollException(
				e,
				absl::Substitute(
					"Error parsing $0. Could not parse: $2",
					WstringToString(key),
					WstringToString(str)));
		}
	}

	template<typename T>
	const IniSection& WriteSetting(const std::wstring& key, const T& value) const
	{
		THROW_IF_FALSE(
			WritePrivateProfileString(
				section_.c_str(),
				key.c_str(),
				ToWstring(value).c_str(),
				path_.c_str()),
			absl::StrCat("WritePrivateProfileString: ", GetErrorMessage(GetLastError())));
		return *this;
	}

private:
	const std::filesystem::path path_;
	const std::wstring section_;
};

class IniFile
{
public:
	IniFile(std::filesystem::path path) : path_(path) {}

	IniSection GetSection(std::wstring section) const
	{
		return IniSection(path_, std::move(section));
	}

private:
	const std::filesystem::path path_;
};

}

#define READ_SETTING(var) ReadSetting(L#var, kDefaultSettings.var)

Settings Settings::FromFile(const std::filesystem::path& path, const std::vector<std::string>& devices)
{
	const IniFile iniFile(path);
	const IniSection globalSection = iniFile.GetSection(L"Global Settings");

	Settings settings;
	settings.globalSettings_ = {
		globalSection.READ_SETTING(enabled),
		globalSection.READ_SETTING(startDeadzone),
		globalSection.READ_SETTING(startDeadzoneAngle),
		globalSection.READ_SETTING(moveDeadzone),
		globalSection.READ_SETTING(reverseDeadzone),
		globalSection.READ_SETTING(reverseDeadzoneAngle),
		globalSection.READ_SETTING(sensScalingFactor),
	};

	for(const auto& device : devices)
	{
		const IniSection iniSection = iniFile.GetSection(StringToWstring(device));
		settings.deviceSettings_[device] = {
			iniSection.READ_SETTING(enabled),
			iniSection.READ_SETTING(typingLockoutMs),
			iniSection.READ_SETTING(vScrollZone),
			iniSection.READ_SETTING(hScrollZone),
			iniSection.READ_SETTING(vSens),
			iniSection.READ_SETTING(hSens),
		};
	}
	return settings;
}

#define WRITE_SETTING(settings, var) WriteSetting(L#var, (settings).var)

void Settings::ToFile(const std::filesystem::path& path) const
{
	const IniFile iniFile(path);
	iniFile.GetSection(L"Global Settings").WRITE_SETTING(globalSettings_, enabled);

	for(const auto& pair : deviceSettings_)
	{
		std::wstring name = StringToWstring(pair.first);
		const DeviceSettings& settings = pair.second;
		iniFile.GetSection(name)
			.WRITE_SETTING(settings, enabled)
			.WRITE_SETTING(settings, typingLockoutMs)
			.WRITE_SETTING(settings, vScrollZone)
			.WRITE_SETTING(settings, hScrollZone)
			.WRITE_SETTING(settings, vSens)
			.WRITE_SETTING(settings, hSens);
	}
}

Settings::GlobalSettings& Settings::GetGlobalSettings()
{
	return globalSettings_;
}

absl::flat_hash_map<std::string, Settings::DeviceSettings>& Settings::GetDeviceSettings()
{
	return deviceSettings_;
}

Settings::DeviceSettings& Settings::GetDeviceSettings(std::string_view deviceName)
{
	absl::string_view abslName = ToAbslView(deviceName);
	if(deviceSettings_.contains(std::string(deviceName)))
	{
		return deviceSettings_.at(abslName);
	}
	DeviceSettings& settings = deviceSettings_[abslName];
	settings = {
		kDefaultSettings.enabled,
		kDefaultSettings.typingLockoutMs,
		kDefaultSettings.vScrollZone,
		kDefaultSettings.hScrollZone,
		kDefaultSettings.vSens,
		kDefaultSettings.hSens,
	};
	return settings;
}

}  // namespace chiralscroll

