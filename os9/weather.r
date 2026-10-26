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
        {181, 20, 226, 320}, StaticText { disabled, "Tab: next field. Return: Apply. Escape: Cancel." };
        {235, 20, 255, 122}, Button { enabled, "Find..." }
    }
};

resource 'DLOG' (201) {
    {60, 80, 430, 560}, dBoxProc, invisible, noGoAway, 0, 201,
    "Find a place", centerMainScreen
};
resource 'DITL' (201) {
    {
        {332, 282, 352, 364}, Button { enabled, "Use" };
        {332, 376, 352, 460}, Button { enabled, "Cancel" };
        {18, 20, 40, 340}, EditText { enabled, "" };
        {18, 354, 40, 460}, Button { enabled, "Search" };
        {90, 20, 112, 460}, RadioButton { enabled, "" };
        {118, 20, 140, 460}, RadioButton { enabled, "" };
        {146, 20, 168, 460}, RadioButton { enabled, "" };
        {174, 20, 196, 460}, RadioButton { enabled, "" };
        {202, 20, 224, 460}, RadioButton { enabled, "" };
        {50, 20, 82, 460}, StaticText { disabled, "" };
        {298, 20, 316, 460}, StaticText { disabled, "Place data: Open-Meteo / GeoNames (CC BY 4.0)" };
        {232, 20, 288, 460}, StaticText { disabled, "" }
    }
};
