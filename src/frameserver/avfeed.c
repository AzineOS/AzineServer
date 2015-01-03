/*
 * No copyright claimed, Public Domain
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include <arcan_shmif.h>
#include "frameserver.h"

static void update_frame(struct arcan_shmif_cont* shms, uint32_t val)
{
	uint32_t* cptr = shms->vidp;

	int np = shms->addr->w * shms->addr->h;
	for (int i = 0; i < np; i++)
		*cptr++ = val;

	arcan_shmif_signal(shms, SHMIF_SIGVID);
}

struct seginf {
	struct arcan_event ev;
	struct arcan_shmif_cont shms;
	uint32_t* vidp;
	uint16_t* audp;
};

static void* segthread(void* arg)
{
	struct seginf* seg = (struct seginf*) arg;
	uint8_t green = 0;

	arcan_shmif_resize(&seg->shms, 320, 200);

	while(seg->shms.addr->dms){
		arcan_event ev;
		printf("waiting for events\n");
		while(1 == arcan_shmif_wait(&seg->shms, &ev)){
			printf("got event\n");
			if (ev.category == EVENT_TARGET &&
				ev.tgt.kind == TARGET_COMMAND_EXIT){
				printf("parent requested termination\n");
				update_frame(&seg->shms, 0xffffff00);
				return NULL;
			}
			update_frame(&seg->shms, 0xff000000 | (green++ << 8));
		}
	}

	return NULL;
}

static void mapseg(int evfd, const char* key)
{
	struct seginf* newseg = malloc(sizeof(struct seginf));
	newseg->shms = arcan_shmif_acquire(
		key, SEGID_GAME, SHMIF_ACQUIRE_FATALFAIL);

	pthread_t thr;
	pthread_create(&thr, NULL, segthread, newseg);
}

static void dump_help()
{
	fprintf(stdout, "the avfeed- frameserver is primarily intended"
		" for testing and prototyping purposes and is not particularly"
		" useful on its own.\n");
}

/*
 * Quick skeleton to map up a audio/video/input
 * source to an arcan frameserver along with some helpers.
 */
int arcan_frameserver_avfeed_run(
	struct arcan_shmif_cont* con,
	struct arg_arr* args)
{
	struct arcan_shmif_cont shms = *con;
	if (!con){
		dump_help();
		return EXIT_FAILURE;
	}

	if (!arcan_shmif_resize(&shms, 320, 200)){
		LOG("arcan_frameserver(decode) shmpage setup, resize failed\n");
		return EXIT_FAILURE;
	}

	update_frame(&shms, 0xffffffff);

/*
 * request a new segment
 */
	arcan_event ev = {
		.category = EVENT_EXTERNAL,
		.ext.kind = EVENT_EXTERNAL_SEGREQ
	};

	arcan_shmif_enqueue(&shms, &ev);

	int lastfd;

	while(1){
		while (1 == arcan_shmif_wait(&shms, &ev)){
			if (ev.category == EVENT_TARGET){
				if (ev.tgt.kind == TARGET_COMMAND_FDTRANSFER){
					lastfd = arcan_fetchhandle(shms.dpipe);
					printf("got handle (for new event transfer)\n");
				}
			}
			if (ev.tgt.kind == TARGET_COMMAND_NEWSEGMENT){
				printf("new segment ready, key: %s\n", ev.tgt.message);
				mapseg(lastfd, ev.tgt.message);
			}
			if (ev.tgt.kind == TARGET_COMMAND_EXIT){
				printf("parent requested termination, leaving.\n");
				return EXIT_SUCCESS;
			}
			else {
				static int red;
				update_frame(&shms, 0xff000000 | red++);
			}
/*
 *	event dispatch loop, look at shmpage interop,
 *	valid categories here should (at least)
 *	be EVENT_SYSTEM, EVENT_IO, EVENT_TARGET
 */
		}
	}
	return EXIT_FAILURE;
}
