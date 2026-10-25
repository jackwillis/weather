/* Memory partition: TLS (about 34 KB I/O buffer), the 64 KB HTTP buffer and the console window need more than the
   1 MB default. This SIZE resource overrides the toolchain's default one. */
#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	3 * 1024 * 1024,
	2 * 1024 * 1024
};
