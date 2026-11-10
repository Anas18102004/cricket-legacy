/* Per-game settings for the Windows layer: Cricket 07 (EA Sports, 2006). */
#ifndef RC_PROFILE_H
#define RC_PROFILE_H

#define PROFILE_EXE        "Cricket07.exe"     /* the executable, relative to the game folder */
#define PROFILE_GUEST_ROOT "C:\\Cricket07"     /* the install folder, as the game sees it */
#define PROFILE_GUEST_EXE  "Cricket07.exe"     /* the executable's name, as the game sees it */
#define PROFILE_TITLE      "Cricket 07"         /* native window title */

/* Cricket 07 profile flags */
#define PROFILE_CRICKET07  1
#define PROFILE_LAZY_VIEWS 1
#define PROFILE_CURSOR     0xa01970u   /* the menu cursor (floats x, y in 640x480): taps steer it (input.c) */

#define PROFILE_REGISTRY \
    { "SOFTWARE\\EA SPORTS\\Cricket 07", "Install Dir", 1, PROFILE_GUEST_ROOT, 0 }, \
    { "SOFTWARE\\EA SPORTS\\Cricket 07", "InstallDir", 1, PROFILE_GUEST_ROOT, 0 }, \
    { "SOFTWARE\\EA SPORTS\\Cricket 07", "Language", 1, "English", 0 }, \
    { "SOFTWARE\\EA SPORTS\\Cricket 07", "Locale", 1, "en_uk", 0 }, \
    { "SOFTWARE\\EA SPORTS\\Cricket 07", "Version", 1, "1.0", 0 }

#endif
