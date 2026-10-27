// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Jack Willis
// The "Location" dialog's data and validation, independent of how it is drawn.
#pragma once
#include "app/textedit.hpp"
#include "core/config.hpp"

namespace wx {

struct LocationForm {
	enum Field { Lat = 0, Lon = 1, Name = 2, Units = 3, NFields = 4 };
	TextEdit<16> lat, lon;
	TextEdit<48> name;
	bool metric = false;
	int focus = Lat;
	const char *error = "";   // static text from the last failed read()

	void load(const Config &c)
	{
		Buf<16> a, b;
		a.add_fixed(c.lat_e4, 4);
		b.add_fixed(c.lon_e4, 4);
		lat.set(a.view());
		lon.set(b.view());
		name.set(c.name);
		metric = c.metric;
		focus = Lat;
		error = "";
	}

	// The field being edited, or nullptr when focus is on the units toggle.
	TextEdit<48> *edit_name() { return focus == Name ? &name : nullptr; }
	void next() { focus = (focus + 1) % NFields; }
	void prev() { focus = (focus + NFields - 1) % NFields; }

	// Validate and apply to *out (a copy of the current config). On failure *out is untouched.
	Status read(Config *out)
	{
		Config c = *out;
		if (!config_set(&c, "latitude", trim(lat.view()), 0).ok()) {
			error = "Latitude must be a number from -90 to 90, like 43.0747";
			focus = Lat;
			return make(Err::Config, error);
		}
		if (!config_set(&c, "longitude", trim(lon.view()), 0).ok()) {
			error = "Longitude must be a number from -180 to 180, like -89.3841";
			focus = Lon;
			return make(Err::Config, error);
		}
		if (!config_set(&c, "name", trim(name.view()), 0).ok()) {
			error = "The name may not contain '#'";
			focus = Name;
			return make(Err::Config, error);
		}
		c.metric = metric;
		*out = c;
		error = "";
		return okay();
	}
};

} // namespace wx
