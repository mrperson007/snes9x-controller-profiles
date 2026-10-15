/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// Controller profiles and auto-configure: the platform-independent part.
// See InputProfilesCore.h.

#include "InputProfilesCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace inprof {

static const char* const kSlotNames[NUM_SLOTS] =
{
	"Up", "Down", "Left", "Right",
	"A", "B", "X", "Y", "L", "R", "Start", "Select",
	"UpLeft", "UpRight", "DownLeft", "DownRight",
	"TurboA", "TurboB", "TurboX", "TurboY", "TurboL", "TurboR", "TurboStart", "TurboSelect",
	"Autohold", "ClearAll", "TempTurbo", "Autofire"
};

const char* SlotName(int slot)
{
	return (slot >= 0 && slot < NUM_SLOTS) ? kSlotNames[slot] : "";
}

/*---------------------------------------------------------------------------*/
// small string helpers

static char LowerAscii(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool EqualNoCase(const std::string& a, const char* b)
{
	size_t n = strlen(b);
	if (a.size() != n) return false;
	for (size_t i = 0; i < n; i++)
		if (LowerAscii(a[i]) != LowerAscii(b[i])) return false;
	return true;
}

static bool ContainsNoCase(const std::string& hay, const char* needle)
{
	size_t n = strlen(needle);
	if (n == 0) return true;
	if (hay.size() < n) return false;
	for (size_t i = 0; i + n <= hay.size(); i++)
	{
		size_t j = 0;
		while (j < n && LowerAscii(hay[i + j]) == LowerAscii(needle[j])) j++;
		if (j == n) return true;
	}
	return false;
}

static std::string Trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) b++;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) e--;
	return s.substr(b, e - b);
}

// values are written on one line
static std::string OneLine(const std::string& s)
{
	std::string r = s;
	for (size_t i = 0; i < r.size(); i++)
		if (r[i] == '\r' || r[i] == '\n') r[i] = ' ';
	return r;
}

/*---------------------------------------------------------------------------*/
// device identity

std::string DeviceBaseKey(const DeviceInfo& d)
{
	if (d.vendorId != 0 || d.productId != 0)
	{
		char buf[16];
		sprintf(buf, "%04X:%04X", (unsigned)d.vendorId, (unsigned)d.productId);
		return buf;
	}

	std::string name = d.name;
	if (name.size() > (size_t)MAX_KEY_NAME_BYTES)
	{
		size_t len = MAX_KEY_NAME_BYTES;
		while (len > 0 && ((unsigned char)name[len] & 0xC0) == 0x80) len--;   // do not split a UTF-8 character
		name.erase(len);
	}
	return "N:" + name;
}

static std::string StripOrdinal(const std::string& key)
{
	size_t hash = key.rfind('#');
	return hash == std::string::npos ? key : key.substr(0, hash);
}

void AssignDeviceKeys(std::vector<DeviceInfo>& devices)
{
	for (size_t i = 0; i < devices.size(); i++)
	{
		std::string base = DeviceBaseKey(devices[i]);
		int ordinal = 0;
		for (size_t j = 0; j < i; j++)
			if (DeviceBaseKey(devices[j]) == base) ordinal++;

		char buf[16];
		sprintf(buf, "#%d", ordinal);
		devices[i].key = base + buf;
	}
}

int FindDeviceByKey(const std::vector<DeviceInfo>& devices, const std::string& key, bool allowSameModel)
{
	if (key.empty()) return -1;

	for (size_t i = 0; i < devices.size(); i++)
		if (devices[i].key == key) return (int)i;

	if (!allowSameModel) return -1;

	std::string base = StripOrdinal(key);
	for (size_t i = 0; i < devices.size(); i++)
		if (StripOrdinal(devices[i].key) == base) return (int)i;

	return -1;
}

/*---------------------------------------------------------------------------*/
// re-pointing bindings

int PrimaryJoyIndex(const Bindings& b)
{
	int counts[MAX_JOYSTICKS] = { 0 };
	for (int i = 0; i < NUM_SLOTS; i++)
		if (IsJoy(b.v[i])) counts[JoyIndex(b.v[i])]++;

	int best = -1, bestCount = 0;
	for (int j = 0; j < MAX_JOYSTICKS; j++)
		if (counts[j] > bestCount) { best = j; bestCount = counts[j]; }
	return best;
}

int RetargetJoystick(Bindings& b, int fromJoy, int toJoy)
{
	if (fromJoy < 0 || fromJoy >= MAX_JOYSTICKS || toJoy < 0 || toJoy >= MAX_JOYSTICKS || fromJoy == toJoy)
		return 0;

	int changed = 0;
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		if (IsJoy(b.v[i]) && JoyIndex(b.v[i]) == fromJoy)
		{
			b.v[i] = MakeJoy(toJoy, JoyCode(b.v[i]));
			changed++;
		}
	}
	return changed;
}

/*---------------------------------------------------------------------------*/
// auto-configure

namespace
{
	// 0-based joystick button numbers (button n is binding code 8 + n)
	struct ButtonMap { int B, A, Y, X, L, R, Select, Start; };

	// A/B/X/Y = 0/1/2/3, LB/RB = 4/5, Back/Start = 6/7. Positionally: SNES B is the bottom
	// face button (Xbox A), SNES A the right one (Xbox B), SNES Y the left one (Xbox X)
	// and SNES X the top one (Xbox Y).
	const ButtonMap kXbox        = { 0, 1, 2, 3, 4, 5, 6, 7 };

	// Square/Cross/Circle/Triangle = 0/1/2/3, L1/R1 = 4/5, Share/Options = 8/9.
	// SNES B = Cross, A = Circle, Y = Square, X = Triangle.
	const ButtonMap kDirectInput = { 1, 2, 0, 3, 4, 5, 8, 9 };
}

Layout DetectLayout(const DeviceInfo& d)
{
	const unsigned vid = d.vendorId, pid = d.productId;

	if (vid == 0x054C)                                   // Sony: DualShock 4, DualSense, ...
		return LAYOUT_DIRECTINPUT;

	if (vid == 0x046D && (pid == 0xC216 || pid == 0xC218 || pid == 0xC219))  // Logitech in DirectInput mode
		return LAYOUT_DIRECTINPUT;

	if (vid == 0 && (ContainsNoCase(d.name, "dualshock") || ContainsNoCase(d.name, "dualsense")))
		return LAYOUT_DIRECTINPUT;

	return LAYOUT_XBOX;
}

const char* LayoutName(Layout l)
{
	switch (l)
	{
		case LAYOUT_DIRECTINPUT: return "PlayStation / DirectInput";
		case LAYOUT_XBOX:
		default:                 return "Xbox";
	}
}

bool AutoConfigure(const DeviceInfo& d, Bindings& out)
{
	if (d.index < 0 || d.index >= MAX_JOYSTICKS) return false;
	if (d.numButtons < 2) return false;
	if (!d.hasPov && d.numAxes < 2) return false;

	Bindings b = out;
	for (int i = 0; i < NUM_PAD_SLOTS; i++) b.v[i] = 0;

	if (d.hasPov)
	{
		b.v[SLOT_UP]    = MakeJoy(d.index, JC_POV_UP);
		b.v[SLOT_DOWN]  = MakeJoy(d.index, JC_POV_DOWN);
		b.v[SLOT_LEFT]  = MakeJoy(d.index, JC_POV_LEFT);
		b.v[SLOT_RIGHT] = MakeJoy(d.index, JC_POV_RIGHT);
		// A hat reports a diagonal as one position, not as two directions, so the
		// diagonals need bindings of their own or they would do nothing.
		b.v[SLOT_UPLEFT]    = MakeJoy(d.index, JC_POV_UPLEFT);
		b.v[SLOT_UPRIGHT]   = MakeJoy(d.index, JC_POV_UPRIGHT);
		b.v[SLOT_DOWNLEFT]  = MakeJoy(d.index, JC_POV_DNLEFT);
		b.v[SLOT_DOWNRIGHT] = MakeJoy(d.index, JC_POV_DNRIGHT);
	}
	else
	{
		// Left stick. Two axes crossing their thresholds already press two directions at
		// once, so no diagonal bindings are needed.
		b.v[SLOT_UP]    = MakeJoy(d.index, JC_AXIS_UP);
		b.v[SLOT_DOWN]  = MakeJoy(d.index, JC_AXIS_DOWN);
		b.v[SLOT_LEFT]  = MakeJoy(d.index, JC_AXIS_LEFT);
		b.v[SLOT_RIGHT] = MakeJoy(d.index, JC_AXIS_RIGHT);
	}

	const ButtonMap& m = (DetectLayout(d) == LAYOUT_DIRECTINPUT) ? kDirectInput : kXbox;
	const struct { int slot; int button; } assign[] =
	{
		{ SLOT_B, m.B }, { SLOT_A, m.A }, { SLOT_Y, m.Y }, { SLOT_X, m.X },
		{ SLOT_L, m.L }, { SLOT_R, m.R }, { SLOT_SELECT, m.Select }, { SLOT_START, m.Start }
	};
	for (size_t i = 0; i < sizeof(assign) / sizeof(assign[0]); i++)
	{
		// never bind a button the controller does not have
		if (assign[i].button < d.numButtons && assign[i].button < 32)
			b.v[assign[i].slot] = MakeJoy(d.index, JC_BUTTON_BASE + assign[i].button);
	}

	out = b;
	return true;
}

/*---------------------------------------------------------------------------*/
// profile files

std::string SerializeProfile(const Profile& p)
{
	std::string s;
	s += "; Snes9x controller profile\r\n";
	s += "Format=1\r\n";
	s += "Name=" + OneLine(p.name) + "\r\n";
	s += "DeviceKey=" + OneLine(p.deviceKey) + "\r\n";
	s += "DeviceName=" + OneLine(p.deviceName) + "\r\n";
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		char buf[32];
		sprintf(buf, "=0x%04X\r\n", (unsigned)p.bindings.v[i]);
		s += kSlotNames[i];
		s += buf;
	}
	return s;
}

bool ParseProfile(const std::string& textIn, Profile& out)
{
	std::string text = textIn;
	if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
		text.erase(0, 3);                                   // UTF-8 byte order mark

	Profile p;
	bool haveFormat = false;

	size_t pos = 0;
	while (pos < text.size())
	{
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos) eol = text.size();
		std::string line = Trim(text.substr(pos, eol - pos));
		pos = eol + 1;

		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = Trim(line.substr(0, eq));
		std::string val = Trim(line.substr(eq + 1));

		if (EqualNoCase(key, "Format"))
		{
			if (val != "1") return false;                   // written by a newer version
			haveFormat = true;
		}
		else if (EqualNoCase(key, "Name"))        p.name = val;
		else if (EqualNoCase(key, "DeviceKey"))   p.deviceKey = val;
		else if (EqualNoCase(key, "DeviceName"))  p.deviceName = val;
		else
		{
			for (int i = 0; i < NUM_SLOTS; i++)
			{
				if (EqualNoCase(key, kSlotNames[i]))
				{
					char* end = NULL;
					unsigned long v = strtoul(val.c_str(), &end, 0);
					if (end != val.c_str() && *end == '\0' && v <= 0xFFFF)
						p.bindings.v[i] = (Binding)v;
					break;
				}
			}
		}
	}

	if (!haveFormat) return false;
	out = p;
	return true;
}

std::string SanitizeProfileName(const std::string& in)
{
	std::string s;
	for (size_t i = 0; i < in.size(); i++)
	{
		unsigned char c = (unsigned char)in[i];
		if (c < 0x20 || c == 0x7F) continue;               // control characters
		if (strchr("\\/:*?\"<>|", c) != NULL) c = '_';     // not allowed in Windows file names
		s += (char)c;
	}

	// at most 64 bytes, without cutting a UTF-8 character in half
	const size_t kMax = 64;
	if (s.size() > kMax)
	{
		size_t len = kMax;
		while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--;
		s.erase(len);
	}

	s = Trim(s);
	while (!s.empty() && (s[s.size() - 1] == '.' || s[s.size() - 1] == ' ')) s.erase(s.size() - 1);
	if (s.empty()) return s;

	// CON, NUL, COM1, ... cannot be used as file names, with or without an extension
	std::string stem = s.substr(0, s.find('.'));
	while (!stem.empty() && stem[stem.size() - 1] == ' ') stem.erase(stem.size() - 1);
	bool reserved = EqualNoCase(stem, "CON") || EqualNoCase(stem, "PRN") || EqualNoCase(stem, "AUX") || EqualNoCase(stem, "NUL");
	if (stem.size() == 4 && (EqualNoCase(stem.substr(0, 3), "COM") || EqualNoCase(stem.substr(0, 3), "LPT")) && stem[3] >= '1' && stem[3] <= '9')
		reserved = true;
	if (reserved) s = "_" + s;

	return s;
}

} // namespace inprof
