#pragma once

#include <string>
#include <string_view>

#include <absl/strings/string_view.h>

namespace chiralscroll
{

std::wstring StringToWstring(std::string_view str);
std::string WstringToString(std::wstring_view wstr);

inline absl::string_view ToAbslView(std::string_view stdView)
{
	return {stdView.data(), stdView.size()};
}


}  // namespace chiralscroll
