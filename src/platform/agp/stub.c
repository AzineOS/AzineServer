/*
 * Copyright 2014-2015, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "../video_platform.h"
#include "../platform.h"

#include "arcan_math.h"
#include "arcan_general.h"
#include "arcan_video.h"
#include "arcan_videoint.h"

agp_shader_id agp_default_shader(enum SHADER_TYPES type)
{
	return 1;
}

const char* agp_ident()
{
	return "STUB";
}

const char* agp_shader_language()
{
	return "STUB";
}

int agp_shader_envv(enum arcan_shader_envts slot, void* value, size_t size)
{
	return 1;
}

agp_shader_id arcan_shader_lookup(const char* tag)
{
	return 1;
}

void agp_shader_source(enum SHADER_TYPES type,
	const char** vert, const char** frag)
{
	*vert = "";
	*frag = "";
}

const char** agp_envopts()
{
	static const char* env[] = {NULL};
	return env;
}

void agp_readback_synchronous(struct storage_info_t* dst)
{
}

void agp_drop_vstore(struct storage_info_t* s)
{
}

struct stream_meta agp_stream_prepare(struct storage_info_t* s,
	struct stream_meta meta, enum stream_type type)
{
	struct stream_meta mout = {0};
	return mout;
}

void agp_stream_release(struct storage_info_t* s)
{
}

void agp_stream_commit(struct storage_info_t* s)
{
}

void agp_resize_vstore(struct storage_info_t* s, size_t w, size_t h)
{
}

void agp_request_readback(struct storage_info_t* s)
{
}

struct asynch_readback_meta agp_poll_readback(struct storage_info_t* t)
{
	struct asynch_readback_meta res = {0};
	return res;
}

void agp_gl_ext_init()
{
}

void agp_empty_vstore(struct storage_info_t* vs, size_t w, size_t h)
{
}

void agp_setup_rendertarget(struct rendertarget* dst,
	struct storage_info_t* vstore, enum rendertarget_mode m)
{
	dst->store = NULL;
}

void agp_init()
{
}

void agp_drop_rendertarget(struct rendertarget* tgt)
{
}

void agp_activate_rendertarget(struct rendertarget* tgt)
{
}

void agp_rendertarget_clear()
{
}

void agp_pipeline_hint(enum pipeline_mode mode)
{
}

void agp_null_vstore(struct storage_info_t* store)
{
}

void agp_resize_rendertarget(struct rendertarget* tgt,
	size_t neww, size_t newh)
{
}

void agp_activate_vstore_multi(struct storage_info_t** backing, size_t n)
{
}

void agp_update_vstore(struct storage_info_t* s, bool copy)
{
	FLAG_DIRTY();
}

void agp_prepare_stencil()
{
}

void agp_activate_stencil()
{
}

void agp_disable_stencil()
{
}

void agp_blendstate(enum arcan_blendfunc mdoe)
{
}

void agp_draw_vobj(float x1, float y1,
	float x2, float y2, float* txcos, float* model)
{
}

void agp_submit_mesh(struct mesh_storage_t* base, enum agp_mesh_flags fl)
{
}

void agp_invalidate_mesh(struct mesh_storage_t* base)
{
}

void agp_activate_vstore(struct storage_info_t* s)
{
}

void agp_deactivate_vstore()
{
}

void agp_save_output(size_t w, size_t h, av_pixel* dst, size_t dsz)
{
}

int agp_shader_activate(arcan_shader_id shid)
{
	return shid == 1;
}

const char* agp_shader_lookuptag(arcan_shader_id id)
{
	if (id != 1)
		return NULL;

	return "tag";
}

bool agp_shader_lookupprgs(arcan_shader_id id,
	const char** vert, const char** frag)
{
	if (id != 1)
		return false;

	*vert = "";
	*frag = "";

	return true;
}

bool agp_shader_valid(arcan_shader_id id)
{
	return 1 == id;
}

agp_shader_id arcan_shader_build(const char* tag, const char* geom,
	const char* vert, const char* frag)
{
	return (vert && frag && tag && geom) ? 1 : 0;
}

void agp_shader_forceunif(const char* label,
	enum shdrutype type, void* val, bool persist)
{
}

#define TBLSIZE (1 + TIMESTAMP_D - MODELVIEW_MATR)
static char* symtbl[TBLSIZE] = {
	"modelview",
	"projection",
	"texturem",
	"obj_opacity",
	"trans_move",
	"trans_scale",
	"trans_rotate",
	"obj_input_sz",
	"obj_output_sz",
	"obj_storage_sz",
	"fract_timestamp",
	"timestamp"
};

const char* agp_shader_symtype(enum arcan_shader_envts env)
{
	return symtbl[env];
}

void agp_shader_flush()
{
}

void agp_shader_rebuild_all()
{
}

