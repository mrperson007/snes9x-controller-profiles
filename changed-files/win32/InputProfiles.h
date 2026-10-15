/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

// Controller profiles, controller selection and auto-configure: the Windows side.
// The logic lives in InputProfilesCore.*; this file talks to winmm (game controllers),
// the file system (profile files) and the Input Configuration dialog.

#ifndef S9X_INPUTPROFILES_H_INCLUDED
#define S9X_INPUTPROFILES_H_INCLUDED

#include <windows.h>
#include <tchar.h>
#include <vector>
#include "InputProfilesCore.h"

#define NUM_PROFILE_PADS        5      // joypads 1-5 are the ones the Input dialog can configure
#define JOYPAD_DEVICE_KEY_LEN   96     // room for any inprof::DeviceInfo::key

// Which physical controller each joypad was last set up for, as inprof::DeviceInfo::key.
// Stored in snes9x.conf (Joypad1:Device ...) so the bindings can follow the controller
// when Windows numbers the game controllers differently from one run to the next.
extern TCHAR JoypadDeviceKey[NUM_PROFILE_PADS][JOYPAD_DEVICE_KEY_LEN];

// Game controllers that are plugged in right now.
std::vector<inprof::DeviceInfo> InputProfiles_EnumerateDevices();

// Called at startup and after "Detect Gamepad Changes": if a joypad's controller is now
// listed under a different joystick number, moves that joypad's bindings along with it.
void InputProfiles_ResolveDevices();

// Called when the Input dialog is accepted: notes which controller each joypad uses.
void InputProfiles_RememberDevices();

// ---- Input Configuration dialog ----
enum
{
	IPD_NOT_HANDLED     = 0,   // not one of ours, the dialog should deal with it
	IPD_HANDLED         = 1,
	IPD_BINDINGS_CHANGED= 2    // handled, and Joypad[] changed: the dialog must redraw its fields
};

// padSlot is 0..4 (joypad 1-5); turboView is true while the "Turbo" panel is displayed.
void InputProfilesDlg_Init(HWND hDlg, int padSlot, bool turboView);
void InputProfilesDlg_OnSlotChanged(HWND hDlg, int padSlot, bool turboView);
int  InputProfilesDlg_OnCommand(HWND hDlg, int controlId, int notifyCode, int padSlot);

#endif
