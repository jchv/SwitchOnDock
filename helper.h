#pragma once
#include <exception>
#include <memory>
#include <windows.h>

class WndClass {
public:
	WndClass(LPCTSTR lpClassName, HINSTANCE hInstance, HICON hIcon, WNDPROC lpfnWndProc) {
		WNDCLASS wndClass = {};
		wndClass.lpszClassName = lpClassName;
		wndClass.hInstance = hInstance;
		wndClass.hIcon = hIcon;
		wndClass.lpfnWndProc = lpfnWndProc;
		ClassAtom = RegisterClass(&wndClass);
		ClassInstance = hInstance;
	}

	WndClass(const WndClass& other) = delete;

	WndClass(WndClass&& other) noexcept : ClassAtom(other.ClassAtom), ClassInstance(other.ClassInstance) {}

	~WndClass() {
		UnregisterClass(reinterpret_cast<LPCTSTR>(ClassAtom), ClassInstance);
	}

	WndClass& operator=(const WndClass& other) = delete;

	WndClass& operator=(WndClass&& other) noexcept {
		ClassAtom = other.ClassAtom;
		ClassInstance = other.ClassInstance;
		return *this;
	}

	ATOM Atom() const {
		return ClassAtom;
	};

	HINSTANCE Instance() const {
		return ClassInstance;
	}

private:
	ATOM ClassAtom;
	HINSTANCE ClassInstance;
};

class Wnd {
public:
	Wnd(const WndClass& wndClass, LPCTSTR lpWindowName, DWORD dwStyle, HWND hWndParent) : Window(CreateWindowEx(
		0, reinterpret_cast<LPCWSTR>(wndClass.Atom()), lpWindowName, dwStyle, 0, 0, 0, 0, hWndParent,
		nullptr, wndClass.Instance(), nullptr)) {}

	Wnd(const Wnd& other) = delete;

	Wnd(Wnd&& other) noexcept : Window(other.Window) {}

	~Wnd() {
		DestroyWindow(Window);
	}

	Wnd& operator=(const Wnd& other) = delete;

	Wnd& operator=(Wnd&& other) noexcept {
		Window = other.Window;
		return *this;
	}

	HWND Handle() const {
		return Window;
	}

private:
	HWND Window;
};

class TrayIcon {
public:
	TrayIcon(const Wnd& window, HICON hIcon, LPCTSTR szTip) : WindowHandle(window.Handle()) {
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(nid);
		nid.hWnd = WindowHandle;
		nid.hIcon = hIcon;
		nid.uCallbackMessage = WM_USER;
		StringCchCopy(nid.szTip, 128, szTip);
		nid.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE;
		nid.uVersion = NOTIFYICON_VERSION_4;
		Shell_NotifyIcon(NIM_ADD, &nid);
		Shell_NotifyIcon(NIM_SETVERSION, &nid);
	}

	TrayIcon(const TrayIcon& other) = delete;

	TrayIcon(TrayIcon&& other) = delete;

	~TrayIcon() {
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(nid);
		nid.hWnd = WindowHandle;
		Shell_NotifyIcon(NIM_DELETE, &nid);
	}

	TrayIcon& operator=(const TrayIcon& other) = delete;

	TrayIcon& operator=(TrayIcon&& other) = delete;

	void SetTip(LPCTSTR szTip) {
		NOTIFYICONDATA nid = {};
		nid.cbSize = sizeof(nid);
		nid.hWnd = WindowHandle;
		nid.uFlags = NIF_TIP | NIF_SHOWTIP;
		StringCchCopy(nid.szTip, 128, szTip);
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}

private:
	HWND WindowHandle;
};

class RegKey {
public:
	RegKey(HKEY hKey, LPCTSTR lpSubKey) : Handle(nullptr) {
		LSTATUS lStatus = RegOpenKey(hKey, lpSubKey, &Handle);
		if (lStatus == ERROR_FILE_NOT_FOUND) {
			throw std::exception("registry key is not found");
		}
		if (lStatus != ERROR_SUCCESS) {
			throw std::exception("error opening registry key");
		}
	}

	RegKey(const RegKey& other) = delete;

	RegKey(RegKey&& other) = delete;

	~RegKey() {
		RegCloseKey(Handle);
	}

	RegKey& operator=(const RegKey& other) = delete;

	RegKey& operator=(RegKey&& other) = delete;

	DWORD QueryDword(LPCTSTR lpValueName) const {
		DWORD result, type = -1, size = sizeof(result);
		LSTATUS lStatus = RegQueryValueEx(Handle, lpValueName, nullptr, &type, nullptr, nullptr);
		if (lStatus == ERROR_FILE_NOT_FOUND) {
			throw std::exception("registry value is not found");
		}
		if (lStatus != ERROR_SUCCESS) {
			throw std::exception("error querying registry value type");
		}
		if (type != REG_DWORD) {
			throw std::exception("registry key is the wrong type");
		}
		lStatus = RegQueryValueEx(Handle, lpValueName, nullptr, nullptr, LPBYTE(&result), &size);
		if (lStatus != ERROR_SUCCESS) {
			throw std::exception("error querying registry value data");
		}
		return result;
	}
	
private:
	HKEY Handle;
};

static std::unique_ptr<TCHAR> WriteTempFile(LPCTSTR lpSuffix, LPCBYTE data, UINT size) {
	TCHAR tempPath[MAX_PATH + 1];
	GetTempPath(MAX_PATH + 1, tempPath);

	std::unique_ptr<TCHAR> tmpFileName{ new TCHAR[MAX_PATH + 1] };
	GetTempFileName(tempPath, TEXT("SD"), 0, tmpFileName.get());

	StringCchCat(tmpFileName.get(), MAX_PATH + 1, lpSuffix);

	HANDLE hFile = CreateFile(tmpFileName.get(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == nullptr) {
		throw std::exception("error creating temp file");
	}

	DWORD bytesWritten;
	WriteFile(hFile, data, size, &bytesWritten, nullptr);
	if (bytesWritten != size) {
		throw std::exception("short write");
	}

	CloseHandle(hFile);

	return tmpFileName;
}