# Phase G.1 Automatic Fielding Preservation

No manual fielding HUD is enabled. The mobile overlay is hidden by default and the native phase gate prevents invisible mobile controls from consuming touches in fielding/unknown contexts. No fielder movement, catching, pickup, throw or wicketkeeper code was changed.

`BTN_DIVE`, throw actions and manual-fielding mode remain declarations in the inherited header, but no such controls are exposed. Runtime fielding/catching behavior was not tested in this correction. Automatic fielding preservation is therefore **PARTIAL by source inspection; gameplay verification NOT TESTED**.
