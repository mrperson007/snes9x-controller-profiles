/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// Controller profiles, controller selection and auto-configure: the Windows side.
// See InputProfiles.h. The decisions (what to bind, how files look, which controller
// a key refers to) are made in InputProfilesCore.cpp, which has unit tests.

#include "../port.h"
#include "../snes9x.h"
#include "../display.h"
#include "wsnes9x.h"
#include "wlanguage.h"
#include "InputProfiles.h"

#include <algorithm>
#include <string>
#include <vector>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

namespace ip = inprof;
typedef std::basic_string<TCHAR> tstring;

TCHAR JoypadDeviceKey[NUM_PROFILE_PADS][JOYPAD_DEVICE_KEY_LEN];

extern SJoyState JoystickF[16];            // wsnes9x.cpp: the Input dialog's own copy of the controller state
extern struct SJoypad DefaultJoypad[16];   // wsnes9x.cpp: the factory keyboard layout
void S9xDetectJoypads();                   // win32.cpp

static const TCHAR* const kProfileExt = TEXT(".s9xpad");

/*---------------------------------------------------------------------------*/
// text helpers: Snes9x keeps TCHAR strings, the core and the profile files use UTF-8

static std::string T2U(const TCHAR* s)
{
#ifdef UNICODE
	int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
	if (n <= 1) return std::string();
	std::string r((size_t)n - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, s, -1, &r[0], n, NULL, NULL);
	return r;
#else
	return std::string(s);
#endif
}

static tstring U2T(const std::string& s)
{
#ifdef UNICODE
	if (s.empty()) return tstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	if (n <= 1) return tstring();
	tstring r((size_t)n - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], n);
	return r;
#else
	return tstring(s.begin(), s.end());
#endif
}

static tstring Format(const TCHAR* fmt, ...)
{
	TCHAR buf[512];
	va_list ap;
	va_start(ap, fmt);
	_vsntprintf(buf, COUNT(buf) - 1, fmt, ap);
	va_end(ap);
	buf[COUNT(buf) - 1] = TEXT('\0');
	return buf;
}

/*---------------------------------------------------------------------------*/
// game controllers

// "VID_045E&PID_028E" style registry key names carry the real vendor / product ids.
static bool ParseHexAfter(const tstring& upper, const TCHAR* tag, unsigned& out)
{
	size_t p = upper.find(tag);
	if (p == tstring::npos) return false;
	p += _tcslen(tag);

	unsigned v = 0;
	int digits = 0;
	while (p < upper.size() && digits < 4)
	{
		TCHAR c = upper[p];
		unsigned d;
		if (c >= TEXT('0') && c <= TEXT('9'))      d = c - TEXT('0');
		else if (c >= TEXT('A') && c <= TEXT('F')) d = c - TEXT('A') + 10;
		else break;
		v = v * 16 + d;
		digits++;
		p++;
	}
	if (digits != 4) return false;
	out = v;
	return true;
}

// The friendly name Windows shows in "Set up USB game controllers" lives in the registry,
// keyed by the joystick's registry key name. The name in JOYCAPS is often a generic
// "Microsoft PC-joystick driver".
static tstring ReadOemName(const TCHAR* regKeyName)
{
	if (!regKeyName || !*regKeyName) return tstring();

	tstring path = TEXT("System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\Joystick\\OEM\\");
	path += regKeyName;

	const HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
	for (int r = 0; r < 2; r++)
	{
		HKEY hk = NULL;
		if (RegOpenKeyEx(roots[r], path.c_str(), 0, KEY_READ, &hk) != ERROR_SUCCESS)
			continue;

		TCHAR buf[256];
		DWORD type = 0, size = (DWORD)(sizeof(buf) - sizeof(TCHAR));   // leave room for a terminator
		LONG rc = RegQueryValueEx(hk, TEXT("OEMName"), NULL, &type, (LPBYTE)buf, &size);
		RegCloseKey(hk);

		if (rc == ERROR_SUCCESS && type == REG_SZ)
		{
			buf[size / sizeof(TCHAR)] = TEXT('\0');
			if (buf[0]) return buf;
		}
	}
	return tstring();
}

std::vector<ip::DeviceInfo> InputProfiles_EnumerateDevices()
{
	std::vector<ip::DeviceInfo> list;

	for (int i = 0; i < ip::MAX_JOYSTICKS; i++)
	{
		JOYCAPS caps;
		if (joyGetDevCaps(JOYSTICKID1 + i, &caps, sizeof(caps)) != JOYERR_NOERROR)
			continue;

		// joyGetDevCaps also succeeds for controllers that were once installed but are
		// unplugged; only list the ones that answer.
		JOYINFOEX state;
		state.dwSize = sizeof(state);
		state.dwFlags = JOY_RETURNALL;
		if (joyGetPosEx(JOYSTICKID1 + i, &state) != JOYERR_NOERROR)
			continue;

		ip::DeviceInfo d;
		d.index = i;

		tstring name = ReadOemName(caps.szRegKey);
		if (name.empty()) name = caps.szPname;
		d.name = T2U(name.c_str());

		unsigned vid = caps.wMid, pid = caps.wPid;
		tstring regKey = caps.szRegKey;
		for (size_t k = 0; k < regKey.size(); k++)
			if (regKey[k] >= TEXT('a') && regKey[k] <= TEXT('z')) regKey[k] = (TCHAR)(regKey[k] - TEXT('a') + TEXT('A'));
		unsigned v2, p2;
		if (ParseHexAfter(regKey, TEXT("VID_"), v2) && ParseHexAfter(regKey, TEXT("PID_"), p2))
		{
			vid = v2;
			pid = p2;
		}
		d.vendorId = (uint16_t)vid;
		d.productId = (uint16_t)pid;

		d.numButtons = (int)caps.wNumButtons;
		d.numAxes = (int)caps.wNumAxes;
		d.hasPov = (caps.wCaps & JOYCAPS_HASPOV) != 0;
		list.push_back(d);
	}

	ip::AssignDeviceKeys(list);
	return list;
}

/*---------------------------------------------------------------------------*/
// Joypad[] <-> Bindings

static void ReadPad(int pad, ip::Bindings& b)        { ip::ReadBindings(Joypad[pad], Joypad[pad + 8], b); }
static void WritePad(int pad, const ip::Bindings& b) { ip::WriteBindings(b, Joypad[pad], Joypad[pad + 8]); }

/*---------------------------------------------------------------------------*/
// remembering which controller a joypad belongs to

void InputProfiles_RememberDevices()
{
	std::vector<ip::DeviceInfo> devices = InputProfiles_EnumerateDevices();

	for (int pad = 0; pad < NUM_PROFILE_PADS; pad++)
	{
		ip::Bindings b;
		ReadPad(pad, b);
		int joy = ip::PrimaryJoyIndex(b);

		// keyboard-only joypads forget their controller. A controller that is not plugged
		// in right now keeps whatever was remembered before.
		if (joy < 0)
		{
			JoypadDeviceKey[pad][0] = TEXT('\0');
			continue;
		}
		for (size_t i = 0; i < devices.size(); i++)
		{
			if (devices[i].index == joy)
			{
				tstring key = U2T(devices[i].key);
				lstrcpyn(JoypadDeviceKey[pad], key.c_str(), JOYPAD_DEVICE_KEY_LEN);
				break;
			}
		}
	}
}

void InputProfiles_ResolveDevices()
{
	std::vector<ip::DeviceInfo> devices = InputProfiles_EnumerateDevices();
	if (devices.empty()) return;

	for (int pad = 0; pad < NUM_PROFILE_PADS; pad++)
	{
		std::string key = T2U(JoypadDeviceKey[pad]);
		if (key.empty()) continue;

		// exact match only: a missing second controller must not send both players to the first one
		int pos = ip::FindDeviceByKey(devices, key, false);
		if (pos < 0) continue;

		ip::Bindings b;
		ReadPad(pad, b);
		int from = ip::PrimaryJoyIndex(b);
		if (from < 0) continue;

		if (ip::RetargetJoystick(b, from, devices[pos].index) > 0)
			WritePad(pad, b);
	}
}

/*---------------------------------------------------------------------------*/
// profile files: <folder of snes9x.exe>\InputProfiles\<name>.s9xpad

static tstring ProfileDir()
{
	tstring dir = S9xGetDirectoryT(DEFAULT_DIR);
	if (!dir.empty() && dir[dir.size() - 1] != TEXT('\\'))
		dir += TEXT('\\');
	dir += TEXT("InputProfiles");
	return dir;
}

static tstring ProfilePath(const std::string& name)
{
	return ProfileDir() + TEXT("\\") + U2T(name) + kProfileExt;
}

static bool ProfileExists(const std::string& name)
{
	DWORD attr = GetFileAttributes(ProfilePath(name).c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool ReadWholeFile(const tstring& path, std::string& out)
{
	HANDLE h = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;

	bool ok = false;
	DWORD size = GetFileSize(h, NULL);
	if (size != INVALID_FILE_SIZE && size <= 65536)         // a profile is about 1 KB
	{
		out.assign(size, '\0');
		DWORD got = 0;
		ok = (size == 0) || (ReadFile(h, &out[0], size, &got, NULL) && got == size);
	}
	CloseHandle(h);
	return ok;
}

// written next to the target first, so a crash cannot leave half a profile behind
static bool WriteWholeFile(const tstring& path, const std::string& data)
{
	tstring tmp = path + TEXT(".tmp");
	HANDLE h = CreateFile(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;

	DWORD put = 0;
	bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &put, NULL) && put == (DWORD)data.size();
	CloseHandle(h);

	if (ok) ok = MoveFileEx(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
	if (!ok) DeleteFile(tmp.c_str());
	return ok;
}

enum ProfileLoadResult { PL_OK, PL_MISSING, PL_BAD };

static ProfileLoadResult LoadProfileFile(const std::string& name, ip::Profile& out)
{
	std::string text;
	if (!ReadWholeFile(ProfilePath(name), text)) return PL_MISSING;
	return ip::ParseProfile(text, out) ? PL_OK : PL_BAD;
}

static bool SaveProfileFile(const ip::Profile& p)
{
	tstring dir = ProfileDir();
	if (!CreateDirectory(dir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		return false;
	return WriteWholeFile(ProfilePath(p.name), ip::SerializeProfile(p));
}

static bool DeleteProfileFile(const std::string& name)
{
	return DeleteFile(ProfilePath(name).c_str()) != 0;
}

static bool NameLess(const tstring& a, const tstring& b)
{
	return lstrcmpi(a.c_str(), b.c_str()) < 0;
}

static std::vector<tstring> ListProfiles()
{
	std::vector<tstring> names;

	WIN32_FIND_DATA fd;
	HANDLE h = FindFirstFile((ProfileDir() + TEXT("\\*") + kProfileExt).c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return names;

	const tstring ext = kProfileExt;
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		tstring n = fd.cFileName;
		if (n.size() > ext.size() && lstrcmpi(n.c_str() + n.size() - ext.size(), ext.c_str()) == 0)
			names.push_back(n.substr(0, n.size() - ext.size()));
	} while (FindNextFile(h, &fd));
	FindClose(h);

	std::sort(names.begin(), names.end(), NameLess);
	return names;
}

/*---------------------------------------------------------------------------*/
// Input Configuration dialog

static std::vector<ip::DeviceInfo> g_devices;   // what the Device list shows (list item n+1 = g_devices[n])

static void SetStatus(HWND hDlg, const tstring& text)
{
	SetDlgItemText(hDlg, IDC_PROFILE_STATUS, text.c_str());
}

static void FillDeviceCombo(HWND hDlg)
{
	SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_ADDSTRING, 0, (LPARAM)INPUTCONFIG_DEVICE_KEYBOARD);
	for (size_t i = 0; i < g_devices.size(); i++)
	{
		tstring item = Format(INPUTCONFIG_DEVICE_ITEM, g_devices[i].index + 1, U2T(g_devices[i].name).c_str());
		SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_ADDSTRING, 0, (LPARAM)item.c_str());
	}
	SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_SETCURSEL, 0, 0);
}

// Position in g_devices of the chosen controller, or -1 for "Keyboard / as saved".
static int SelectedDevicePos(HWND hDlg)
{
	int sel = (int)SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_GETCURSEL, 0, 0);
	if (sel <= 0 || sel > (int)g_devices.size()) return -1;
	return sel - 1;
}

static void SelectDevicePos(HWND hDlg, int pos)
{
	SendDlgItemMessage(hDlg, IDC_DEVCOMBO, CB_SETCURSEL, (WPARAM)(pos + 1), 0);
}

// Show the controller this joypad's bindings currently point at.
static void PreselectDeviceForPad(HWND hDlg, int pad)
{
	ip::Bindings b;
	ReadPad(pad, b);
	int joy = ip::PrimaryJoyIndex(b);

	int pos = -1;
	for (size_t i = 0; i < g_devices.size(); i++)
		if (g_devices[i].index == joy) pos = (int)i;
	SelectDevicePos(hDlg, pos);
}

static std::string TypedProfileName(HWND hDlg)
{
	TCHAR buf[256] = { 0 };
	GetDlgItemText(hDlg, IDC_PROFILECOMBO, buf, COUNT(buf));
	return ip::SanitizeProfileName(T2U(buf));
}

// Reloads the list of saved profiles. The text box keeps what was typed unless a name is given.
static void RefillProfiles(HWND hDlg, const std::string* showName)
{
	TCHAR typed[256] = { 0 };
	GetDlgItemText(hDlg, IDC_PROFILECOMBO, typed, COUNT(typed));

	SendDlgItemMessage(hDlg, IDC_PROFILECOMBO, CB_RESETCONTENT, 0, 0);
	std::vector<tstring> names = ListProfiles();
	for (size_t i = 0; i < names.size(); i++)
		SendDlgItemMessage(hDlg, IDC_PROFILECOMBO, CB_ADDSTRING, 0, (LPARAM)names[i].c_str());

	if (showName) SetDlgItemText(hDlg, IDC_PROFILECOMBO, U2T(*showName).c_str());
	else          SetDlgItemText(hDlg, IDC_PROFILECOMBO, typed);
}

// Turns a joypad on when it has just been given buttons, and refreshes the "Enabled" box.
static void EnablePadIfNeeded(HWND hDlg, int pad, const ip::Bindings& b)
{
	bool any = false;
	for (int i = 0; i < ip::NUM_PAD_SLOTS; i++)
		if (b.v[i] != 0) any = true;
	if (any && !Joypad[pad].Enabled)
		Joypad[pad].Enabled = TRUE;

	CheckDlgButton(hDlg, IDC_JPTOGGLE, Joypad[pad].Enabled ? BST_CHECKED : BST_UNCHECKED);
}

static void RefreshDevices(HWND hDlg, int pad)
{
	std::string keep;
	int pos = SelectedDevicePos(hDlg);
	if (pos >= 0) keep = g_devices[pos].key;

	// let the emulator itself notice new / removed controllers, and give the dialog's
	// button / axis capture the ranges of newly plugged in ones
	S9xDetectJoypads();
	for (short C = 0; C != 16; C++)
		JoystickF[C].Attached = joyGetDevCaps(JOYSTICKID1 + C, &JoystickF[C].Caps, sizeof(JOYCAPS)) == JOYERR_NOERROR;

	g_devices = InputProfiles_EnumerateDevices();
	FillDeviceCombo(hDlg);

	int again = keep.empty() ? -1 : ip::FindDeviceByKey(g_devices, keep, false);
	if (again >= 0) SelectDevicePos(hDlg, again);
	else            PreselectDeviceForPad(hDlg, pad);

	SetStatus(hDlg, Format(INPUTCONFIG_MSG_DEVICES_FOUND, (int)g_devices.size()));
}

static bool DoAutoConfigure(HWND hDlg, int pad)
{
	ip::Bindings b;
	ReadPad(pad, b);

	int pos = SelectedDevicePos(hDlg);
	if (pos < 0)
	{
		// "Keyboard": put back the factory keys. Only joypads 1 and 2 have any.
		ip::Bindings defaults;
		ip::ReadBindings(DefaultJoypad[pad], DefaultJoypad[pad + 8], defaults);
		bool any = false;
		for (int i = 0; i < ip::NUM_PAD_SLOTS; i++)
			if (defaults.v[i] != 0) any = true;
		if (!any)
		{
			SetStatus(hDlg, Format(INPUTCONFIG_MSG_NO_KEYBOARD_DEFAULT, pad + 1));
			return false;
		}
		for (int i = 0; i < ip::NUM_PAD_SLOTS; i++)      // the Turbo panel keeps its keys
			b.v[i] = defaults.v[i];
		WritePad(pad, b);
		EnablePadIfNeeded(hDlg, pad, b);
		SetStatus(hDlg, Format(INPUTCONFIG_MSG_KEYBOARD_RESET, pad + 1));
		return true;
	}

	const ip::DeviceInfo& dev = g_devices[pos];
	tstring devName = U2T(dev.name);
	if (!ip::AutoConfigure(dev, b))
	{
		SetStatus(hDlg, Format(INPUTCONFIG_MSG_AUTOCONFIG_UNUSABLE, devName.c_str()));
		return false;
	}
	WritePad(pad, b);
	EnablePadIfNeeded(hDlg, pad, b);
	SetStatus(hDlg, Format(INPUTCONFIG_MSG_AUTOCONFIG_DONE, pad + 1, devName.c_str(),
	                       U2T(ip::LayoutName(ip::DetectLayout(dev))).c_str()));
	return true;
}

static bool DoLoadProfile(HWND hDlg, int pad)
{
	std::string name = TypedProfileName(hDlg);
	if (name.empty())
	{
		SetStatus(hDlg, INPUTCONFIG_MSG_NEED_NAME);
		return false;
	}

	ip::Profile prof;
	switch (LoadProfileFile(name, prof))
	{
		case PL_MISSING: SetStatus(hDlg, Format(INPUTCONFIG_MSG_NO_SUCH_PROFILE, U2T(name).c_str())); return false;
		case PL_BAD:     SetStatus(hDlg, Format(INPUTCONFIG_MSG_BAD_PROFILE, U2T(name).c_str())); return false;
		default: break;
	}

	ip::Bindings b = prof.bindings;
	const int from = ip::PrimaryJoyIndex(b);

	// Which controller gets the profile's buttons: the one chosen in the Device list. If that
	// says "Keyboard / as saved", use the controller the profile was made for when it is plugged
	// in, and otherwise keep the joystick numbers stored in the profile.
	int pos = SelectedDevicePos(hDlg);
	bool controllerMissing = false;
	if (pos < 0 && from >= 0 && !prof.deviceKey.empty())
	{
		pos = ip::FindDeviceByKey(g_devices, prof.deviceKey, true);
		if (pos >= 0) SelectDevicePos(hDlg, pos);
		else          controllerMissing = true;
	}
	if (pos >= 0 && from >= 0)
		ip::RetargetJoystick(b, from, g_devices[pos].index);

	WritePad(pad, b);
	EnablePadIfNeeded(hDlg, pad, b);

	if (controllerMissing)
		SetStatus(hDlg, Format(INPUTCONFIG_MSG_PROFILE_LOADED_NO_DEVICE, U2T(name).c_str(), pad + 1,
		                       U2T(prof.deviceName.empty() ? prof.deviceKey : prof.deviceName).c_str()));
	else
		SetStatus(hDlg, Format(INPUTCONFIG_MSG_PROFILE_LOADED, U2T(name).c_str(), pad + 1));
	return true;
}

static void DoSaveProfile(HWND hDlg, int pad)
{
	std::string name = TypedProfileName(hDlg);
	if (name.empty())
	{
		SetStatus(hDlg, INPUTCONFIG_MSG_NEED_NAME);
		return;
	}

	if (ProfileExists(name))
	{
		tstring ask = Format(INPUTCONFIG_MSG_CONFIRM_OVERWRITE, U2T(name).c_str());
		if (MessageBox(hDlg, ask.c_str(), INPUTCONFIG_TITLE, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
			return;
	}

	ip::Profile prof;
	prof.name = name;
	ReadPad(pad, prof.bindings);

	// note which controller the joystick bindings are for (none for a keyboard-only joypad)
	int joy = ip::PrimaryJoyIndex(prof.bindings);
	for (size_t i = 0; joy >= 0 && i < g_devices.size(); i++)
	{
		if (g_devices[i].index == joy)
		{
			prof.deviceKey = g_devices[i].key;
			prof.deviceName = g_devices[i].name;
		}
	}

	if (!SaveProfileFile(prof))
	{
		tstring msg = Format(INPUTCONFIG_MSG_SAVE_FAILED, U2T(name).c_str());
		MessageBox(hDlg, msg.c_str(), INPUTCONFIG_TITLE, MB_OK | MB_ICONERROR);
		return;
	}

	RefillProfiles(hDlg, &name);
	SetStatus(hDlg, Format(INPUTCONFIG_MSG_PROFILE_SAVED, U2T(name).c_str()));
}

static void DoDeleteProfile(HWND hDlg)
{
	std::string name = TypedProfileName(hDlg);
	if (name.empty())
	{
		SetStatus(hDlg, INPUTCONFIG_MSG_NEED_NAME);
		return;
	}
	if (!ProfileExists(name))
	{
		SetStatus(hDlg, Format(INPUTCONFIG_MSG_NO_SUCH_PROFILE, U2T(name).c_str()));
		return;
	}

	tstring ask = Format(INPUTCONFIG_MSG_CONFIRM_DELETE, U2T(name).c_str());
	if (MessageBox(hDlg, ask.c_str(), INPUTCONFIG_TITLE, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;

	if (!DeleteProfileFile(name))
	{
		tstring msg = Format(INPUTCONFIG_MSG_DELETE_FAILED, U2T(name).c_str());
		MessageBox(hDlg, msg.c_str(), INPUTCONFIG_TITLE, MB_OK | MB_ICONERROR);
		return;
	}

	std::string none;
	RefillProfiles(hDlg, &none);
	SetStatus(hDlg, Format(INPUTCONFIG_MSG_PROFILE_DELETED, U2T(name).c_str()));
}

// The user picked a name from the Profile list: show which controller it was made for.
static void OnProfilePicked(HWND hDlg)
{
	int sel = (int)SendDlgItemMessage(hDlg, IDC_PROFILECOMBO, CB_GETCURSEL, 0, 0);
	if (sel < 0) return;
	int len = (int)SendDlgItemMessage(hDlg, IDC_PROFILECOMBO, CB_GETLBTEXTLEN, (WPARAM)sel, 0);
	if (len <= 0) return;

	std::vector<TCHAR> text((size_t)len + 1, TEXT('\0'));
	SendDlgItemMessage(hDlg, IDC_PROFILECOMBO, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)&text[0]);

	ip::Profile prof;
	if (LoadProfileFile(ip::SanitizeProfileName(T2U(&text[0])), prof) != PL_OK) return;

	if (!prof.deviceKey.empty())
	{
		int pos = ip::FindDeviceByKey(g_devices, prof.deviceKey, true);
		if (pos >= 0) SelectDevicePos(hDlg, pos);
	}
}

/*---------------------------------------------------------------------------*/

void InputProfilesDlg_Init(HWND hDlg, int padSlot, bool turboView)
{
	SetDlgItemText(hDlg, IDC_GROUP_PROFILES, INPUTCONFIG_GROUP_PROFILES);
	SetDlgItemText(hDlg, IDC_LABEL_DEVICE, INPUTCONFIG_LABEL_DEVICE);
	SetDlgItemText(hDlg, IDC_LABEL_PROFILE, INPUTCONFIG_LABEL_PROFILE);
	SetDlgItemText(hDlg, IDC_DEV_REFRESH, INPUTCONFIG_BUTTON_REFRESH);
	SetDlgItemText(hDlg, IDC_AUTOCONFIG, INPUTCONFIG_BUTTON_AUTOCONFIG);
	SetDlgItemText(hDlg, IDC_PROFILE_LOAD, INPUTCONFIG_BUTTON_LOAD);
	SetDlgItemText(hDlg, IDC_PROFILE_SAVE, INPUTCONFIG_BUTTON_SAVE);
	SetDlgItemText(hDlg, IDC_PROFILE_DELETE, INPUTCONFIG_BUTTON_DELETE);
	SetDlgItemText(hDlg, IDC_PROFILE_STATUS, TEXT(""));

	g_devices = InputProfiles_EnumerateDevices();
	FillDeviceCombo(hDlg);
	RefillProfiles(hDlg, NULL);

	InputProfilesDlg_OnSlotChanged(hDlg, padSlot, turboView);
}

void InputProfilesDlg_OnSlotChanged(HWND hDlg, int padSlot, bool turboView)
{
	PreselectDeviceForPad(hDlg, padSlot);

	// Auto-Configure only fills in the pad itself, so it would look like it did nothing
	// while the Turbo panel is on screen.
	EnableWindow(GetDlgItem(hDlg, IDC_AUTOCONFIG), turboView ? FALSE : TRUE);
}

int InputProfilesDlg_OnCommand(HWND hDlg, int controlId, int notifyCode, int padSlot)
{
	switch (controlId)
	{
		case IDC_DEVCOMBO:
			return IPD_HANDLED;                      // read when Auto-Configure / Load is pressed

		case IDC_DEV_REFRESH:
			if (notifyCode == BN_CLICKED) RefreshDevices(hDlg, padSlot);
			return IPD_HANDLED;

		case IDC_AUTOCONFIG:
			if (notifyCode == BN_CLICKED && DoAutoConfigure(hDlg, padSlot)) return IPD_BINDINGS_CHANGED;
			return IPD_HANDLED;

		case IDC_PROFILECOMBO:
			if (notifyCode == CBN_SELCHANGE) OnProfilePicked(hDlg);
			return IPD_HANDLED;

		case IDC_PROFILE_LOAD:
			if (notifyCode == BN_CLICKED && DoLoadProfile(hDlg, padSlot)) return IPD_BINDINGS_CHANGED;
			return IPD_HANDLED;

		case IDC_PROFILE_SAVE:
			if (notifyCode == BN_CLICKED) DoSaveProfile(hDlg, padSlot);
			return IPD_HANDLED;

		case IDC_PROFILE_DELETE:
			if (notifyCode == BN_CLICKED) DoDeleteProfile(hDlg);
			return IPD_HANDLED;

		case IDOK:
		{
			// Enter while typing a profile name must not close the whole dialog
			HWND focus = GetFocus();
			HWND combo = GetDlgItem(hDlg, IDC_PROFILECOMBO);
			if (focus && combo && (focus == combo || GetParent(focus) == combo))
				return IPD_HANDLED;
			return IPD_NOT_HANDLED;
		}
	}
	return IPD_NOT_HANDLED;
}
