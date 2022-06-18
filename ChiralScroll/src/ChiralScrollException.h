#pragma once

#include <exception>
#include <string>
#include <string_view>
#include <Windows.h>

#include <absl/strings/string_view.h>
#include <absl/strings/substitute.h>

namespace chiralscroll
{

std::string NtstatusToString(NTSTATUS status);
std::string HresultToString(HRESULT status);
std::string GetErrorMessage(DWORD errNo);

class ChiralScrollException : public std::exception
{
public:
	ChiralScrollException(std::string_view what) : what_(what) {}
	ChiralScrollException(const std::exception& e, absl::string_view what)
		: what_(absl::Substitute("$0\nCaused by: $1", what, e.what())) {}

	static ChiralScrollException FromNtstatus(NTSTATUS status, absl::string_view what)
	{
		return ChiralScrollException(absl::Substitute("$0: $1", NtstatusToString(status), what));
	}

	static ChiralScrollException FromHresult(HRESULT result, absl::string_view what)
	{
		return ChiralScrollException(absl::Substitute("$0: $1", HresultToString(result), what));
	}

	const char* what() const override
	{
		return what_.c_str();
	}

private:
	std::string what_;
};

#define THROW_IF_NTERROR(expr, what)                                    \
    do                                                                  \
    {                                                                   \
        NTSTATUS status = (expr);                                       \
        if(status != HIDP_STATUS_SUCCESS)                               \
        {                                                               \
            throw ChiralScrollException::FromNtstatus(status, (what));  \
        }                                                               \
    } while(false)

#define THROW_IF_FALSE(expr, what)                \
    do                                            \
    {                                             \
        bool result = (expr);                     \
        if(!result)                               \
        {                                         \
            throw ChiralScrollException((what));  \
        }                                         \
    } while(false)

#define THROW_IF_HRESULT(expr, what)                                   \
    do                                                                 \
    {                                                                  \
        HRESULT result = (expr);                                       \
        if(result != S_OK)                                             \
        {                                                              \
            throw ChiralScrollException::FromHresult(result, (what));  \
        }                                                              \
    } while(false)

}  // namespace chiralscroll
