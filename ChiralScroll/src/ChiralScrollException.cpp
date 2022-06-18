#include "ChiralScrollException.h"

#include <Hidclass.h>
#include <hidsdi.h>

namespace chiralscroll
{

std::string NtstatusToString(const NTSTATUS status)
{
	switch(status)
	{
		case HIDP_STATUS_SUCCESS:
			return "HIDP_STATUS_SUCCESS";
		case HIDP_STATUS_INVALID_REPORT_LENGTH:
			return "HIDP_STATUS_INVALID_REPORT_LENGTH";
		case HIDP_STATUS_INVALID_REPORT_TYPE:
			return "HIDP_STATUS_INVALID_REPORT_TYPE";
		case HIDP_STATUS_INCOMPATIBLE_REPORT_ID:
			return "HIDP_STATUS_INCOMPATIBLE_REPORT_ID";
		case HIDP_STATUS_INVALID_PREPARSED_DATA:
			return "HIDP_STATUS_INVALID_PREPARSED_DATA";
		case HIDP_STATUS_USAGE_NOT_FOUND:
			return "HIDP_STATUS_USAGE_NOT_FOUND";
		case HIDP_STATUS_BAD_LOG_PHY_VALUES:
			return "HIDP_STATUS_BAD_LOG_PHY_VALUES";
		case HIDP_STATUS_NULL:
			return "HIDP_STATUS_NULL";
		case HIDP_STATUS_VALUE_OUT_OF_RANGE:
			return "HIDP_STATUS_VALUE_OUT_OF_RANGE";
		default:
			return "UNKNOWN_NTSTATUS";
	}
}

std::string HresultToString(HRESULT status)
{
	switch(status)
	{
		case S_OK:
			return "S_OK";
		default:
			return "UNKNOWN_HRESULT";
	}
}

std::string GetErrorMessage(DWORD errNo)
{
	wchar_t* message;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errNo, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&message, 0, nullptr);

	int size = WideCharToMultiByte(CP_UTF8, 0, message, -1, nullptr, 0, nullptr, nullptr);
	std::string strMessage(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, message, -1, strMessage.data(), size, nullptr, nullptr);
	strMessage.resize(static_cast<size_t>(size) - 1);

	LocalFree(message);  // Horrible Win32 APIs.
	return strMessage;
}

}  // namespace chiralscroll
