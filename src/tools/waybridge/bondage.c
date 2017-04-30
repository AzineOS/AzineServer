/*
static void bind_shm(struct wl_client* client,
	void* data, uint32_t version, uint32_t id)
{
	struct wl_resource* res = wl_resource_create(client,
		&wl_shm_interface, version, id);
	wl_resource_set_implementation(res, &shm_if, NULL, NULL);
	wl_shm_send_format(res, WL_SHM_FORMAT_XRGB8888);
	wl_shm_send_format(res, WL_SHM_FORMAT_ARGB8888);
}
 */

static void bind_comp(struct wl_client *client,
	void *data, uint32_t version, uint32_t id)
{
	trace("wl_bind(compositor %d:%d)", version, id);
	struct wl_resource* res = wl_resource_create(client,
		&wl_compositor_interface, version, id);
	if (!res){
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(res, &compositor_if, NULL, NULL);
}

static void bind_seat(struct wl_client *client,
	void *data, uint32_t version, uint32_t id)
{
	trace("wl_bind(seat %d:%d)", version, id);
	struct wl_resource* res = wl_resource_create(client,
		&wl_seat_interface, version, id);
	if (!res){
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(res, &seat_if, NULL, NULL);
	wl_seat_send_capabilities(res, WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_TOUCH);
}

static void bind_shell(struct wl_client* client,
	void *data, uint32_t version, uint32_t id)
{
	trace("wl_bind(shell %d:%d)", version, id);
	struct wl_resource* res = wl_resource_create(client,
		&wl_shell_interface, version, id);
	if (!res){
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(res, &shell_if, NULL, NULL);
}

static void bind_xdg(struct wl_client* client,
	void *data, uint32_t version, uint32_t id)
{
	trace("wl_bind(xdg %d:%d)", version, id);
	struct wl_resource* res = wl_resource_create(client,
		&zxdg_shell_v6_interface, version, id);
	if (!res){
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(res, &xdgshell_if, NULL, NULL);
}

static void bind_subcomp(struct wl_client* client,
	void* data, uint32_t version, uint32_t id)
{
	trace("wl_bind(subcomp %d:%d)", version, id);
	struct wl_resource* res = wl_resource_create(client,
		&wl_subcompositor_interface, version, id);
	if (!res){
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(res, &subcomp_if, NULL, NULL);
}

static void bind_output(struct wl_client* client,
	void* data, uint32_t version, uint32_t id)
{
	trace("bind_output");
	struct wl_resource* resource = wl_resource_create(client,
		&wl_output_interface, version, id);
	if (!resource){
		wl_client_post_no_memory(client);
		return;
	}

/* convert the initial display info from x, y to mm using ppcm */
	wl_output_send_geometry(resource, 0, 0,
		(float)wl.init.display_width_px / wl.init.density * 10.0,
		(float)wl.init.display_height_px / wl.init.density * 10.0,
		0, /* init.fonts[0] hinting should work */
		"unknown", "unknown",
		0
	);

	wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT,
		wl.init.display_width_px, wl.init.display_height_px, wl.init.rate);

	if (version >= 2)
		wl_output_send_done(resource);
}
