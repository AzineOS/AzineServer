#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include "../arcan_math.h"
#include "../arcan_general.h"
#include "../arcan_event.h"

#include "arcan_frameserver.h"
#include "../arcan_frameserver_shmpage.h"
#include "arcan_frameserver_decode.h"

static void interleave_pict(uint8_t* buf, uint32_t size, AVFrame* frame, uint16_t width, uint16_t height, enum PixelFormat pfmt);

static void synch_audio(arcan_ffmpeg_context* ctx)
{
	ctx->shmcont.addr->aready = true;
	frameserver_semcheck( ctx->shmcont.asem, -1);
	ctx->shmcont.addr->abufused = 0;
}

static ssize_t dbl_s16conv(int16_t* dbuf, ssize_t plane_size, int nch, double* ch_l, double* ch_r)
{
	size_t rv = 0;
	
	if (nch == 1 || (nch == 2 && ch_r == NULL))
		while (plane_size > 0){
			dbuf[rv++] = *(ch_l++) * INT16_MAX;
			plane_size -= sizeof(double);
		}
	else if (nch == 2 && ch_r)
		while (plane_size > 0){
			dbuf[rv++] = *(ch_l++) * INT16_MAX;
			dbuf[rv++] = *(ch_r++) * INT16_MAX;
			plane_size -= sizeof(double);
		}
	else;

	return rv * sizeof(int16_t);
}

static ssize_t flt_s16conv(int16_t* dbuf, ssize_t plane_size, int nch, float* ch_l, float* ch_r)
{
	size_t rv = 0;
	
	if (nch == 1 || (nch == 2 && ch_r == NULL))
		while (plane_size > 0){
			dbuf[rv++] = *(ch_l++) * INT16_MAX;
			plane_size -= sizeof(float);
		}
	else if (nch == 2 && ch_r)
		while (plane_size > 0){
			dbuf[rv++] = *(ch_l++) * INT16_MAX;
			dbuf[rv++] = *(ch_r++) * INT16_MAX;
			plane_size -= sizeof(float);
		}
	else;

	return rv * sizeof(int16_t);
}

static ssize_t s32_s16conv(int16_t* dbuf, ssize_t plane_size, int nch, int32_t* ch_l, int32_t* ch_r)
{
	size_t rv = 0;
	
	if (nch == 1 || (nch == 2 && ch_r == NULL))
		while (plane_size > 0){
			dbuf[rv++] = (float)(*ch_l++) / (float)INT32_MAX * (float)INT16_MAX;
			plane_size -= sizeof(int32_t);
		}
	else if (nch == 2 && ch_r)
		while (plane_size > 0){
			dbuf[rv++] = (float)(*ch_l++) / (float)INT32_MAX * (float)INT16_MAX;
			dbuf[rv++] = (float)(*ch_r++) / (float)INT32_MAX * (float)INT16_MAX;
			plane_size -= sizeof(int32_t);
		}
	else;

	return rv * sizeof(int16_t);
}

static ssize_t s16_s16conv(int16_t* dbuf, ssize_t plane_size, int nch, int16_t* ch_l, int16_t* ch_r)
{
	size_t rv = 0;

	if (nch == 1 || (nch == 2 && ch_r == NULL))
		while (plane_size > 0){
			dbuf[rv++] = *ch_l++;
			plane_size -= sizeof(int16_t);
		}
	else if (nch == 2 && ch_r)
		while (plane_size > 0){
			dbuf[rv++] = *ch_l++;
			dbuf[rv++] = *ch_r++;
			plane_size -= sizeof(int16_t);
		}
	else;

	return rv * sizeof(int16_t);
}

static ssize_t conv_fmt(int16_t* dbuf, size_t plane_size, int nch, int afmt, uint8_t** aplanes)
{
	ssize_t rv = -1;

		switch (afmt){
			case AV_SAMPLE_FMT_DBLP: rv = dbl_s16conv(dbuf, plane_size, nch, (double*) aplanes[0], (double*) aplanes[1]); break;
			case AV_SAMPLE_FMT_DBL : rv = dbl_s16conv(dbuf, plane_size, nch, (double*) aplanes[0], (double*) NULL);       break;
			case AV_SAMPLE_FMT_FLTP: rv = flt_s16conv(dbuf, plane_size, nch, (float* ) aplanes[0], (float* ) aplanes[1]); break;
			case AV_SAMPLE_FMT_FLT : rv = flt_s16conv(dbuf, plane_size, nch, (float* ) aplanes[0], (float* ) NULL);       break;
			case AV_SAMPLE_FMT_S32 : rv = s32_s16conv(dbuf, plane_size, nch, (int32_t*)aplanes[0], (int32_t*)NULL);       break;
			case AV_SAMPLE_FMT_S32P: rv = s32_s16conv(dbuf, plane_size, nch, (int32_t*)aplanes[0], (int32_t*)aplanes[1]); break;
			case AV_SAMPLE_FMT_S16 : rv = s16_s16conv(dbuf, plane_size, nch, (int16_t*)aplanes[0], (int16_t*)NULL);       break;
			case AV_SAMPLE_FMT_S16P: rv = s16_s16conv(dbuf, plane_size, nch, (int16_t*)aplanes[0], (int16_t*)aplanes[1]); break;
			default: break;
		}

	return rv;
}

/* decode as much into our shared audio buffer as possible,
 * when it's full OR we switch to video frames, synch the audio */
static bool decode_aframe(arcan_ffmpeg_context* ctx)
{
	static char* afr_sconv = NULL;
	static size_t afr_sconv_sz = 0;
	
	AVPacket cpkg = {
		.size = ctx->packet.size,
		.data = ctx->packet.data
	};

	int got_frame = 1;

	while (cpkg.size > 0) {
		uint32_t ofs = 0;
		avcodec_get_frame_defaults(ctx->aframe);
		int nts = avcodec_decode_audio4(ctx->acontext, ctx->aframe, &got_frame, &cpkg);

		if (nts == -1)
			return false;

		cpkg.size -= nts;
		cpkg.data += nts;

		if (got_frame){
			int plane_size;
			ssize_t ds = av_samples_get_buffer_size(&plane_size, ctx->acontext->channels, ctx->aframe->nb_samples, ctx->acontext->sample_fmt, 1);
			
/* skip packets with broken sample formats (shouldn't happen) */
			if (ds < 0)
				continue;
	
/* there's an av_convert, but unfortunately isn't exported properly or implement all that we need, maintain a scratch buffer and
 * flush / convert there */
			if (afr_sconv_sz < ds){
				free(afr_sconv);
				afr_sconv = malloc( ds );
				if (afr_sconv == NULL)
					abort();

				afr_sconv_sz = ds;
			}

/* Convert incoming sample format to something we can use. Returns the number of bytes to flush,
 * everytime our output buffer is full, synch! */
			plane_size = conv_fmt((int16_t*) afr_sconv, plane_size, ctx->aframe->channels, ctx->aframe->format, ctx->aframe->extended_data);

			if (plane_size == -1)
				continue;

			char* ofbuf = afr_sconv;
			uint32_t* abufused = &ctx->shmcont.addr->abufused;
			size_t ntc;

			do {
				ntc = plane_size > SHMPAGE_AUDIOBUF_SIZE - *abufused ?
					SHMPAGE_AUDIOBUF_SIZE - *abufused : plane_size;

				memcpy(&ctx->audp[*abufused], ofbuf, ntc);
				*abufused += ntc;
				plane_size -= ntc;
				ofbuf += ntc;

				if (*abufused == SHMPAGE_AUDIOBUF_SIZE)
					synch_audio(ctx);

			} while (plane_size);
	
		}
	}

	return true;
}

/* note, if shared + vbufofs is aligned to ffmpeg standards, we could sws_scale directly to it */
static bool decode_vframe(arcan_ffmpeg_context* ctx)
{
	int complete_frame = 0;

	avcodec_decode_video2(ctx->vcontext, ctx->pframe, &complete_frame, &(ctx->packet));
	if (complete_frame) {
		if (ctx->extcc)
			interleave_pict(ctx->video_buf, ctx->c_video_buf, ctx->pframe, ctx->vcontext->width, ctx->vcontext->height, ctx->vcontext->pix_fmt);
		else {
			uint8_t* dstpl[4] = {NULL, NULL, NULL, NULL};
			int dststr[4] = {0, 0, 0, 0};
			dststr[0] =ctx->width * ctx->bpp;
			dstpl[0] = ctx->video_buf;
			if (!ctx->ccontext) {
				ctx->ccontext = sws_getContext(ctx->vcontext->width, ctx->vcontext->height, ctx->vcontext->pix_fmt,
					ctx->vcontext->width, ctx->vcontext->height, PIX_FMT_BGR32, SWS_FAST_BILINEAR, NULL, NULL, NULL);
			}
			sws_scale(ctx->ccontext, (const uint8_t* const*) ctx->pframe->data, ctx->pframe->linesize, 0, ctx->vcontext->height, dstpl, dststr);
		}

/* SHM-CHG-PT */
		ctx->shmcont.addr->vpts = (ctx->packet.dts != AV_NOPTS_VALUE ? ctx->packet.dts : 0) * av_q2d(ctx->vstream->time_base) * 1000.0;
		memcpy(ctx->vidp, ctx->video_buf, ctx->c_video_buf);

/* parent will check vready, then set to false and post */
		ctx->shmcont.addr->vready = true;
		frameserver_semcheck( ctx->shmcont.vsem, -1);
	}

	return true;
}

bool ffmpeg_decode(arcan_ffmpeg_context* ctx)
{
	bool fstatus = true;
	/* Main Decoding sequence */
	while (fstatus &&
		av_read_frame(ctx->fcontext, &ctx->packet) >= 0) {

/* got a videoframe */
		if (ctx->packet.stream_index == ctx->vid){
			fstatus = decode_vframe(ctx);
		}

/* or audioframe, not that currently both audio and video synch separately */
		else if (ctx->packet.stream_index == ctx->aid){
			fstatus = decode_aframe(ctx);
			if (ctx->shmcont.addr->abufused)
				synch_audio(ctx);
		}

		av_free_packet(&ctx->packet);
	}

	return fstatus;
}

void ffmpeg_cleanup(arcan_ffmpeg_context* ctx)
{
	if (ctx){
		free(ctx->video_buf);
		av_free(ctx->pframe);
		av_free(ctx->aframe);
	}
}


static arcan_ffmpeg_context* ffmpeg_preload(const char* fname, AVInputFormat* iformat)
{
	arcan_ffmpeg_context* dst = (arcan_ffmpeg_context*) calloc(sizeof(arcan_ffmpeg_context), 1);

	int errc = avformat_open_input(&dst->fcontext, fname, iformat, NULL);
	if (0 != errc || !dst->fcontext)
		return NULL;

	errc = avformat_find_stream_info(dst->fcontext, NULL);

	if (! (errc >= 0) )
		return NULL;

/* locate streams and codecs */
	int vid,aid;

	vid = aid = dst->vid = dst->aid = -1;

/* scan through all video streams, grab the first one,
 * find a decoder for it, extract resolution etc. */
	for (int i = 0; i < dst->fcontext->nb_streams && (vid == -1 || aid == -1); i++)
		if (dst->fcontext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && vid == -1) {
			dst->vid = vid = i;
			dst->vcontext  = dst->fcontext->streams[vid]->codec;
			dst->vcodec    = avcodec_find_decoder(dst->vcontext->codec_id);
			dst->vstream   = dst->fcontext->streams[vid];
			avcodec_open2(dst->vcontext, dst->vcodec, NULL);

			if (dst->vcontext) {
				dst->width  = dst->vcontext->width;
				dst->bpp    = 4;
				dst->height = dst->vcontext->height;
/* dts + dimensions */
				dst->c_video_buf = (dst->vcontext->width * dst->vcontext->height * dst->bpp);
				dst->video_buf   = (uint8_t*) av_malloc(dst->c_video_buf);
			}
		}
		else if (dst->fcontext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && aid == -1) {
			dst->aid      = aid = i;
			dst->astream  = dst->fcontext->streams[aid];
			dst->acontext = dst->fcontext->streams[aid]->codec;
			dst->acodec   = avcodec_find_decoder(dst->acontext->codec_id);
			avcodec_open(dst->acontext, dst->acodec);

/* weak assumption that we always have 2 channels, but the frameserver interface is this simplistic atm. */
			dst->channels   = 2;
			dst->samplerate = dst->acontext->sample_rate;
		}

	dst->pframe = avcodec_alloc_frame();
	dst->aframe = avcodec_alloc_frame();

	return dst;
}

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
static const char* probe_vidcap(signed prefind, AVInputFormat** dst)
{
	char arg[16];
	struct stat fs;

	for (int i = prefind; i >= 0; i--){
		snprintf(arg, sizeof(arg)-1, "/dev/video%d", i);
		if (stat(arg, &fs) == 0){
			*dst = av_find_input_format("video4linux2");
			return strdup(arg);
		}
	}

	*dst = NULL;
	return NULL;
}
#else
/* for windows, the suggested approach nowadays isn't vfwcap but dshow,
 * the problem is that we both need to be able to probe and look for a video device
 * and this enumeration then requires bidirectional communication with the parent */
static const char* probe_vidcap(signed prefind, AVInputFormat** dst)
{
	char arg[16];
	
	snprintf(arg, sizeof(arg)-1, "%d", prefind);
	*dst = av_find_input_format("vfwcap");
	
	return strdup(arg);
}
#endif

static arcan_ffmpeg_context* ffmpeg_vidcap(unsigned ind)
{
	AVInputFormat* format;
	avdevice_register_all();

	const char* fname = probe_vidcap(ind, &format);
	if (!fname || !format)
		return NULL;

	return ffmpeg_preload(fname, format);
}

static void interleave_pict(uint8_t* buf, uint32_t size, AVFrame* frame, uint16_t width, uint16_t height, enum PixelFormat pfmt)
{
	bool planar = (pfmt == PIX_FMT_YUV420P || pfmt == PIX_FMT_YUV422P || pfmt == PIX_FMT_YUV444P || pfmt == PIX_FMT_YUV411P);

	if (planar) { /* need to expand into an interleaved format */
		/* av_malloc (buf) guarantees alignment */
		uint32_t* dst = (uint32_t*) buf;

/* atm. just assuming plane1 = Y, plane2 = U, plane 3 = V and that correct linewidth / height is present */
		for (int row = 0; row < height; row++)
			for (int col = 0; col < width; col++) {
				uint8_t y = frame->data[0][row * frame->linesize[0] + col];
				uint8_t u = frame->data[1][(row/2) * frame->linesize[1] + (col / 2)];
				uint8_t v = frame->data[2][(row/2) * frame->linesize[2] + (col / 2)];

				*dst = 0xff | y << 24 | u << 16 | v << 8;
				dst++;
			}
	}
}

void arcan_frameserver_ffmpeg_run(const char* resource, const char* keyfile)
{
	arcan_ffmpeg_context* vidctx;
	struct arg_arr* args = arg_unpack(resource);
	struct frameserver_shmcont shms = frameserver_getshm(keyfile, true);
	
	av_register_all();

	do {
/* vidcap:ind where ind is a hint to the probing function */
		if (strncmp(resource, "vidcap", 6) == 0){
			unsigned devind = 0;
			char* ofs = strchr(resource, ':');
			if (ofs && (*++ofs)){
				signed ind = strtol(ofs, NULL, 10);
				if (ind < 255)
					devind = ind;
			}

			vidctx = ffmpeg_vidcap(devind);
		} else {
			vidctx = ffmpeg_preload(resource, NULL);
		}

		if (!vidctx)
			break;

/* initialize both semaphores to 0 => render frame (wait for parent to signal) => regain lock */
		vidctx->shmcont = shms;
		frameserver_semcheck(vidctx->shmcont.asem, -1);
		frameserver_semcheck(vidctx->shmcont.vsem, -1);

		if (!frameserver_shmpage_resize(&shms, vidctx->width, vidctx->height, vidctx->bpp, vidctx->channels, vidctx->samplerate))
		arcan_fatal("arcan_frameserver_ffmpeg_run() -- setup of vid(%d x %d @ %d) aud(%d,%d) failed \n",
			vidctx->width,
			vidctx->height,
			vidctx->bpp,
			vidctx->channels,
			vidctx->samplerate
		);

		frameserver_shmpage_calcofs(shms.addr, &(vidctx->vidp), &(vidctx->audp) );

	} while (ffmpeg_decode(vidctx) && vidctx->shmcont.addr->loop);
}
