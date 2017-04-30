static void xdg_getsurf(struct wl_client* client,
	struct wl_resource* res, uint32_t id, struct wl_resource* surf_res)
{
	trace("get xdg-shell surface");
	struct bridge_surf* surf = wl_resource_get_user_data(surf_res);
	struct wl_resource* xdgsurf_res = wl_resource_create(client,
		&zxdg_surface_v6_interface, wl_resource_get_version(res), id);

	if (!xdgsurf_res){
		wl_resource_post_no_memory(res);
		return;
	}

	wl_resource_set_implementation(xdgsurf_res, &xdgsurf_if, surf, NULL);
	zxdg_surface_v6_send_configure(xdgsurf_res, wl_display_next_serial(wl.disp));
}

static void xdg_pong(
	struct wl_client* client, struct wl_resource* res, uint32_t serial)
{
	trace("xdg_pong(%PRIu32)", serial);
}

static void xdg_createpos(
	struct wl_client* client, struct wl_resource* res, uint32_t id)
{
	trace("xdg_createpos");
}

static void xdg_destroy(
	struct wl_client* client, struct wl_resource* res)
{
	trace("xdg_destroy");
}

