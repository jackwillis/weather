/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis */
#include "Processes.r"
resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    needsActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    3 * 1024 * 1024, 2 * 1024 * 1024
};
