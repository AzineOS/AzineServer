/*
 * Copyright 2014-2016, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 * Description: Platform that draws to an arcan display server using the shmif.
 * Multiple displays are simulated when we explicitly get a subsegment pushed
 * to us although they only work with the agp readback approach currently.
 */

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <poll.h>
#include <setjmp.h>
#include <math.h>

extern jmp_buf arcanmain_recover_state;

#include "../video_platform.h"
#include "../agp/glfun.h"

#define WANT_ARCAN_SHMIF_HELPER
#include "arcan_shmif.h"
#include "arcan_math.h"
#include "arcan_general.h"
#include "arcan_video.h"
#include "arcan_event.h"
#include "arcan_videoint.h"
#include "arcan_renderfun.h"

#define EGL_EGLEXT_PROTOTYPES
#define MESA_EGL_NO_X11_HEADERS
#include <EGL/egl.h>
#include <EGL/eglext.h>

static char* synchopts[] = {
	"parent", "display server controls synchronisation",
	"pre-wake", "use the cost of and jitter from previous frames",
	"adaptive", "skip a frame if syncpoint is at risk",
	NULL
};

static char* input_envopts[] = {
	NULL
};

static enum {
	PARENT = 0,
	PREWAKE,
	ADAPTIVE
} syncopt;

static struct monitor_mode mmodes[] = {
	{
		.id = 0,
		.width = 640,
		.height = 480,
		.refresh = 60,
		.depth = sizeof(av_pixel) * 8,
		.dynamic = true
	},
};

#define MAX_DISPLAYS 8

struct display {
	struct arcan_shmif_cont conn;
	bool mapped, visible, focused, dirty;
	enum dpms_state dpms;
	struct storage_info_t* vstore;
	float ppcm;
	int id;
} disp[MAX_DISPLAYS];

static struct arg_arr* shmarg;
static bool nopass;

bool platform_video_init(uint16_t width, uint16_t height, uint8_t bpp,
	bool fs, bool frames, const char* title)
{
	static bool first_init = true;

	for (size_t i = 0; i < MAX_DISPLAYS; i++)
		disp[i].id = i;

	if (first_init){
/* we send our own register events, so set empty type */
		disp[0].conn = arcan_shmif_open(0, 0, &shmarg);
		if (disp[0].conn.addr == NULL){
			arcan_warning("video setup failed : couldn't connect to parent\n");
			return false;
		}

/* setup requires shmif connection as we might get metadata that way */
	enum shmifext_setup_status status;
	if ((status = arcan_shmifext_headless_setup(&disp[0].conn,
		arcan_shmifext_headless_defaults())) != SHMIFEXT_OK){
		arcan_warning("headless graphics setup failed, code: %d\n", status);
		arcan_shmif_drop(&disp[0].conn);
		return false;
	}

/* empty dimensions will retain what the connection was setup for, though some
 * arcan setups still don't preset- good values for an external connection
 * point, so clamp */
		disp[0].conn.hints = SHMIF_RHINT_ORIGO_LL;
		size_t dw = width ? width : disp[0].conn.w;
		if (dw < 320)
			dw < 320;
		size_t dh = height ? height : disp[0].conn.h;
		if (dh < 200)
			dh < 200;

		if (!arcan_shmif_resize_ext( &disp[0].conn, dw, dh,
			(struct shmif_resize_ext){.abuf_sz = 1, .abuf_cnt = 8, .vbuf_cnt = 1}
		)){
			arcan_warning("couldn't set shm dimensions (%d, %d)\n", width, height);
			return false;
		}

/* we provide our own cursor that is blended in the output */
		arcan_shmif_enqueue(&disp[0].conn, &(struct arcan_event){
			.category = EVENT_EXTERNAL,
			.ext.kind = ARCAN_EVENT(CURSORHINT),
			.ext.message = "hidden"
		});

/* disp[0] always start out mapped / enabled and we'll use the
 * current world unless overridden */
		disp[0].mapped = true;
		disp[0].ppcm = ARCAN_SHMPAGE_DEFAULT_PPCM;
		disp[0].dpms = ADPMS_ON;
		disp[0].visible = true;
		disp[0].focused = true;
		first_init = false;
	}
	else if (width && height){
		if (!arcan_shmif_resize( &disp[0].conn, width, height )){
			arcan_warning("couldn't set shm dimensions (%d, %d)\n", width, height);
			return false;
		}
	}

/*
 * currently, we actually never de-init this, just generate a nonsense hash-id
 * before we get a better (signatures etc.) solution
 */
	unsigned long h = 5381;
	const char* str = title;
	for (; *str; str++)
		h = (h << 5) + h + *str;

	arcan_shmif_setprimary(SHMIF_INPUT, &disp[0].conn);
	struct arcan_event ev = {
		.category = EVENT_EXTERNAL,
		.ext.kind = ARCAN_EVENT(REGISTER),
		.ext.registr.kind = SEGID_LWA,
		.ext.registr.guid = {h, 0}
	};
	snprintf(ev.ext.registr.title,
		COUNT_OF(ev.ext.registr.title), "%s", title);
	arcan_shmif_enqueue(&disp[0].conn, &ev);
	return true;
}

/*
 * These are just direct maps that will be statically sucked in
 */
void platform_video_shutdown()
{
	for (size_t i=0; i < MAX_DISPLAYS; i++)
		if (disp[i].conn.addr)
			arcan_shmif_drop(&disp[i].conn);
}

bool platform_video_display_edid(platform_display_id did,
	char** out, size_t* sz)
{
	*out = NULL;
	*sz = 0;
	return false;
}

void platform_video_prepare_external()
{
/* comes with switching in AGP, should give us card- switching
 * as well (and will be used as test case for that) */
}

void platform_video_restore_external()
{
}

void* platform_video_gfxsym(const char* sym)
{
	return arcan_shmifext_headless_lookup(&disp[0].conn, sym);
}

void platform_video_setsynch(const char* arg)
{
	int ind = 0;

	while(synchopts[ind]){
		if (strcmp(synchopts[ind], arg) == 0){
			syncopt = (ind > 0 ? ind / 2 : ind);
			break;
		}

		ind += 2;
	}
}

int platform_video_cardhandle(int cardn)
{
	return -1;
}

void platform_event_samplebase(int devid, float xyz[3])
{
}

const char** platform_video_synchopts()
{
	return (const char**) synchopts;
}

static const char* arcan_envopts[] = {
	NULL
};

const char** platform_video_envopts()
{
	return arcan_envopts;
}

const char** platform_input_envopts()
{
	return (const char**) input_envopts;
}

static struct monitor_mode* get_platform_mode(platform_mode_id mode)
{
	for (size_t i = 0; i < sizeof(mmodes)/sizeof(mmodes[0]); i++){
		if (mmodes[i].id == mode)
			return &mmodes[i];
	}

	return NULL;
}

bool platform_video_specify_mode(platform_display_id id,
	struct monitor_mode mode)
{
	if (!(id < MAX_DISPLAYS && disp[id].conn.addr))
		return false;

	return (mode.width > 0 && mode.height > 0 &&
		arcan_shmif_resize(&disp[id].conn, mode.width, mode.height));
}

struct monitor_mode platform_video_dimensions()
{
	struct monitor_mode mode = {
		.width = disp[0].conn.addr->w,
		.height = disp[0].conn.addr->h,
	};
	mode.phy_width = (float)mode.width / disp[0].ppcm * 10.0;
	mode.phy_height = (float)mode.height / disp[0].ppcm * 10.0;

	return mode;
}

bool platform_video_set_mode(platform_display_id id, platform_mode_id newmode)
{
	struct monitor_mode* mode = get_platform_mode(newmode);

	if (!mode)
		return false;

	if (!(id < MAX_DISPLAYS && disp[id].conn.addr))
		return false;

	return arcan_shmif_resize(&disp[id].conn, mode->width, mode->height);

	return false;
}

static bool check_store(platform_display_id id)
{
	struct storage_info_t* vs = (disp[id].vstore ?
		disp[id].vstore : arcan_vint_world());

	if (vs->w != disp[id].conn.w || vs->h != disp[id].conn.h){
		if (!arcan_shmif_resize(&disp[id].conn, vs->w, vs->h)){
			arcan_warning("platform_video_map_display(), attempt to switch "
				"display output mode to match backing store failed.\n");
			return false;
		}
	}
	return true;
}

/*
 * Two things that are currently wrong with this approach to mapping:
 * 1. hint is ignored entirely, mapping mode is just based on WORLDID
 * 2. the texture coordinates of the source are not being ignored.
 *
 * For these to be solved, we need to extend the full path of shmif rhints
 * to cover all possible mapping modes, and a on-gpu rtarget- style blit
 * with extra buffer or partial synch and VIEWPORT events.
 */
bool platform_video_map_display(arcan_vobj_id vid, platform_display_id id,
	enum blitting_hint hint)
{
	if (id > MAX_DISPLAYS)
		return false;

	if (disp[id].vstore){
		arcan_vint_drop_vstore(disp[id].vstore);
		disp[id].vstore = NULL;
	}

	disp[id].mapped = false;

	if (vid == ARCAN_VIDEO_WORLDID){
		disp[id].conn.hints = SHMIF_RHINT_ORIGO_LL;
		disp[id].vstore = arcan_vint_world();
	}
	else if (vid == ARCAN_EID)
		return true;
	else{
		arcan_vobject* vobj = arcan_video_getobject(vid);
		if (vobj == NULL){
			arcan_warning("platform_video_map_display(), attempted to map a "
				"non-existing video object");
			return false;
		}

		if (vobj->vstore->txmapped != TXSTATE_TEX2D){
			arcan_warning("platform_video_map_display(), attempted to map a "
				"video object with an invalid backing store");
			return false;
		}

		disp[id].conn.hints = 0;
		disp[id].vstore = vobj->vstore;
	}

/*
 * enforce display size constraint, this wouldn't be necessary
 * when doing a buffer passing operation
 */
	if (!check_store(id))
		return false;

	disp[id].vstore->refcount++;
	disp[id].mapped = true;
	disp[id].dirty = true;

	return true;
}

struct monitor_mode* platform_video_query_modes(
	platform_display_id id, size_t* count)
{
	*count = sizeof(mmodes) / sizeof(mmodes[0]);

	return mmodes;
}

void platform_video_query_displays()
{
}

bool platform_video_map_handle(struct storage_info_t* store, int64_t handle)
{
	return false;
}

/*
 * we use a deferred stub here to avoid having the headless platform
 * sync function generate bad statistics due to our two-stage synch
 * process
 */
static void stub()
{
}

static void synch_copy(struct display* disp, struct storage_info_t* vs)
{
	check_store(disp->id);
	struct storage_info_t store = *vs;
	store.vinf.text.raw = disp->conn.vidp;

	agp_readback_synchronous(&store);
	arcan_shmif_signal(&disp->conn, SHMIF_SIGVID);
}

void platform_video_synch(uint64_t tick_count, float fract,
	video_synchevent pre, video_synchevent post)
{
	if (pre)
		pre();

	unsigned long long frametime = arcan_timemillis();

	size_t platform_nupd;
	arcan_bench_register_cost(arcan_vint_refresh(fract, &platform_nupd));

/* actually needed here or handle content will be broken */
	glFlush();

	unsigned long long ts = arcan_timemillis();
	if (ts < frametime){
		frametime = (unsigned long long)-1 - frametime + ts;
	}
	else
		frametime = ts - frametime;

	for (size_t i = 0; i < MAX_DISPLAYS; i++){
		if (!(disp[i].dirty || (platform_nupd && disp[i].visible)))
			continue;

		if (!disp[i].mapped || disp[i].dpms != ADPMS_ON)
			continue;

		enum status_handle status;
		disp[i].dirty = false;
		struct storage_info_t* vs = disp[i].vstore ?
			disp[i].vstore : arcan_vint_world();

		if (-1 == arcan_shmifext_eglsignal(&disp[i].conn, 0,
			SHMIF_SIGVID, vs->vinf.text.glid)){
			synch_copy(&disp[i], vs);
		}
	}

	unsigned long long synchtime = arcan_timemillis();
	if (synchtime < ts){
		synchtime = (unsigned long long)-1 - synchtime + ts;
	}
	else
		synchtime = synchtime - ts;

/*
 * missing synchronization strategy setting here entirely, this
 * is just based on an assumed ~16ish max delay using the rendertime
 */
	if (frametime + synchtime < 16){
		struct pollfd pfd = {
			.fd = disp[0].conn.epipe,
			.events = POLLIN | POLLERR | POLLHUP | POLLNVAL
		};
		poll(&pfd, 1, 16 - frametime - synchtime);
	}

	if (post)
		post();
}

/*
 * The regular event layer is just stubbed, when the filtering etc.
 * is broken out of the platform layer, we can re-use that to have
 * local filtering untop of the one the engine is doing.
 */
arcan_errc platform_event_analogstate(int devid, int axisid,
	int* lower_bound, int* upper_bound, int* deadzone,
	int* kernel_size, enum ARCAN_ANALOGFILTER_KIND* mode)
{
	return ARCAN_ERRC_NO_SUCH_OBJECT;
}

void platform_event_analogall(bool enable, bool mouse)
{
}

void platform_event_analogfilter(int devid,
	int axisid, int lower_bound, int upper_bound, int deadzone,
	int buffer_sz, enum ARCAN_ANALOGFILTER_KIND kind)
{
}

/*
 * For LWA simulated multidisplay, we still simulate disable by
 * drawing an empty output display.
 */
enum dpms_state
	platform_video_dpms(platform_display_id did, enum dpms_state state)
{
	if (!(did < MAX_DISPLAYS && did[disp].mapped))
		return ADPMS_IGNORE;

	if (state == ADPMS_IGNORE)
		return disp[did].dpms;

	disp[did].dpms = state;

	return state;
}

const char* platform_video_capstr()
{
	return "Video Platform (Arcan - in - Arcan)\n";
}

const char* platform_event_devlabel(int devid)
{
	return "no device";
}

/*
 * Ignoring mapping the segment will mean that it will eventually timeout,
 * either long (seconds+) or short (empty outevq and frames but no
 * response).
 */
static void map_window(struct arcan_shmif_cont* seg, arcan_evctx* ctx,
	int kind, const char* key)
{

/* encoder should really be mapped as an avfeed- type frameserver,
 * maintain it in a 'pending' slot, enqueue some notification and
 * let alloc_target() handle it */
	if (kind == SEGID_ENCODER){
		arcan_warning("(FIXME) SEGID_ENCODER type not yet supported.\n");
		return;
	}

/*
 * we encode all our IDs (except clipboard) with the internal VID and
 * connected to a rendertarget slot, so re-use that fact.
 */

	struct display* base = NULL;
	size_t i = 0;

	for (; i < MAX_DISPLAYS; i++)
		if (disp[i].conn.addr == NULL){
			base = disp + i;
			break;
		}

	if (base == NULL){
		arcan_warning("Hard-coded display-limit reached (%d), "
			"ignoring new segment.\n", (int)MAX_DISPLAYS);
		return;
	}

	base->conn = arcan_shmif_acquire(seg, key, SEGID_LWA, SHMIF_DISABLE_GUARD);
	base->ppcm = ARCAN_SHMPAGE_DEFAULT_PPCM;
	base->dpms = ADPMS_ON;
	base->visible = true;
	base->dirty = true;

	arcan_event ev = {
		.category = EVENT_VIDEO,
		.vid.kind = EVENT_VIDEO_DISPLAY_ADDED,
		.vid.source = -1,
		.vid.displayid = i,
		.vid.width = seg->w,
		.vid.height = seg->h,
	};

	arcan_event_enqueue(ctx, &ev);
}

/*
 * return true if the segment has expired
 */
static bool event_process_disp(arcan_evctx* ctx, struct display* d)
{
	if (!d->conn.addr)
		return true;

	arcan_event ev;

	while (1 == arcan_shmif_poll(&d->conn, &ev))
		if (ev.category == EVENT_TARGET)
		switch(ev.tgt.kind){

/*
 * We use subsegments forced from the parent- side as an analog for
 * hotplug displays, giving developers a testbed for a rather hard
 * feature and at the same time get to evaluate the API.
 * Other enhancements would be to let alloc_surface+flag for rendertargets
 * act as newseg request and map the new rendertarget to that segment.
 *
 * Note: we don't handle SEGID_CLIPBOARD_PASTE yet, as these come as DISPLAY_
 * ADDED we can hook them up to default handlers matching kind as the caller
 * is allowed to change anyhow.
*/
		case TARGET_COMMAND_NEWSEGMENT:
			map_window(&d->conn, ctx, ev.tgt.ioevs[0].iv, ev.tgt.message);
		break;

/*
 * Depends on active synchronization strategy, could also be used with a
 * 'every tick' timer to synch clockrate to server or have a single-frame
 * stepping mode. This ought to be used with the ability to set RT clocking
 * mode
 */
		case TARGET_COMMAND_STEPFRAME:
		break;

/*
 * We can't automatically resize as the layouting in the running appl may not
 * be able to handle relayouting in an event-driven manner, so we translate and
 * forward as a monitor event.
 */
		case TARGET_COMMAND_DISPLAYHINT:
			if (ev.tgt.ioevs[0].iv && ev.tgt.ioevs[1].iv){
				arcan_event_enqueue(ctx, &(arcan_event) {
					.category = EVENT_VIDEO,
					.vid.kind = EVENT_VIDEO_DISPLAY_RESET,
					.vid.source = -1,
					.vid.displayid = d->id,
					.vid.width = ev.tgt.ioevs[0].iv,
					.vid.height = ev.tgt.ioevs[1].iv,
					.vid.flags = ev.tgt.ioevs[2].iv,
					.vid.vppcm = ev.tgt.ioevs[4].fv,
				});
			}

			if (!(ev.tgt.ioevs[2].iv & 128)){
				bool vss = !((ev.tgt.ioevs[2].iv & 2) > 0);
				if (vss && !d->visible){
					d->dirty = true;
				}
				d->visible = vss;
				d->focused = !((ev.tgt.ioevs[2].iv & 4) > 0);
			}

/*
 * If the density has changed, grab the current standard font size
 * and convert to mm to get the scaling factor, apply and update default
 */
			if (ev.tgt.ioevs[4].fv > 0){
				int font_sz;
				int hint;
				arcan_video_fontdefaults(NULL, &font_sz, &hint);
				float sf = ev.tgt.ioevs[4].fv / d->ppcm;
				arcan_video_defaultfont("arcan-default",
					BADFD, (float)font_sz * sf, hint, false);
				d->ppcm = ev.tgt.ioevs[4].fv;
			}
		break;
/*
 * This behavior may be a bit strong, but we allow the display server
 * to override the default font (if provided)
 */
		case TARGET_COMMAND_FONTHINT:{
			int newfd = BADFD;
			int font_sz = 0;
			int hint = ev.tgt.ioevs[3].iv;

			if (ev.tgt.ioevs[1].iv == 1 && BADFD != ev.tgt.ioevs[0].iv){
				newfd = dup(ev.tgt.ioevs[0].iv);
			};

			if (ev.tgt.ioevs[2].fv > 0)
				font_sz = ceilf(d->ppcm * ev.tgt.ioevs[2].fv);
			arcan_video_defaultfont("arcan-default",
				newfd, font_sz, hint, ev.tgt.ioevs[4].iv);

			arcan_event_enqueue(ctx, &(arcan_event){
				.category = EVENT_VIDEO,
				.vid.kind = EVENT_VIDEO_DISPLAY_RESET,
				.vid.source = -2,
				.vid.displayid = d->id,
				.vid.vppcm = ev.tgt.ioevs[2].fv,
				.vid.width = ev.tgt.ioevs[3].iv
			});
		}
		break;

		case TARGET_COMMAND_BUFFER_FAIL:
			nopass = true;
		break;

/*
 * This is harsher than perhaps necessary as this does not care
 * for adoption of old connections, they are just killed off.
 */
		case TARGET_COMMAND_RESET:
			longjmp(arcanmain_recover_state, 2);
		break;

/*
 * The nodes have already been unlinked, so all cleanup
 * can be made when the process dies.
 */
		case TARGET_COMMAND_EXIT:
			if (d == &disp[0]){
				ev.category = EVENT_SYSTEM;
				ev.sys.kind = EVENT_SYSTEM_EXIT;
				arcan_event_enqueue(ctx, &ev);
			}
/* Need to explicitly drop single segment */
			else {
				arcan_event ev = {
					.category = EVENT_VIDEO,
					.vid.kind = EVENT_VIDEO_DISPLAY_REMOVED,
					.vid.displayid = d->id
				};
				arcan_event_enqueue(ctx, &ev);
				free(d->conn.user);
				arcan_shmif_drop(&d->conn);
				if (d->vstore)
					arcan_vint_drop_vstore(d->vstore);

				memset(d, '\0', sizeof(struct display));
			}
			return true; /* it's not safe here */
		break;

		default:
		break;
		}
		else
			arcan_event_enqueue(ctx, &ev);

	return false;
}

void platform_input_help()
{
}

void platform_event_keyrepeat(arcan_evctx* ctx, int* period, int* delay)
{
	*period = 0;
	*delay = 0;
/* in principle, we could use the tick, implied in _process,
 * track the latest input event that corresponded to a translated
 * keyboard device (track per devid) and emit that every oh so often */
}

void platform_event_process(arcan_evctx* ctx)
{
/*
 * Most events can just be added to the local queue,
 * but we want to handle some of the target commands separately
 * (with a special path to LUA and a different hook)
 */
	for (size_t i = 0; i < MAX_DISPLAYS; i++)
		event_process_disp(ctx, &disp[i]);
}

void platform_event_rescan_idev(arcan_evctx* ctx)
{
}

enum PLATFORM_EVENT_CAPABILITIES platform_input_capabilities()
{
	return ACAP_TRANSLATED | ACAP_MOUSE | ACAP_TOUCH |
		ACAP_POSITION | ACAP_ORIENTATION;
}

void platform_key_repeat(arcan_evctx* ctx, unsigned int rate)
{
}

void platform_event_deinit(arcan_evctx* ctx)
{
}

void platform_video_recovery()
{
	arcan_event ev = {
		.category = EVENT_VIDEO,
		.vid.kind = EVENT_VIDEO_DISPLAY_ADDED
	};
	arcan_evctx* evctx = arcan_event_defaultctx();
	arcan_event_enqueue(evctx, &ev);

	for (size_t i = 0; i < MAX_DISPLAYS; i++){
		platform_video_map_display(ARCAN_VIDEO_WORLDID, i, HINT_NONE);
		ev.vid.source = -1;
		ev.vid.displayid = i;
		arcan_event_enqueue(evctx, &ev);
	}
}

void platform_event_reset(arcan_evctx* ctx)
{
	platform_event_deinit(ctx);
}

void platform_device_lock(int devind, bool state)
{
}

void platform_event_init(arcan_evctx* ctx)
{
}

