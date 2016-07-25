-- Quite exhaustive tester for the I/O parts of the event management.
-- (use with -w 640 -h 480)

analogtbl = {};
digitaltbl = {};
translatetbl = {};
statustbl = {};
touchtbl = {};
analogdata = {};

function drawline(text, size)
    if (size == nil) then
	size = 18
    end

    return render_text( [[\ffonts/default.ttf,]] .. size .. " " .. text );
end

function eventtest()
	symtable = system_load("scripts/symtable.lua")();

	analabel = drawline([[\bAnalog]], 18);
	digilabel = drawline([[\bDigital]], 18);
	touchlabel = drawline([[\bTouch]], 18);
	statuslabel = drawline([[\bStatus]], 18);
	translabel = drawline([[\bTranslated]], 18);

	inputanalog_toggle(1);
	tc = null_surface(1, 1);

	local w3 = VRESW / 3;
	local h2 = VRESH / 2;
	move_image(analabel, 0, 0);
	move_image(digilabel, w3, 0);
	move_image(translabel, w3 + w3, 0);
	move_image(touchlabel, 0, h2);
	move_image(statuslabel, w3 + w3, h2);

	show_image({statuslabel, analabel, digilabel, translabel, touchlabel});

-- enumerate the list of found devices
	restbl = inputanalog_query();
	print(#restbl, "entries found");

	for k, v in ipairs(restbl) do
		print("-- new device -- ");
		for i, j in pairs(v) do
			print(i, j);
		end
	end

-- enable analog events
	inputanalog_toggle(1);
end

function round(inn, dec)
	return math.floor( (inn * 10^dec) / 10^dec);
end

function touch_str(iotbl)
	table.insert(touchtbl, string.format(
		"dev(%d:%d) @ %d, %d, press: %.2f, size: %.2f, active: %s",
		iotbl.devid, iotbl.x, iotbl.y, iotbl.subid, iotbl.pressure, iotbl.size,
		iotbl.active and "yes" or "no")
	);
	if (#touchtbl > 10) then
		table.remove(touchtbl, 1);
	end
	local line = table.concat(touchtbl, "\\r\\n");
	if (touchimg) then
		delete_image(touchimg);
	end
	touchimg = drawline(line, 12);
	link_image(touchimg, touchlabel);
	nudge_image(touchimg, 0, 20);
	show_image(touchimg);
end

function digital_str(iotbl)
 	table.insert(digitaltbl, string.format(
		"dev(%d:%d) %s", iotbl.devid, iotbl.subid, iotbl.active and "press" or "release"));

	if (#digitaltbl > 10) then
		table.remove(digitaltbl, 1);
	end

	local line = table.concat(digitaltbl, "\\r\\n");

	if (digitalimg) then
		delete_image(digitalimg);
	end

	digitalimg = drawline(line, 12);
	link_image(digitalimg, digilabel);
	nudge_image(digitalimg, 0, 20);
	show_image(digitalimg);
end

function translate_str(iotbl)
	table.insert(translatetbl, string.format("dev(%d:%d)[mod %s] => %s, %s, %s, %s",
		iotbl.devid, iotbl.subid,
		table.concat(decode_modifiers(iotbl.modifiers),","),
		iotbl.keysym, iotbl.active and "press" or "release",
		symtable[iotbl.keysym] and symtable[iotbl.keysym] or "_nil",
		iotbl.utf8)
	);

	if (#translatetbl > 10) then
		table.remove(translatetbl, 1);
	end

	line = table.concat(translatetbl, [[\r\n]]);

	if (translateimg) then
		delete_image(translateimg);
	end

	translateimg = drawline(line, 12);
	link_image(translateimg, translabel);
	nudge_image(translateimg, 0, 20);
	show_image(translateimg);
end

function analog_str(intbl)
	return string.format("%d:%f/%f avg: %f", intbl.count,
		round(intbl.min, 2), round(intbl.max, 2), round(intbl.avg, 2));
end

tick_counter = 500;
function eventtest_clock_pulse(stamp, delta)
	if (analogimg) then
		delete_image(analogimg);
	end

	line = "";
	for ak, ad in pairs( analogdata ) do
		workline = [[\n\rDevice(]] .. ak .. [[):\n\r\t]];
		for id, iv in pairs( ad ) do
			workline = workline .. " axis: " .. id .. " # " .. analog_str(iv) .. [[\n\r\t]];
		end

		line = line .. workline .. [[\r\n]];
	end

	analogimg = drawline(line, 12);
	link_image(analogimg, analabel);
	nudge_image(analogimg, 0, 20);
	show_image(analogimg);

-- need a fallback counter as all our inputs might be busy
-- should possible have an 'all analog off' switch as well or this
-- might not trigger on noisy sensors
	tick_counter = tick_counter - 1;
	if (tick_counter == 0) then
		return shutdown("timeout");
	else
		delete_image(tc);
		tc = render_text("Shutdown in " .. tostring(tick_counter));
		show_image(tc);
		move_image(tc, 0, VRESH - 20);
	end
end

function eventtest_input( iotbl )
	if (iotbl.digital) then
		tick_counter = 500;
		if (iotbl.translated) then
			translate_str(iotbl);
		else
			digital_str(iotbl);
		end

	elseif (iotbl.touch) then
		touch_str(iotbl);

	elseif (iotbl.analog) then
		local anatbl = {};

		if (analogdata[iotbl.devid] == nil) then
			analogdata[iotbl.devid] = {};
		end

		if (analogdata[iotbl.devid][iotbl.subid] == nil) then
			analogdata[iotbl.devid][iotbl.subid] = anatbl;
			anatbl.count = 0;
			anatbl.min = 65535;
			anatbl.max = 0;
			anatbl.avg = 1;
			anatbl.samples = {};
		end

		anatbl = analogdata[iotbl.devid][iotbl.subid];
		anatbl.count = anatbl.count + 1;
		table.insert(anatbl.samples, iotbl.samples[1]);

		if (iotbl.samples[1] < anatbl.min) then
			anatbl.min = iotbl.samples[1];
		end

		if (iotbl.samples[1] > anatbl.max) then
			anatbl.max = iotbl.samples[1];
		end

		anatbl.avg = (anatbl.avg + iotbl.samples[1]) / 2;

		if (#anatbl.samples > 10) then
			table.remove(anatbl.samples, 1);
		end

		anatbl.match = tbl;
	elseif (iotbl.kind == "status") then
		warning(string.format("status(%d) - %s, %s, %s", iotbl.devid, iotbl.devkind,
			iotbl.label, iotbl.action));
	end
end

