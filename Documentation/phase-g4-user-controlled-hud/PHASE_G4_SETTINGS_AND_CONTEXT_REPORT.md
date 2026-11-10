# Settings and Context Selector

The original Cricket 07 menus were not modified. Android Back opens an app-level `Mobile controls - choose current context` selector outside the game menu architecture. It provides explicit context selection without using timers or guessed engine addresses.

Batting options in that selector are Swipe and Classic Joystick. The chosen batting mode is stored in Android `SharedPreferences` and survives app restart. Active context resets to Hide on launch, pause, and surface destruction. The selection must be explicitly re-armed in open play. Bowling, Running, Ball dead, Fielding (automatic/no controls), and Replay (hidden) are explicit user contexts.

This is a native settings extension, not integration into the original Cricket 07 Settings menu. Access depends on the Android Back action. Direct in-menu integration remains unproven and was not attempted.
