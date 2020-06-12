#include <windows.h>
#include <strsafe.h>
#include <powersetting.h>

#include "helper.h"
#include "resource.h"

// Presumed name of the internal display of the laptop.
constexpr TCHAR INTERNAL_DISPLAY_NAME[] = TEXT("\\\\.\\DISPLAY1");

// Window class for SwitchOnDock windows.
constexpr TCHAR WINDOW_CLASS[] = TEXT("SwitchOnDock");

// Application name.
constexpr TCHAR APPLICATION_NAME[] = TEXT("SwitchOnDock");

// Warning for any generic unexpected error.
constexpr TCHAR UNEXPECTED_ERROR_WARNING[] = TEXT("Unexpected error.");

// Warning for when an unexpected error is encountered detecting the lid close event.
constexpr TCHAR LID_CLOSE_DETECTION_FAILED_WARNING[] = TEXT(
	"Unexpected error checking the lid close event. This application may not work correctly on your operating system.");

// Warning for when the lid close event is disabled in the UI.
constexpr TCHAR LID_CLOSE_DISABLED_WARNING[] = TEXT(
	"The power plan option for lid close events is currently hidden. This will cause the UI to behave counter-intuitively and display incorrect information. Would you like to change this?");

// Registry key for the lid close action.
constexpr TCHAR LID_CLOSE_REG_KEY[] = TEXT(
	"SYSTEM\\CurrentControlSet\\Control\\Power\\PowerSettings\\4f971e89-eebd-4455-a8de-9e59040e7347\\5ca83367-6e45-459f-a27b-476b1d01c936");

// Registry value name for a power setting's attributes value.
constexpr TCHAR POWER_SETTING_ATTRIBUTE_VALUE_NAME[] = TEXT("Attributes");

// Registry file for enabling the power lid close action in the UI.
constexpr BYTE ENABLE_POWER_LID_REG[] =
	"Windows Registry Editor Version 5.00\r\n\r\n[HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Power\\PowerSettings\\4f971e89-eebd-4455-a8de-9e59040e7347\\5ca83367-6e45-459f-a27b-476b1d01c936]\r\n\"Attributes\"=dword:00000000\r\n\r\n";

class App {
public:
	App(HINSTANCE hInstance, HICON hIcon) : WindowClass{WINDOW_CLASS, hInstance, hIcon, StaticWndProc},
	                                        MessageWindow{
		                                        WindowClass, TEXT("SwitchOnDock Message Window"), 0, HWND_DESKTOP
	                                        },
	                                        TrayIcon{MessageWindow, hIcon, APPLICATION_NAME} {
		SetWindowLongPtr(MessageWindow.Handle(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		HandleDisplaysChanged();
	}

	void CheckLidCloseEnabledInUI() {
		UNREFERENCED_PARAMETER(this);

		try {
			RegKey lidClose{HKEY_LOCAL_MACHINE, LID_CLOSE_REG_KEY};
			DWORD dwAttributes = lidClose.QueryDword(POWER_SETTING_ATTRIBUTE_VALUE_NAME);
			if ((dwAttributes & 1) == 0) {
				return;
			}
		} catch (std::exception&) {
			MessageBox(
				HWND_DESKTOP,
				LID_CLOSE_DETECTION_FAILED_WARNING,
				APPLICATION_NAME, MB_ICONWARNING);
			return;
		}

		int cmd = MessageBox(HWND_DESKTOP, LID_CLOSE_DISABLED_WARNING, APPLICATION_NAME, MB_YESNO);
		if (cmd == IDYES) {
			try {
				auto tmpFileName = WriteTempFile(TEXT(".reg"), ENABLE_POWER_LID_REG, sizeof(ENABLE_POWER_LID_REG));
				ShellExecute(nullptr, TEXT("open"), tmpFileName.get(), nullptr, nullptr, SW_SHOWDEFAULT);
			} catch (std::exception&) {
				MessageBox(HWND_DESKTOP, UNEXPECTED_ERROR_WARNING, APPLICATION_NAME, MB_ICONWARNING);
				return;
			}
		}
	}

	INT Run() {
		CheckLidCloseEnabledInUI();

		MSG msg = {};
		while (GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return INT(msg.wParam);
	}

private:
	static BOOL CALLBACK StaticCheckMonitorExternal(HMONITOR hMonitor, HDC hDc, LPRECT lpRect, LPARAM lParam) {
		return reinterpret_cast<App*>(lParam)->CheckMonitorExternal(hMonitor, hDc, lpRect);
	}

	// Checks to see if this monitor is external. If it is, the global 'docked' flag is set.
	BOOL CheckMonitorExternal(HMONITOR hMonitor, HDC hDc, LPRECT lpRect) {
		UNREFERENCED_PARAMETER(hDc);
		UNREFERENCED_PARAMETER(lpRect);

		MONITORINFOEX monitorInfo = {};
		monitorInfo.cbSize = sizeof(monitorInfo);
		GetMonitorInfo(hMonitor, &monitorInfo);

		if (lstrcmpi(monitorInfo.szDevice, INTERNAL_DISPLAY_NAME) != 0) {
			IsDocked = true;
		}

		return TRUE;
	}

	void DetectDockingStatus() {
		IsDocked = false;
		EnumDisplayMonitors(nullptr, nullptr, StaticCheckMonitorExternal, reinterpret_cast<LPARAM>(this));
	}

	void UpdateTrayTip() {
		TrayIcon.SetTip(IsDocked ? TEXT("SwitchOnDock [Docked]") : TEXT("SwitchOnDock [Undocked]"));
	}

	void UpdatePowerPreferences() {
		GUID* schemeGuid;

		DWORD result = PowerGetActiveScheme(nullptr, &schemeGuid);
		if (result != ERROR_SUCCESS) {
			MessageBox(HWND_DESKTOP, TEXT("Failed to get active power scheme."), APPLICATION_NAME,
			           MB_OK | MB_ICONERROR);
			return;
		}

		result = PowerWriteDCValueIndex(nullptr, schemeGuid, &GUID_SYSTEM_BUTTON_SUBGROUP,
		                                &GUID_LIDCLOSE_ACTION, IsDocked ? 0 : 1);
		if (result != ERROR_SUCCESS) {
			MessageBox(HWND_DESKTOP, TEXT("Failed to write lid close action for battery mode."), APPLICATION_NAME,
			           MB_OK | MB_ICONERROR);
			return;
		}

		result = PowerWriteACValueIndex(nullptr, schemeGuid, &GUID_SYSTEM_BUTTON_SUBGROUP,
		                                &GUID_LIDCLOSE_ACTION, IsDocked ? 0 : 1);
		if (result != ERROR_SUCCESS) {
			MessageBox(HWND_DESKTOP, TEXT("Failed to write lid close action for AC mode."), APPLICATION_NAME,
			           MB_OK | MB_ICONERROR);
			return;
		}

		result = PowerSetActiveScheme(nullptr, schemeGuid);
		if (result != ERROR_SUCCESS) {
			MessageBox(HWND_DESKTOP, TEXT("Failed to update power scheme."), APPLICATION_NAME,
			           MB_OK | MB_ICONERROR);
			return;
		}

		LocalFree(schemeGuid);
	}

	void HandleDisplaysChanged() {
		DetectDockingStatus();
		UpdateTrayTip();
		UpdatePowerPreferences();
	}

	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		App* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		if (app) {
			return app->WndProc(hWnd, msg, wParam, lParam);
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		switch (msg) {
			case WM_DISPLAYCHANGE:
				HandleDisplaysChanged();
				break;
			case WM_USER:
				switch (LOWORD(lParam)) {
					case WM_CONTEXTMENU:
						if (MessageBox(HWND_DESKTOP, TEXT("Would you like to exit SwitchOnDock?"), APPLICATION_NAME,
						               MB_YESNO) == IDYES) {
							PostQuitMessage(0);
						}
				}
				break;
			default:
				break;
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	WndClass WindowClass;
	Wnd MessageWindow;
	TrayIcon TrayIcon;

	// Whether or not the laptop is believed to be docked.
	bool IsDocked = false;
};

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR lpCmdLine, INT nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	auto hTrayIcon = HICON(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR));
	App app{hInstance, hTrayIcon};
	return app.Run();
}
