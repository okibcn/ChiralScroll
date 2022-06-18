#include "StringUtils.h"

#include <Windows.h>

namespace chiralscroll
{

std::wstring StringToWstring(std::string_view str)
{
	int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
	std::wstring wStr(size, '\0');
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), wStr.data(), size);
	wStr.resize(static_cast<size_t>(size));
	return wStr;
}

std::string WstringToString(std::wstring_view wstr)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.length()), nullptr, 0, nullptr, nullptr);
	std::string str(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.length()), str.data(), size, nullptr, nullptr);
	str.resize(static_cast<size_t>(size));
	return str;
}

}  // namespace chiralscroll
