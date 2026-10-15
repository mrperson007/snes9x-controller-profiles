// Unit tests for the platform-independent part of the controller profile feature.
// Not part of the Visual Studio project. Build and run on any OS, see run_tests.sh.

#include "../InputProfilesCore.h"
#include <stdio.h>
#include <string.h>

#ifdef HAVE_REAL_SJOYPAD
// typedefs the real struct needs; the struct itself is cut out of wsnes9x.h by run_tests.sh
typedef unsigned short WORD;
typedef int BOOL;
#include "sjoypad_real.h"
#endif

using namespace inprof;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; if (!((a) == (b))) { g_failed++; printf("  FAIL line %d: %s == %s\n", __LINE__, #a, #b); } } while (0)

static DeviceInfo Dev(int index, const char* name, unsigned vid, unsigned pid, int buttons, int axes, bool pov)
{
	DeviceInfo d;
	d.index = index; d.name = name; d.vendorId = (uint16_t)vid; d.productId = (uint16_t)pid;
	d.numButtons = buttons; d.numAxes = axes; d.hasPov = pov;
	return d;
}

static void test_encoding()
{
	printf("encoding\n");
	Binding b = MakeJoy(3, 52);
	CHECK_EQ(b, 0x8334);
	CHECK(IsJoy(b)); CHECK_EQ(JoyIndex(b), 3); CHECK_EQ(JoyCode(b), 52);
	CHECK(!IsJoy(0x41)); CHECK(!IsJoy(0));
	CHECK_EQ(MakeJoy(0, JC_BUTTON_BASE + 5), 0x800D);
	CHECK_EQ(MakeJoy(15, 7), 0x8F07);
	CHECK_EQ(MakeJoy(16, 7), 0x8007);               // id wraps like the 4 bit field in S9xGetState
}

static void test_slot_names()
{
	printf("slot names\n");
	for (int i = 0; i < NUM_SLOTS; i++)
	{
		CHECK(strlen(SlotName(i)) > 0);
		for (int j = i + 1; j < NUM_SLOTS; j++) CHECK(strcmp(SlotName(i), SlotName(j)) != 0);
	}
	CHECK_EQ(NUM_SLOTS, 28);
	CHECK_EQ(NUM_PAD_SLOTS, 16);
	CHECK(strcmp(SlotName(-1), "") == 0);
	CHECK(strcmp(SlotName(NUM_SLOTS), "") == 0);
}

static void test_primary_and_retarget()
{
	printf("primary / retarget\n");
	Bindings b;
	CHECK_EQ(PrimaryJoyIndex(b), -1);

	b.v[SLOT_UP] = 'W'; b.v[SLOT_A] = 'Z';              // keyboard only
	CHECK_EQ(PrimaryJoyIndex(b), -1);

	b.v[SLOT_B] = MakeJoy(4, 8);
	CHECK_EQ(PrimaryJoyIndex(b), 4);
	b.v[SLOT_X] = MakeJoy(1, 9);                        // 1 vs 1: tie -> lowest id
	CHECK_EQ(PrimaryJoyIndex(b), 1);
	b.v[SLOT_Y] = MakeJoy(4, 10);                       // 4 now has two
	CHECK_EQ(PrimaryJoyIndex(b), 4);
	b.v[SLOT_AUTOHOLD] = MakeJoy(1, 11); b.v[SLOT_TURBO_A] = MakeJoy(1, 12);   // turbo slots count too
	CHECK_EQ(PrimaryJoyIndex(b), 1);

	CHECK_EQ(RetargetJoystick(b, 4, 7), 2);
	CHECK_EQ(b.v[SLOT_B], MakeJoy(7, 8));
	CHECK_EQ(b.v[SLOT_Y], MakeJoy(7, 10));
	CHECK_EQ(b.v[SLOT_X], MakeJoy(1, 9));               // other device untouched
	CHECK_EQ(b.v[SLOT_UP], 'W');                        // keyboard untouched
	CHECK_EQ(b.v[SLOT_A], 'Z');

	CHECK_EQ(RetargetJoystick(b, 1, 1), 0);             // same id
	CHECK_EQ(RetargetJoystick(b, -1, 2), 0);
	CHECK_EQ(RetargetJoystick(b, 1, 16), 0);
	CHECK_EQ(RetargetJoystick(b, 9, 2), 0);             // nothing on 9
}

static void test_device_keys()
{
	printf("device keys\n");
	std::vector<DeviceInfo> v;
	v.push_back(Dev(0, "Pad A", 0x045E, 0x028E, 10, 5, true));
	v.push_back(Dev(1, "Pad A", 0x045E, 0x028E, 10, 5, true));    // identical model
	v.push_back(Dev(3, "Wireless Controller", 0x054C, 0x09CC, 14, 6, true));
	v.push_back(Dev(4, "Mystery", 0, 0, 8, 2, false));            // no ids: falls back to the name
	AssignDeviceKeys(v);

	CHECK(v[0].key == "045E:028E#0");
	CHECK(v[1].key == "045E:028E#1");
	CHECK(v[2].key == "054C:09CC#0");
	CHECK(v[3].key == "N:Mystery#0");

	CHECK_EQ(FindDeviceByKey(v, "045E:028E#1"), 1);
	CHECK_EQ(FindDeviceByKey(v, "054C:09CC#0"), 2);
	CHECK_EQ(FindDeviceByKey(v, "N:Mystery#0"), 3);
	CHECK_EQ(FindDeviceByKey(v, "045E:028E#7"), 0);               // same model, other ordinal -> first of that model
	CHECK_EQ(FindDeviceByKey(v, "045E:028E#7", false), -1);       // ... unless only an exact match is acceptable
	CHECK_EQ(FindDeviceByKey(v, "045E:028E#1", false), 1);
	CHECK_EQ(FindDeviceByKey(v, "1234:5678#0"), -1);
	CHECK_EQ(FindDeviceByKey(v, ""), -1);
	CHECK_EQ(FindDeviceByKey(std::vector<DeviceInfo>(), "045E:028E#0"), -1);

	// one of the two identical pads is unplugged: the #0 pad is still found, #1 only on request
	std::vector<DeviceInfo> one(v.begin(), v.begin() + 1);
	AssignDeviceKeys(one);
	CHECK_EQ(FindDeviceByKey(one, "045E:028E#0", false), 0);
	CHECK_EQ(FindDeviceByKey(one, "045E:028E#1"), 0);
	CHECK_EQ(FindDeviceByKey(one, "045E:028E#1", false), -1);   // player 2 must not grab player 1's pad

	// very long names still give short keys, and a multi-byte character is never cut in half
	std::vector<DeviceInfo> lng;
	lng.push_back(Dev(0, (std::string(63, 'x') + "\xC3\xA9" + "tail").c_str(), 0, 0, 8, 2, false));
	AssignDeviceKeys(lng);
	CHECK(lng[0].key == "N:" + std::string(63, 'x') + "#0");
	std::vector<DeviceInfo> lng2;
	lng2.push_back(Dev(0, std::string(200, 'y').c_str(), 0, 0, 8, 2, false));
	AssignDeviceKeys(lng2);
	CHECK_EQ(lng2[0].key.size(), (size_t)(2 + MAX_KEY_NAME_BYTES + 2));
}

static void test_autoconfigure_xbox_pov()
{
	printf("auto-configure: Xbox layout, d-pad\n");
	DeviceInfo d = Dev(2, "Controller (XBOX 360 For Windows)", 0x045E, 0x028E, 10, 5, true);
	CHECK_EQ(DetectLayout(d), LAYOUT_XBOX);

	Bindings b;
	b.v[SLOT_A] = 'V'; b.v[SLOT_UPLEFT] = 'Q';                    // old stuff must go
	b.v[SLOT_TURBO_A] = 'T'; b.v[SLOT_AUTOHOLD] = 'H';            // turbo panel must stay
	CHECK(AutoConfigure(d, b));

	CHECK_EQ(b.v[SLOT_UP],    MakeJoy(2, JC_POV_UP));
	CHECK_EQ(b.v[SLOT_DOWN],  MakeJoy(2, JC_POV_DOWN));
	CHECK_EQ(b.v[SLOT_LEFT],  MakeJoy(2, JC_POV_LEFT));
	CHECK_EQ(b.v[SLOT_RIGHT], MakeJoy(2, JC_POV_RIGHT));
	CHECK_EQ(b.v[SLOT_UPLEFT],    MakeJoy(2, 51));
	CHECK_EQ(b.v[SLOT_UPRIGHT],   MakeJoy(2, 52));
	CHECK_EQ(b.v[SLOT_DOWNLEFT],  MakeJoy(2, 49));
	CHECK_EQ(b.v[SLOT_DOWNRIGHT], MakeJoy(2, 50));
	CHECK_EQ(b.v[SLOT_B],      MakeJoy(2, 8 + 0));                // bottom face button
	CHECK_EQ(b.v[SLOT_A],      MakeJoy(2, 8 + 1));                // right
	CHECK_EQ(b.v[SLOT_Y],      MakeJoy(2, 8 + 2));                // left
	CHECK_EQ(b.v[SLOT_X],      MakeJoy(2, 8 + 3));                // top
	CHECK_EQ(b.v[SLOT_L],      MakeJoy(2, 8 + 4));
	CHECK_EQ(b.v[SLOT_R],      MakeJoy(2, 8 + 5));
	CHECK_EQ(b.v[SLOT_SELECT], MakeJoy(2, 8 + 6));
	CHECK_EQ(b.v[SLOT_START],  MakeJoy(2, 8 + 7));
	CHECK_EQ(b.v[SLOT_TURBO_A], 'T');
	CHECK_EQ(b.v[SLOT_AUTOHOLD], 'H');
	CHECK_EQ(PrimaryJoyIndex(b), 2);

	// every assigned binding must be distinct (no two buttons on one input)
	for (int i = 0; i < NUM_PAD_SLOTS; i++)
		for (int j = i + 1; j < NUM_PAD_SLOTS; j++)
			if (b.v[i] != 0) CHECK(b.v[i] != b.v[j]);
}

static void test_autoconfigure_playstation()
{
	printf("auto-configure: PlayStation layout\n");
	DeviceInfo d = Dev(0, "Wireless Controller", 0x054C, 0x09CC, 14, 6, true);
	CHECK_EQ(DetectLayout(d), LAYOUT_DIRECTINPUT);
	Bindings b;
	CHECK(AutoConfigure(d, b));
	CHECK_EQ(b.v[SLOT_B],      MakeJoy(0, 8 + 1));                // Cross
	CHECK_EQ(b.v[SLOT_A],      MakeJoy(0, 8 + 2));                // Circle
	CHECK_EQ(b.v[SLOT_Y],      MakeJoy(0, 8 + 0));                // Square
	CHECK_EQ(b.v[SLOT_X],      MakeJoy(0, 8 + 3));                // Triangle
	CHECK_EQ(b.v[SLOT_L],      MakeJoy(0, 8 + 4));
	CHECK_EQ(b.v[SLOT_R],      MakeJoy(0, 8 + 5));
	CHECK_EQ(b.v[SLOT_SELECT], MakeJoy(0, 8 + 8));                // Share
	CHECK_EQ(b.v[SLOT_START],  MakeJoy(0, 8 + 9));                // Options

	CHECK_EQ(DetectLayout(Dev(0, "Logitech Dual Action", 0x046D, 0xC216, 12, 4, true)), LAYOUT_DIRECTINPUT);
	CHECK_EQ(DetectLayout(Dev(0, "Logitech F310 in XInput mode", 0x046D, 0xC21D, 10, 5, true)), LAYOUT_XBOX);
	CHECK_EQ(DetectLayout(Dev(0, "DualSense Wireless Controller", 0, 0, 14, 6, true)), LAYOUT_DIRECTINPUT);   // ids unknown, name known
	CHECK_EQ(DetectLayout(Dev(0, "Some pad", 0, 0, 10, 4, true)), LAYOUT_XBOX);                                // unknown -> fallback
}

static void test_autoconfigure_edge_cases()
{
	printf("auto-configure: edge cases\n");

	// no d-pad: left stick, and no diagonals
	{
		DeviceInfo d = Dev(5, "Stick pad", 0x1234, 0x5678, 8, 2, false);
		Bindings b; b.v[SLOT_UPLEFT] = 'Q';
		CHECK(AutoConfigure(d, b));
		CHECK_EQ(b.v[SLOT_UP],    MakeJoy(5, JC_AXIS_UP));
		CHECK_EQ(b.v[SLOT_DOWN],  MakeJoy(5, JC_AXIS_DOWN));
		CHECK_EQ(b.v[SLOT_LEFT],  MakeJoy(5, JC_AXIS_LEFT));
		CHECK_EQ(b.v[SLOT_RIGHT], MakeJoy(5, JC_AXIS_RIGHT));
		CHECK_EQ(b.v[SLOT_UPLEFT], 0); CHECK_EQ(b.v[SLOT_UPRIGHT], 0);
		CHECK_EQ(b.v[SLOT_DOWNLEFT], 0); CHECK_EQ(b.v[SLOT_DOWNRIGHT], 0);
	}
	// only 6 buttons: Select/Start (buttons 6,7) do not exist and must stay unbound
	{
		DeviceInfo d = Dev(1, "Small pad", 0x1111, 0x2222, 6, 2, true);
		Bindings b;
		CHECK(AutoConfigure(d, b));
		CHECK_EQ(b.v[SLOT_R], MakeJoy(1, 8 + 5));
		CHECK_EQ(b.v[SLOT_SELECT], 0);
		CHECK_EQ(b.v[SLOT_START], 0);
	}
	// not a usable gamepad: false, and the bindings are left alone
	{
		Bindings b; b.v[SLOT_A] = 'V'; b.v[SLOT_UP] = MakeJoy(3, 6);
		CHECK(!AutoConfigure(Dev(0, "Throttle", 1, 1, 1, 3, false), b));    // < 2 buttons
		CHECK(!AutoConfigure(Dev(0, "Button box", 1, 1, 8, 0, false), b));  // no hat, no axes
		CHECK(!AutoConfigure(Dev(16, "Bad id", 1, 1, 10, 2, true), b));
		CHECK(!AutoConfigure(Dev(-1, "Bad id", 1, 1, 10, 2, true), b));
		CHECK_EQ(b.v[SLOT_A], 'V');
		CHECK_EQ(b.v[SLOT_UP], MakeJoy(3, 6));
	}
	// 32 buttons never index past bit 31
	{
		DeviceInfo d = Dev(0, "Big", 1, 1, 32, 6, true);
		Bindings b; CHECK(AutoConfigure(d, b));
		CHECK_EQ(JoyCode(b.v[SLOT_START]), 8 + 7);
	}
}

static void test_reorder_scenario()
{
	printf("scenario: controller enumerates at a different id\n");
	// Player 1 was configured while the pad was joystick 0
	std::vector<DeviceInfo> before;
	before.push_back(Dev(0, "Pad", 0x045E, 0x028E, 10, 5, true));
	AssignDeviceKeys(before);
	Bindings b;
	AutoConfigure(before[0], b);
	std::string remembered = before[0].key;
	Bindings original = b;

	// next session Windows lists a different device first
	std::vector<DeviceInfo> after;
	after.push_back(Dev(0, "Other", 0x046D, 0xC21D, 10, 5, true));
	after.push_back(Dev(1, "Pad", 0x045E, 0x028E, 10, 5, true));
	AssignDeviceKeys(after);

	int pos = FindDeviceByKey(after, remembered);
	CHECK_EQ(pos, 1);
	int from = PrimaryJoyIndex(b);
	CHECK_EQ(from, 0);
	CHECK(RetargetJoystick(b, from, after[pos].index) > 0);
	for (int i = 0; i < NUM_PAD_SLOTS; i++)
	{
		if (IsJoy(original.v[i]))
		{
			CHECK_EQ(JoyCode(b.v[i]), JoyCode(original.v[i]));    // same buttons ...
			CHECK_EQ(JoyIndex(b.v[i]), 1);                         // ... on the new id
		}
	}
}

static void test_profile_roundtrip()
{
	printf("profile files\n");
	Profile p;
	p.name = "Player 1 = Xbox, caf\xC3\xA9";
	p.deviceKey = "045E:028E#0";
	p.deviceName = "Controller (XBOX 360 For Windows)";
	for (int i = 0; i < NUM_SLOTS; i++) p.bindings.v[i] = (Binding)(0x8000 + i * 257 + (i == 3 ? 0x7000 : 0));
	p.bindings.v[SLOT_UP] = 'W';
	p.bindings.v[SLOT_DOWN] = 0;

	std::string text = SerializeProfile(p);
	Profile q;
	CHECK(ParseProfile(text, q));
	CHECK(q.name == p.name); CHECK(q.deviceKey == p.deviceKey); CHECK(q.deviceName == p.deviceName);
	for (int i = 0; i < NUM_SLOTS; i++) CHECK_EQ(q.bindings.v[i], p.bindings.v[i]);

	// the same with LF-only line ends, a BOM, comments, blank lines and different key case
	std::string lf;
	for (size_t i = 0; i < text.size(); i++) if (text[i] != '\r') lf += text[i];
	Profile r;
	CHECK(ParseProfile("\xEF\xBB\xBF" + lf, r));
	CHECK(r.bindings.v[SLOT_UP] == 'W');

	Profile s;
	CHECK(ParseProfile("# hand written\n\nFORMAT = 1\nup = 0x8006\n  Down=  1234  \nbogus=5\nthis line has no equals\nA=0x1FFFF\nB=banana\nX=\n", s));
	CHECK_EQ(s.bindings.v[SLOT_UP], 0x8006);
	CHECK_EQ(s.bindings.v[SLOT_DOWN], 1234);
	CHECK_EQ(s.bindings.v[SLOT_A], 0);                  // out of range -> ignored
	CHECK_EQ(s.bindings.v[SLOT_B], 0);                  // not a number -> ignored
	CHECK_EQ(s.bindings.v[SLOT_X], 0);                  // empty -> ignored

	Profile t; t.name = "keep";
	CHECK(!ParseProfile("Up=0x8006\n", t));             // no Format line
	CHECK(!ParseProfile("", t));
	CHECK(!ParseProfile("Format=2\nUp=0x8006\n", t));   // from the future
	CHECK(!ParseProfile("Format=1x\n", t));
	CHECK(t.name == "keep");                            // failed parses do not touch the output

	// multi-line values cannot break the file
	Profile u; u.name = "two\r\nlines"; u.deviceName = "x\ny";
	Profile v; CHECK(ParseProfile(SerializeProfile(u), v));
	CHECK(v.name == "two  lines"); CHECK(v.deviceName == "x y");
}

static void test_sanitize()
{
	printf("profile names\n");
	CHECK(SanitizeProfileName("My Pad") == "My Pad");
	CHECK(SanitizeProfileName("  padded  ") == "padded");
	CHECK(SanitizeProfileName("a/b\\c:d*e?f\"g<h>i|j") == "a_b_c_d_e_f_g_h_i_j");
	CHECK(SanitizeProfileName("..\\..\\evil") == "..__..__evil" || SanitizeProfileName("..\\..\\evil") == ".._.._evil");
	CHECK(SanitizeProfileName("trailing dots...") == "trailing dots");
	CHECK(SanitizeProfileName("tab\there\n") == "tabhere");
	CHECK(SanitizeProfileName("") == "");
	CHECK(SanitizeProfileName("   ") == "");
	CHECK(SanitizeProfileName("...") == "");
	CHECK(SanitizeProfileName("con") == "_con");
	CHECK(SanitizeProfileName("NUL.txt") == "_NUL.txt");
	CHECK(SanitizeProfileName("Com3") == "_Com3");
	CHECK(SanitizeProfileName("lpt9") == "_lpt9");
	CHECK(SanitizeProfileName("COM0") == "COM0");          // only COM1-9 are reserved
	CHECK(SanitizeProfileName("console") == "console");
	CHECK(SanitizeProfileName("caf\xC3\xA9") == "caf\xC3\xA9");

	std::string longName(100, 'x');
	CHECK_EQ(SanitizeProfileName(longName).size(), (size_t)64);
	// 63 ASCII bytes + a 2-byte character: cutting at 64 would split it, so it is dropped whole
	std::string split = std::string(63, 'x') + "\xC3\xA9" + "yyy";
	CHECK(SanitizeProfileName(split) == std::string(63, 'x'));
}

#ifdef HAVE_REAL_SJOYPAD
static void test_sjoypad_mapping()
{
	printf("SJoypad <-> Bindings (real struct from wsnes9x.h)\n");
	SJoypad pad, turbo;
	memset(&pad, 0, sizeof(pad)); memset(&turbo, 0, sizeof(turbo));

	Bindings in;
	for (int i = 0; i < NUM_SLOTS; i++) in.v[i] = (Binding)(0x100 + i);   // every slot unique
	WriteBindings(in, pad, turbo);

	CHECK_EQ(pad.Up, 0x100 + SLOT_UP);           CHECK_EQ(pad.Down, 0x100 + SLOT_DOWN);
	CHECK_EQ(pad.Left, 0x100 + SLOT_LEFT);       CHECK_EQ(pad.Right, 0x100 + SLOT_RIGHT);
	CHECK_EQ(pad.Left_Up, 0x100 + SLOT_UPLEFT);  CHECK_EQ(pad.Right_Down, 0x100 + SLOT_DOWNRIGHT);
	CHECK_EQ(pad.Start, 0x100 + SLOT_START);     CHECK_EQ(pad.Select, 0x100 + SLOT_SELECT);
	// the turbo panel reuses the direction fields for its four extra inputs
	CHECK_EQ(turbo.Left,  0x100 + SLOT_AUTOHOLD);
	CHECK_EQ(turbo.Right, 0x100 + SLOT_CLEARALL);
	CHECK_EQ(turbo.Up,    0x100 + SLOT_TEMPTURBO);
	CHECK_EQ(turbo.Down,  0x100 + SLOT_AUTOFIRE);
	CHECK_EQ(turbo.A,     0x100 + SLOT_TURBO_A);

	Bindings out;
	ReadBindings(pad, turbo, out);
	for (int i = 0; i < NUM_SLOTS; i++) CHECK_EQ(out.v[i], in.v[i]);

	// an Enabled flag next to the bindings is not touched by either direction
	pad.Enabled = 1; turbo.Enabled = 0;
	WriteBindings(in, pad, turbo);
	CHECK_EQ(pad.Enabled, 1); CHECK_EQ(turbo.Enabled, 0);
}
#endif

int main()
{
	test_encoding();
	test_slot_names();
	test_primary_and_retarget();
	test_device_keys();
	test_autoconfigure_xbox_pov();
	test_autoconfigure_playstation();
	test_autoconfigure_edge_cases();
	test_reorder_scenario();
	test_profile_roundtrip();
	test_sanitize();
#ifdef HAVE_REAL_SJOYPAD
	test_sjoypad_mapping();
#else
	printf("(SJoypad mapping test skipped: build with HAVE_REAL_SJOYPAD, see run_tests.sh)\n");
#endif
	printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed ? 1 : 0;
}
