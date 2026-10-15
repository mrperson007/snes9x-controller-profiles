/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// Controller profiles and auto-configure: the platform-independent part.
//
// Nothing in here touches Windows. It works on plain data (a "Bindings" array and
// a description of a game controller), so it can be unit tested anywhere
// (see win32/tests/inputprofiles_core_test.cpp). The Windows side
// (InputProfiles.cpp) feeds it real devices and real SJoypad structs.

#ifndef S9X_INPUTPROFILES_CORE_H_INCLUDED
#define S9X_INPUTPROFILES_CORE_H_INCLUDED

#include <stdint.h>
#include <string>
#include <vector>

namespace inprof {

typedef uint16_t Binding;

/*---------------------------------------------------------------------------*\
  Binding encoding. This mirrors S9xGetState() in win32.cpp:
    keyboard:  a Windows virtual-key code (< 0x100), 0 = unbound
    joystick:  0x8000 | (joystick id << 8) | code
\*---------------------------------------------------------------------------*/
enum
{
	JOY_FLAG        = 0x8000,

	JC_AXIS_LEFT    = 0,    // X axis negative
	JC_AXIS_RIGHT   = 1,
	JC_AXIS_UP      = 2,    // Y axis negative
	JC_AXIS_DOWN    = 3,
	JC_POV_LEFT     = 4,
	JC_POV_RIGHT    = 5,
	JC_POV_UP       = 6,
	JC_POV_DOWN     = 7,
	JC_BUTTON_BASE  = 8,    // button n (0-based) is code 8 + n, n = 0..31
	JC_POV_DNLEFT   = 49,
	JC_POV_DNRIGHT  = 50,
	JC_POV_UPLEFT   = 51,
	JC_POV_UPRIGHT  = 52,

	MAX_JOYSTICKS   = 16
};

inline bool    IsJoy(Binding b)               { return (b & JOY_FLAG) != 0; }
inline int     JoyIndex(Binding b)            { return (b >> 8) & 0x0F; }
inline int     JoyCode(Binding b)             { return b & 0xFF; }
inline Binding MakeJoy(int joy, int code)     { return (Binding)(JOY_FLAG | ((joy & 0x0F) << 8) | (code & 0xFF)); }

/*---------------------------------------------------------------------------*\
  All bindings that belong to one player slot: the 16 pad inputs followed by the
  12 inputs of that slot's "Turbo" panel.
\*---------------------------------------------------------------------------*/
enum Slot
{
	SLOT_UP, SLOT_DOWN, SLOT_LEFT, SLOT_RIGHT,
	SLOT_A, SLOT_B, SLOT_X, SLOT_Y, SLOT_L, SLOT_R, SLOT_START, SLOT_SELECT,
	SLOT_UPLEFT, SLOT_UPRIGHT, SLOT_DOWNLEFT, SLOT_DOWNRIGHT,
	NUM_PAD_SLOTS,                                   // 16

	SLOT_TURBO_A = NUM_PAD_SLOTS, SLOT_TURBO_B, SLOT_TURBO_X, SLOT_TURBO_Y,
	SLOT_TURBO_L, SLOT_TURBO_R, SLOT_TURBO_START, SLOT_TURBO_SELECT,
	SLOT_AUTOHOLD, SLOT_CLEARALL, SLOT_TEMPTURBO, SLOT_AUTOFIRE,
	NUM_SLOTS                                        // 28
};

const char* SlotName(int slot);   // the key used in profile files ("Up", "TurboA", ...)

struct Bindings
{
	Binding v[NUM_SLOTS];
	Bindings() { for (int i = 0; i < NUM_SLOTS; i++) v[i] = 0; }
};

/*---------------------------------------------------------------------------*\
  Generic copy between Bindings and Snes9x's SJoypad pair (main pad + turbo panel).
  A template so this header does not need wsnes9x.h; it works with any struct that
  has SJoypad's field names (the unions Left/Autohold, Right/ClearAll, Up/TempTurbo
  and Down/Autofire share storage in the real struct, which is exactly how the
  turbo panel is stored).
\*---------------------------------------------------------------------------*/
template <class PadT>
void ReadBindings(const PadT& pad, const PadT& turbo, Bindings& out)
{
	out.v[SLOT_UP] = pad.Up;           out.v[SLOT_DOWN] = pad.Down;
	out.v[SLOT_LEFT] = pad.Left;       out.v[SLOT_RIGHT] = pad.Right;
	out.v[SLOT_A] = pad.A;             out.v[SLOT_B] = pad.B;
	out.v[SLOT_X] = pad.X;             out.v[SLOT_Y] = pad.Y;
	out.v[SLOT_L] = pad.L;             out.v[SLOT_R] = pad.R;
	out.v[SLOT_START] = pad.Start;     out.v[SLOT_SELECT] = pad.Select;
	out.v[SLOT_UPLEFT] = pad.Left_Up;  out.v[SLOT_UPRIGHT] = pad.Right_Up;
	out.v[SLOT_DOWNLEFT] = pad.Left_Down; out.v[SLOT_DOWNRIGHT] = pad.Right_Down;

	out.v[SLOT_TURBO_A] = turbo.A;     out.v[SLOT_TURBO_B] = turbo.B;
	out.v[SLOT_TURBO_X] = turbo.X;     out.v[SLOT_TURBO_Y] = turbo.Y;
	out.v[SLOT_TURBO_L] = turbo.L;     out.v[SLOT_TURBO_R] = turbo.R;
	out.v[SLOT_TURBO_START] = turbo.Start; out.v[SLOT_TURBO_SELECT] = turbo.Select;
	out.v[SLOT_AUTOHOLD] = turbo.Autohold;   out.v[SLOT_CLEARALL] = turbo.ClearAll;
	out.v[SLOT_TEMPTURBO] = turbo.TempTurbo; out.v[SLOT_AUTOFIRE] = turbo.Autofire;
}

template <class PadT>
void WriteBindings(const Bindings& in, PadT& pad, PadT& turbo)
{
	pad.Up = in.v[SLOT_UP];            pad.Down = in.v[SLOT_DOWN];
	pad.Left = in.v[SLOT_LEFT];        pad.Right = in.v[SLOT_RIGHT];
	pad.A = in.v[SLOT_A];              pad.B = in.v[SLOT_B];
	pad.X = in.v[SLOT_X];              pad.Y = in.v[SLOT_Y];
	pad.L = in.v[SLOT_L];              pad.R = in.v[SLOT_R];
	pad.Start = in.v[SLOT_START];      pad.Select = in.v[SLOT_SELECT];
	pad.Left_Up = in.v[SLOT_UPLEFT];   pad.Right_Up = in.v[SLOT_UPRIGHT];
	pad.Left_Down = in.v[SLOT_DOWNLEFT]; pad.Right_Down = in.v[SLOT_DOWNRIGHT];

	turbo.A = in.v[SLOT_TURBO_A];      turbo.B = in.v[SLOT_TURBO_B];
	turbo.X = in.v[SLOT_TURBO_X];      turbo.Y = in.v[SLOT_TURBO_Y];
	turbo.L = in.v[SLOT_TURBO_L];      turbo.R = in.v[SLOT_TURBO_R];
	turbo.Start = in.v[SLOT_TURBO_START]; turbo.Select = in.v[SLOT_TURBO_SELECT];
	turbo.Autohold = in.v[SLOT_AUTOHOLD];   turbo.ClearAll = in.v[SLOT_CLEARALL];
	turbo.TempTurbo = in.v[SLOT_TEMPTURBO]; turbo.Autofire = in.v[SLOT_AUTOFIRE];
}

/*---------------------------------------------------------------------------*\
  Game controllers
\*---------------------------------------------------------------------------*/
struct DeviceInfo
{
	int         index;        // Windows joystick id (0..15): what bindings refer to
	std::string name;         // display name, UTF-8
	uint16_t    vendorId;
	uint16_t    productId;
	int         numButtons;
	int         numAxes;
	bool        hasPov;       // has a d-pad / hat
	std::string key;          // stable identity, see AssignDeviceKeys()

	DeviceInfo() : index(0), vendorId(0), productId(0), numButtons(0), numAxes(0), hasPov(false) {}
};

// "045E:028E" when the vendor/product ids are known, otherwise "N:<name>" (the name cut
// to MAX_KEY_NAME_BYTES so a key always fits the fixed-size field in snes9x.conf).
const int MAX_KEY_NAME_BYTES = 64;
std::string DeviceBaseKey(const DeviceInfo& d);

// Sets .key of every device to "<base>#<n>", n counting devices with the same base
// key in list order. Two identical controllers therefore get #0 and #1.
void AssignDeviceKeys(std::vector<DeviceInfo>& devices);

// Position in 'devices' (not the Windows id) of the device with this key, or -1.
// With allowSameModel, a key that is not connected (say "...#1" when only one such
// controller is plugged in) falls back to the first device with the same base key.
// That is right for suggesting a controller for a profile, but not for silently
// re-pointing a player: two players must never end up on one physical controller.
int FindDeviceByKey(const std::vector<DeviceInfo>& devices, const std::string& key, bool allowSameModel = true);

/*---------------------------------------------------------------------------*\
  Re-pointing bindings at another physical controller
\*---------------------------------------------------------------------------*/

// The joystick id used by most of the joystick bindings (lowest id on a tie), or -1
// if every binding is a keyboard key / unbound.
int PrimaryJoyIndex(const Bindings& b);

// Changes every joystick binding that refers to joystick 'fromJoy' so it refers to
// 'toJoy' instead. Button/axis/hat codes are untouched. Returns how many changed.
int RetargetJoystick(Bindings& b, int fromJoy, int toJoy);

/*---------------------------------------------------------------------------*\
  Auto-configure
\*---------------------------------------------------------------------------*/
enum Layout
{
	LAYOUT_XBOX,          // A/B/X/Y = buttons 0-3, Back/Start = 6/7 (XInput order; also the fallback)
	LAYOUT_DIRECTINPUT    // Square/Cross/Circle/Triangle = 0-3, Share/Options = 8/9 (DualShock 4, DualSense, Logitech DInput)
};

Layout      DetectLayout(const DeviceInfo& d);
const char* LayoutName(Layout l);

// Fills the 16 pad slots of 'out' for this controller, "same physical position" style:
// the SNES B button goes on the bottom face button, A on the right, Y on the left,
// X on the top, plus shoulders, Select and Start. D-pad is used for directions when
// the controller has one (including its diagonals), else the left stick.
// The turbo slots of 'out' are left alone. Returns false (and leaves 'out' alone) if
// the device does not look like a usable gamepad.
bool AutoConfigure(const DeviceInfo& d, Bindings& out);

/*---------------------------------------------------------------------------*\
  Profiles: a named set of bindings plus the controller they were made for.
\*---------------------------------------------------------------------------*/
struct Profile
{
	std::string name;
	std::string deviceKey;    // DeviceInfo::key of the controller it was saved from ("" = keyboard only)
	std::string deviceName;   // for display only
	Bindings    bindings;
};

std::string SerializeProfile(const Profile& p);
bool        ParseProfile(const std::string& text, Profile& out);

// Turns what the user typed into something safe to use as a file name (no path
// separators or other characters Windows forbids, no reserved device names, no
// trailing dots/spaces, at most 64 bytes). Returns "" if nothing usable is left.
std::string SanitizeProfileName(const std::string& in);

} // namespace inprof

#endif
