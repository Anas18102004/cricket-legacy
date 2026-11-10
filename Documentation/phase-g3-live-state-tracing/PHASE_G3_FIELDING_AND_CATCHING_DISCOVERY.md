# Fielding and Catching Discovery

**Status: NOT TESTED in live gameplay.** No fielder movement, ball pickup, catch, wicketkeeper or throw event was manually observed.

RTTI includes `CFielderManager`, `CCatchBehaviour`, `CKeeperBehaviour`; generated functions are present. Inspection of `CFielderManager::vfunc_0` at `f_005aabe0` shows a wrapper/delegation to another function, which is insufficient to assign a gameplay action. Automatic behavior and runtime call chain remain unproven.

G.3 has not introduced manual fielding inputs or modified fielding code. Preserve automatic fielding. Before any mobile fielding control is considered, follow event dispatch/callers and verify behavior in an actual catch/pickup/wicketkeeper sequence.
