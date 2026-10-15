Snes9x 1.60 - controller profiles, device selection and auto-configure
======================================================================

Where: Options > Input... (the Input Configuration window). There is a new
"Device and Profile" box under the button fields.

What it does
------------
Device     Lists the game controllers Windows sees right now, plus
           "Keyboard / as saved". "Refresh" re-scans (same as the
           "Detect Gamepad Changes" menu item) so you can plug a controller in
           while the window is open.

Auto-Configure
           Pick a controller in the Device list, click Auto-Configure: the
           selected joypad is mapped for that controller in one click.
             - Directions: the d-pad (including its diagonals, which a d-pad
               needs); the left stick if the controller has no d-pad.
             - SNES B/A/Y/X go on the bottom/right/left/top face buttons, L/R
               on the shoulder buttons, Select/Start on Back/Start.
             - Xbox-style button order is used for everything, except
               PlayStation controllers and Logitech pads in DirectInput mode,
               which have their own order.
           A joypad that was switched off is switched on. The Turbo panel is
           left alone. With "Keyboard / as saved" selected, Auto-Configure puts
           the default keyboard keys back (joypads 1 and 2 only).

Profiles   Type a name in the Profile box and click Save to keep the current
           joypad's mapping (pad buttons and its Turbo panel). Pick a name and
           click Load to apply it to whichever joypad is showing - so one
           profile per controller can be reused on any joypad. Delete removes
           it. Profiles are plain text files: InputProfiles\<name>.s9xpad next
           to snes9x.exe, so they can be copied between machines.
           Loading uses the controller shown in the Device list; picking a
           profile selects the controller it was made for, when plugged in.
           Two identical controllers: load the profile, then choose the other
           one in the Device list and load again (the buttons follow the
           selected device).

Remembering controllers
           Windows numbers game controllers in the order it finds them, and
           Snes9x used to store that number, so a mapping broke whenever the
           order changed. Now Snes9x also remembers *which* controller each
           joypad uses (Joypad1:Device ... in snes9x.conf, filled in when you
           press OK). At startup, and on "Detect Gamepad Changes", a joypad's
           buttons follow its controller to its new number. If that controller
           isn't plugged in nothing changes.

Also fixed: Cancel in the Input window now undoes changes to all five Turbo
panels (before, joypads 3-5 kept their Turbo changes).

Not verified - please read
--------------------------
This was written and tested without a Windows machine:
  - The profile / auto-configure logic (win32/InputProfilesCore.*) has 773
    automatic checks (win32/tests, run: sh win32/tests/run_tests.sh).
  - The Windows code was only syntax-checked against stand-in headers. It has
    NOT been built with Visual Studio and NOT run with a real controller.
  - The button layouts are based on how these controllers report themselves
    to Windows; I could not try them on hardware. If a button lands wrong,
    fix it by hand and click Save - the profile keeps your fix.
Expect to read a compiler message or two on the first real build.

Building the .exe
-----------------
A) No installs - GitHub Actions
   1. Make a new GitHub repository and upload everything in this folder
      (controller-profiles.patch, .gitattributes, .github).
   2. Actions tab > "Build Snes9x" > Run workflow.
   3. When it finishes, download the "snes9x-x64" artifact: snes9x-x64.exe.
   (The workflow downloads Snes9x 1.60 itself and applies the patch.)

B) Visual Studio
   1. git clone --branch 1.60 --recurse-submodules https://github.com/snes9xgit/snes9x.git
   2. In that folder: git apply controller-profiles.patch
      (or copy the contents of changed-files\ over the clone)
   3. Open win32\snes9xw.sln, choose "Release Unicode" and "x64". If Visual
      Studio offers to retarget the project to your installed toolset, accept.
   4. Build. Output: win32\snes9x-x64.exe

Files changed
-------------
New:      win32/InputProfilesCore.h/.cpp   logic, no Windows code (tested)
          win32/InputProfiles.h/.cpp       Windows side: controllers, files, window
          win32/tests/                     unit tests
Changed:  win32/wsnes9x.cpp                Input window hooks, factory key copy
          win32/rsrc/snes9x.rc, resource.h new controls
          win32/wlanguage.h                new text
          win32/wconfig.cpp                Joypad1:Device ... settings
          win32/win32.cpp                  re-match controllers at startup
          win32/snes9xw.vcxproj(.filters)  new files

Snes9x's own license (LICENSE in the source) applies to these changes too.
