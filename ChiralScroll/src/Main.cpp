// Necessary for LoadIconMetric.
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <wx/taskbar.h>
#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/msw/private.h>
#include <wx/msw/wrapwin.h>
#include <wx/taskbar.h>
#include <wx/valnum.h>

// Must come after wrapwin.h
#include <commctrl.h>
#include <hidusage.h>

#include "ChiralScroll.h"
#include "ChiralScrollException.h"
#include "framework.h"
#include "HidUtils.h"
#include "resource.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "StringUtils.h"
#include "WinScroller.h"

#define MAX_LOADSTRING 100

using chiralscroll::HidData;
using chiralscroll::TouchDevice;
using chiralscroll::WinScroller;

static constexpr char kTitle[] = "ChiralScroll";

namespace chiralscroll
{

std::filesystem::path GetCurrentDirectory()
{
	const DWORD size = ::GetCurrentDirectory(0, nullptr);
	std::wstring str(size, '\0');
	::GetCurrentDirectory(size, str.data());
	str.resize(size - 1);
	return std::filesystem::path(str);
}

class ChiralScrollFrame : public wxFrame
{
private:
	class NotificationIcon : public wxTaskBarIcon
	{
	public:
		NotificationIcon(ChiralScrollFrame& frame) : frame_(frame)
		{
			wxIcon icon;
			THROW_IF_FALSE(icon.CreateFromHICON(LoadIcon(wxGetInstance(), MAKEINTRESOURCE(IDI_CHIRALSCROLL))),
				"CreateFromHICON failed.");
			SetIcon(icon, kTitle);
		}

		wxMenu* CreatePopupMenu() override
		{
			wxMenu* menu = new wxMenu();
			menu->AppendCheckItem(PU_ENABLE, "Enable");
			menu->Append(PU_SETTINGS, "Settings");
			menu->AppendSeparator();
			menu->Append(PU_CLOSE, "Close");

			menu->Check(PU_ENABLE, frame_.settings_.GetGlobalSettings().enabled);

			return menu;
		}

		void OnClick(wxTaskBarIconEvent& event)
		{
			PopupMenu(CreatePopupMenu());
		}

		void OnEnable(wxCommandEvent& event)
		{
			frame_.ToggleEnabled();
		}

		void OnSettings(wxCommandEvent& event)
		{
			frame_.ShowSettings();
		}

		void OnClose(wxCommandEvent& event)
		{
			frame_.Close();
		}

		wxDECLARE_EVENT_TABLE();

	private:
		enum
		{
			PU_ENABLE,
			PU_SETTINGS,
			PU_CLOSE,
		};

		ChiralScrollFrame& frame_;
	};

	class SettingsDialogImpl : public SettingsDialog
	{
	public:
		SettingsDialogImpl(ChiralScrollFrame& frame)
			: SettingsDialog(&frame),
			  settings_(frame.settings_),
			  deviceSettings_(nullptr),
			  frame_(frame)
		{
			touchpadCtrl_->Bind(EVT_TOUCHPAD_VERTICAL, &SettingsDialogImpl::OnVerticalZone, this);
			touchpadCtrl_->Bind(EVT_TOUCHPAD_HORIZONTAL, &SettingsDialogImpl::OnHorizontalZone, this);
			for(const auto& pair : settings_.GetDeviceSettings())
			{
				deviceSelector_->Append(pair.first);
			}
			deviceSelector_->SetSelection(0);
			SelectDevice(0);
		}

		void OnSave(wxCommandEvent& event) override
		{
			TransferDataFromWindow();
			frame_.SaveSettings(settings_);
			Close(true);
		}

		void OnSelectDevice(wxCommandEvent& event) override
		{
			TransferDataFromWindow();
			SelectDevice(event.GetSelection());
		}

		void OnEnable(wxCommandEvent& event) override
		{
			if(deviceSettings_)
			{
				deviceSettings_->enabled = static_cast<bool>(event.GetInt());
				EnableControls(deviceSettings_->enabled);
			}
		}

		void OnVerticalZone(TouchpadEvent& event)
		{
			if(deviceSettings_)
			{
				deviceSettings_->vScrollZone = event.GetValue();
			}
		}

		void OnHorizontalZone(TouchpadEvent& event)
		{
			if(deviceSettings_)
			{
				deviceSettings_->hScrollZone = event.GetValue();
			}
		}

	private:
		void SelectDevice(int selection)
		{
			if(selection >= 0 && static_cast<unsigned int>(selection) < deviceSelector_->GetCount())
			{
				deviceSettings_ = &settings_.GetDeviceSettings(
					std::string(deviceSelector_->GetStringSelection()));

				ShowDeviceSettings();

				enableDevice_->Enable();
				EnableControls(deviceSettings_->enabled);

				keyboardLockoutMs_->SetValidator(wxIntegerValidator<int>(&deviceSettings_->typingLockoutMs));
				verticalSens_->SetValidator(wxFloatingPointValidator<float>(2, &deviceSettings_->vSens));
				horizontalSens_->SetValidator(wxFloatingPointValidator<float>(2, &deviceSettings_->hSens));
			}
			else
			{
				// Should only occur if there are no touch devices.
				enableDevice_->SetValue(false);
				verticalSens_->SetValue("");
				horizontalSens_->SetValue("");
				touchpadCtrl_->SetValue(0.5f, 0.5f);

				enableDevice_->Enable(false);
				EnableControls(false);
			}
		}

		void ShowDeviceSettings()
		{
			enableDevice_->SetValue(deviceSettings_->enabled);
			verticalSens_->SetValue(absl::StrFormat("%.2f", deviceSettings_->vSens));
			horizontalSens_->SetValue(absl::StrFormat("%.2f", deviceSettings_->hSens));
			touchpadCtrl_->SetValue(deviceSettings_->vScrollZone, deviceSettings_->hScrollZone);
		}

		void EnableControls(bool enable)
		{
			keyboardLockoutMs_->Enable(enable);
			verticalSens_->Enable(enable);
			horizontalSens_->Enable(enable);
			touchpadCtrl_->Enable(enable);
		}

		Settings settings_;
		Settings::DeviceSettings* deviceSettings_;
		ChiralScrollFrame& frame_;
	};

public:
	ChiralScrollFrame(
		const std::string& title,
		Settings& settings,
		absl::flat_hash_map<HANDLE, TouchDevice> touchDevices,
		ChiralScroll chiralScroll)
		: wxFrame(nullptr, wxID_ANY, title),
		  hWnd_(static_cast<HWND>(GetHWND())),
		  icon_(new NotificationIcon(*this)),  // wx takes ownership
		  settings_(settings),
		  touchDevices_(std::move(touchDevices)),
		  chiralScroll_(std::move(chiralScroll)),
		  stopped_(false)
	{
		RAWINPUTDEVICE rid[]{
			{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_INPUTSINK, hWnd_},
			{HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TOUCH_PAD, RIDEV_INPUTSINK, hWnd_},
		};
		THROW_IF_FALSE(RegisterRawInputDevices(rid, sizeof(rid)/sizeof(RAWINPUTDEVICE), sizeof(RAWINPUTDEVICE)),
			absl::StrCat("RegisterRawInputDevices failed: ", GetErrorMessage(GetLastError())));
	}

	~ChiralScrollFrame()
	{
		icon_->Destroy();
	}

	void ToggleEnabled()
	{
		settings_.GetGlobalSettings().enabled = !settings_.GetGlobalSettings().enabled;
		chiralScroll_.SetSettings(settings_);
	}

	void ShowSettings()
	{
		SettingsDialog* settingsDialog = new SettingsDialogImpl(*this);
		settingsDialog->Show(true);
	}

	void SaveSettings(Settings& settings)
	{
		settings_ = settings;
		chiralScroll_.SetSettings(settings);
	}

	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override
	{
		if(!stopped_ && message == WM_INPUT)
		{
			auto maybeData = HidData::FromRawInput(reinterpret_cast<HRAWINPUT>(lParam));
			if(maybeData)
			{
				HandleRawInput(*maybeData);
			}
			else
			{
				// Input must have been keyboard.
				chiralScroll_.ProcessKeyboard();
			}

			// Indicates that application was in foreground, we must call DefWindowProc
			// to cleanup.
			if(GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT)
			{
				return DefWindowProc(hWnd_, message, wParam, lParam);
			}
			return 0;
		}
		return wxFrame::MSWWindowProc(message, wParam, lParam);
	}

	void Stop()
	{
		stopped_ = true;
	}

private:
	void HandleRawInput(HidData& hidData)
	{
		if(!touchDevices_.contains(hidData.header.hDevice))
		{
			return;
		}

		auto& touchDevice = touchDevices_.at(hidData.header.hDevice);
		const std::optional<std::vector<TouchDevice::Contact>> contacts = touchDevice.GetContacts(hidData);
		if(!contacts)
		{
			return;
		}
		chiralScroll_.ProcessTouch(touchDevice, *contacts);
	}

	const HWND hWnd_;
	NotificationIcon* const icon_;
	Settings& settings_;
	absl::flat_hash_map<HANDLE, TouchDevice> touchDevices_;
	ChiralScroll chiralScroll_;
	bool stopped_;
};

wxBEGIN_EVENT_TABLE(ChiralScrollFrame::NotificationIcon, wxTaskBarIcon)
	EVT_TASKBAR_LEFT_UP(ChiralScrollFrame::NotificationIcon::OnClick)
	EVT_MENU(PU_ENABLE, ChiralScrollFrame::NotificationIcon::OnEnable)
	EVT_MENU(PU_SETTINGS, ChiralScrollFrame::NotificationIcon::OnSettings)
	EVT_MENU(PU_CLOSE, ChiralScrollFrame::NotificationIcon::OnClose)
wxEND_EVENT_TABLE()


class ChiralScrollApp : public wxApp
{
public:
	virtual ~ChiralScrollApp()
	{
		if(logToConsole_)
		{
			FreeConsole();
		}
	}

	void OnInitCmdLine(wxCmdLineParser& parser) override
	{
		wxApp::OnInitCmdLine(parser);

		const wxCmdLineEntryDesc desc[] = {
			{wxCMD_LINE_SWITCH, "", "logToConsole", "Log to console."},
			{wxCMD_LINE_OPTION, "", "logLevel", "Logging level: trace, debug, info, warn, err, critical, or off (default warn).", wxCMD_LINE_VAL_STRING},
			{wxCMD_LINE_SWITCH, "", "panicOnUnexpectedInput", "Panic and crash when unexpected inputs are received."},
			{wxCMD_LINE_NONE},
		};
		parser.SetDesc(desc);
	}

	bool OnCmdLineParsed(wxCmdLineParser& parser) override
	{
		if(!wxApp::OnCmdLineParsed(parser))
		{
			return false;
		}

		if(parser.Found("logToConsole"))
		{
			logToConsole_= true;
		}

		if(parser.Found("panicOnUnexpectedInput"))
		{
			panicOnUnexpectedInput_ = true;
		}

		static const absl::flat_hash_map<wxString, spdlog::level::level_enum> levelMap = {
			{"trace", spdlog::level::trace},
			{"debug", spdlog::level::debug},
			{"info", spdlog::level::info},
			{"warn", spdlog::level::warn},
			{"err", spdlog::level::err},
			{"critical", spdlog::level::critical},
			{"off", spdlog::level::off},
		};
		wxString level = "warn";
		if(parser.Found("logLevel", &level))
		{
			if(!levelMap.contains(level))
			{
				return false;
			}
		}
		spdlog::set_level(levelMap.at(level));

		return true;
	}

	bool OnInit() override
	{
		if(!wxApp::OnInit())
		{
			return false;
		}

		if(logToConsole_)
		{
			AllocConsole();
			spdlog::set_default_logger(spdlog::stderr_color_mt("stderr_logger"));
		}
		else
		{
			spdlog::set_default_logger(
				spdlog::basic_logger_st("basic_logger", (GetCurrentDirectory() / "chiralscroll.log").string(), true));
		}

		absl::flat_hash_map<HANDLE, TouchDevice> devices = chiralscroll::GetTouchDevices(panicOnUnexpectedInput_);
		std::vector<std::string> deviceNames;
		deviceNames.reserve(devices.size());
		for(const auto& pair : devices)
		{
			deviceNames.push_back(std::string(pair.second.name()));
		}

		settingsPath_ = GetCurrentDirectory() / "settings.ini";
		settings_ = Settings::FromFile(settingsPath_, deviceNames);

		// wx takes ownership.
		chiralScrollFrame_ = new ChiralScrollFrame(
			kTitle,
			settings_,
			std::move(devices),
			ChiralScroll(
				settings_,
				std::make_unique<WinScroller>(WinScroller::Direction::kVertical),
				std::make_unique<WinScroller>(WinScroller::Direction::kHorizontal)));
		return true;
	}

	int OnExit() override
	{
		// We must handle exceptions here because this is called from a destructor.
		try
		{
			settings_.ToFile(settingsPath_);
		}
		catch(const std::exception& e)
		{
			OnException(e);
		}
		return wxApp::OnExit();
	}

	bool OnExceptionInMainLoop() override
	{
		// Let OnUnhandledException handle this.
		throw;
	}

	void OnUnhandledException() override
	{
		try
		{
			throw;
		}
		catch(const std::exception& e)
		{
			chiralScrollFrame_->Stop();
			OnException(e);
		}
	}

private:
	void OnException(const std::exception& e)
	{
		std::string message = absl::StrCat("Caught exception: ", e.what());
		SPDLOG_ERROR(message);
		MessageBox(
			nullptr,
			StringToWstring(message).c_str(),
			L"ChiralScroll Error",
			MB_OK | MB_ICONERROR);
	}

	std::filesystem::path settingsPath_;
	Settings settings_;
	ChiralScrollFrame* chiralScrollFrame_;
	bool logToConsole_ = false;
	bool panicOnUnexpectedInput_ = false;
};

wxIMPLEMENT_APP(ChiralScrollApp);

}  // namespace chiralscroll
