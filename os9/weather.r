/* Memory partition: TLS (about 34 KB I/O buffer), the 64 KB HTTP buffer and the buffered native window need more than the
   1 MB default. This SIZE resource overrides the toolchain's default one. */
#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
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

#include "Dialogs.r"
resource 'DLOG' (200) {
    {90, 100, 365, 440}, dBoxProc, invisible, noGoAway, 0, 200,
    "Location", centerMainScreen
};
resource 'DITL' (200) {
    {
        {235, 236, 255, 318}, Button { enabled, "Apply" };
        {235, 140, 255, 222}, Button { enabled, "Cancel" };
        {24, 112, 42, 314}, EditText { enabled, "" };
        {58, 112, 76, 314}, EditText { enabled, "" };
        {92, 112, 110, 314}, EditText { enabled, "" };
        {124, 24, 144, 314}, CheckBox { enabled, "Metric units" };
        {150, 24, 170, 314}, CheckBox { enabled, "Automatic updates" };
        {24, 20, 42, 104}, StaticText { disabled, "Latitude:" };
        {58, 20, 76, 104}, StaticText { disabled, "Longitude:" };
        {92, 20, 110, 104}, StaticText { disabled, "Name:" };
        {181, 20, 226, 320}, StaticText { disabled, "Tab: next field. Return: Apply. Escape: Cancel." }
    }
};
