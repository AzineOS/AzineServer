/* Arcan-fe, scriptable front-end engine
 *
 * Arcan-fe is the legal property of its developers, please refer
 * to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,USA.
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif

#include <math.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <SDL.h>
#include <sqlite3.h>

#include "getopt.h"
#include "arcan_math.h"
#include "arcan_general.h"
#include "arcan_event.h"
#include "arcan_audio.h"
#include "arcan_video.h"
#include "arcan_framequeue.h"
#include "arcan_frameserver_backend.h"
#include "arcan_lua.h"
#include "arcan_led.h"
#include "arcan_db.h"
#include "arcan_util.h"
#include "arcan_videoint.h"

arcan_dbh* dbhandle = NULL;

/* globals, hackishly used in other places */
extern char* arcan_themename;
extern char* arcan_themepath;
extern char* arcan_resourcepath;
extern char* arcan_libpath;
extern char* arcan_binpath;

bool stderr_redirected = false;
bool stdout_redirected = false;

static const struct option longopts[] = {
	{ "help",         no_argument,       NULL, '?'},
	{ "width",        required_argument, NULL, 'w'},
	{ "height",       required_argument, NULL, 'h'},
	{ "winx",         required_argument, NULL, 'x'},
	{ "winy",         required_argument, NULL, 'y'},
	{ "fullscreen",   no_argument,       NULL, 'f'},
	{ "windowed",     no_argument,       NULL, 's'},
	{ "debug",        no_argument,       NULL, 'g'},
	{ "rpath",        required_argument, NULL, 'p'},
	{ "themepath",    required_argument, NULL, 't'},
	{ "hijacklib",    required_argument, NULL, 'l'},
	{ "frameserver",  required_argument, NULL, 'o'},
	{ "conservative", no_argument,       NULL, 'm'},
	{ "database",     required_argument, NULL, 'd'},
	{ "scalemode",    required_argument, NULL, 'r'},
	{ "multisamples", required_argument, NULL, 'a'},
	{ "interactive",  no_argument,       NULL, 'i'},
	{ "nosound",      no_argument,       NULL, 'S'},
	{ "novsync",      no_argument,       NULL, 'v'},
/* no points guessing which platform forcing this .. */
	{ "stdout",       required_argument, NULL, '1'},
	{ "stderr",       required_argument, NULL, '2'},
	{ "nowait",       no_argument,       NULL, 'V'},
	{ "vsync-falign", required_argument, NULL, 'F'},
	{ "monitor",      required_argument, NULL, 'M'},
	{ "monitor-out",  required_argument, NULL, 'O'},
	{ NULL,           no_argument,       NULL,  0 }
};

void usage()
{
printf("usage:\narcan [-whxyfmstptodgavSrMO] [theme] [themearguments]\n"
"-w\t--width       \tdesired width (default: 640)\n"
"-h\t--height      \tdesired height (default: 480)\n"
"-x\t--winx        \tforce window x position (default: don't set)\n"
"-y\t--winy        \tforce window y position (default: don't set)\n"
"-f\t--fullscreen  \ttoggle fullscreen mode ON (default: off)\n"
"-m\t--conservative\ttoggle conservative memory management (default: off)\n"
#ifndef _WIN32
"-M\t--monitor     \tsplit open a debug arcan monitoring session\n"
"-O\t--monitor-out \tLOG or script.lua (resourcepath/scripts/monitors)\n"
#endif
"-s\t--windowed    \ttoggle borderless window mode\n"
"-p\t--rpath       \tchange path for resources (default: autodetect)\n"
"-t\t--themepath   \tchange path for themes (default: autodetect)\n"
"-o\t--frameserver \tforce frameserver (default: autodetect)\n"
"-l\t--hijacklib   \tforce library for internal launch (default: autodetect)\n"
"-d\t--database    \tsqlite database (default: arcandb.sqlite)\n"
"-g\t--debug       \ttoggle debug output (stacktraces, events,"
" coredumps, etc.)\n"
"-a\t--multisamples\tset number of multisamples (default 4, disable 0)\n"
"-v\t--novsync     \tdisable synch to video refresh (default, vsync on)\n"
"-V\t--nowait      \tdisable sleeping between superflous frames\n"
"-F\t--vsync-falign\t (0..1, default: 0.6) balance processing vs. CPU usage\n"
"-S\t--nosound     \tdisable audio output\n"
"-r\t--scalemode   \tset texture mode:\n\t"
"%i(rectangle sized textures, default),\n\t"
"%i(scale to power of two)\n\t", ARCAN_VIMAGE_NOPOW2, ARCAN_VIMAGE_SCALEPOW2);
}

int main(int argc, char* argv[])
{
	char* script_override = NULL;
	bool windowed     = false;
	bool fullscreen   = false;
	bool conservative = false;
	bool nosound      = false;
	bool interactive  = false;
	bool waitsleep    = true;

	unsigned char debuglevel = 0;

	int scalemode = ARCAN_VIMAGE_NOPOW2;
	int width     = 640;
	int height    = 480;
	int winx      = -1;
	int winy      = -1;
	float vfalign     = 0.6;
	
/* only used when monitor mode is activated, where we want some 
 * of the global paths etc. accessible, but not *all* of them */
	FILE* monitor_outf    = NULL;
	int monitor_outfd     = -1;
	int monitor           = 0;
	bool monitor_parent   = true;
	char* monitor_arg     = "LOG";

	FILE* errc;
	char* dbfname = "arcandb.sqlite";
	char ch;
	
	srand( time(0) );
/* VIDs all have a randomized base to provoke crashes in poorly written scripts,
 * only -g will make their base and sequence repeatable */

	while ((ch = getopt_long(argc, argv, "w:h:x:y:?fvVmisp:"
		"t:M:O:o:l:a:d:F:1:2:gr:S", longopts, NULL)) != -1){
	switch (ch) {
	case '?' :
		usage();
		exit(1);
	break;
	case 'w' : width = strtol(optarg, NULL, 10); break;
	case 'h' : height = strtol(optarg, NULL, 10); break;
	case 'm' : conservative = true; break;
	case 'x' : winx = strtol(optarg, NULL, 10); break;
	case 'y' : winy = strtol(optarg, NULL, 10); break;
	case 'F' : vfalign = strtof(optarg, NULL); break;
	case 'f' : fullscreen = true; break;
	case 's' : windowed = true; break;
	case 'l' : arcan_libpath = strdup(optarg); break;
	case 'd' : dbfname = strdup(optarg); break;
	case 'S' : nosound = true; break;
	case 'a' : arcan_video_display.msasamples = strtol(optarg, NULL, 10); break;
	case 'v' : arcan_video_display.vsync = false; break;
	case 'V' : waitsleep = false; break;
	case 'p' : arcan_resourcepath = strdup(optarg); break;
#ifndef _WIN32
	case 'i' : fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); interactive = true; break;
	case 'M' : monitor = abs( strtol(optarg, NULL, 10) ); break;
	case 'O' : monitor_arg = strdup( optarg ); break; 
#endif
	case 't' : arcan_themepath = strdup(optarg); break;
	case 'o' : arcan_binpath = strdup(optarg); break;
	case 'g' :
		debuglevel++;
		srand(0xdeadbeef);
		break;
	case 'r' :
		scalemode = strtol(optarg, NULL, 10);
		printf("scalemode: %d\n", scalemode);
		if (scalemode != ARCAN_VIMAGE_NOPOW2 && scalemode != 
			ARCAN_VIMAGE_SCALEPOW2 && scalemode){
			arcan_warning("Warning: main(), -r, invalid scalemode. Ignoring.\n");
			scalemode = ARCAN_VIMAGE_SCALEPOW2;
		}
	break;
	case '1' :
		stdout_redirected = true;
		errc = freopen(optarg, "a", stdout);
		break;
	case '2' :
		stderr_redirected = true;
		errc = freopen(optarg, "a", stderr);
		break;

	default:
		break;
	}
	}

	if (arcan_setpaths() == false)
		goto error;

	if (check_theme(*(argv + optind)))
		arcan_themename = *(argv + optind);
	else if (check_theme("welcome"))
		arcan_themename = "welcome";
	else
		arcan_fatal("No theme found.\n");

	if (strcmp(arcan_themename, "arcan") == 0){
		arcan_fatal("Theme name 'arcan' is reserved\n");
	}

	if (vfalign > 1.0 || vfalign < 0.0){
		arcan_warning("Argument Error (-F, --vsync-falign): "
		"bad range specified (%f), reverting to default (0.6)\n", vfalign);
		vfalign = 0.6;
	}

#ifndef _WIN32
/* pipe to file, socket or launch script based on monitor output,
 * format will be LUA tables with the exception of each cell ending with
 * #ENDSAMPLE . The block will be sampled, parsed and should return a table
 * pushed through the sample() function */
	if (monitor > 0){
		if (strcmp(monitor_arg, "LOG:") == 0){
			arcan_warning("monitor log not yet added\n");
		}
		else {
			int pair[2];
			char scriptfnbuf[256] = {0};
			snprintf(scriptfnbuf, 255, "scripts/monitor/%s", monitor_arg);
			char* tmpscript = arcan_find_resource(scriptfnbuf, ARCAN_RESOURCE_SHARED);
			if (tmpscript == NULL)
				arcan_fatal("Missing monitor script: %s\n", scriptfnbuf);
			
			pid_t p1;

			pipe(pair);
			if ( (p1 = fork()) == 0){
				close(pair[1]);
				monitor_parent = false;
				if (fork() != 0)
					exit(0); 

				monitor_outfd   = pair[0]; 
				script_override = tmpscript;
				arcan_themename = strdup(monitor_arg);
				char* tmp = strchr(arcan_themename, '.');
				if (tmp)
					*tmp = '\0';
			} else {
				int status;
/* close these as they occlude data from the monitor session
 * ideally, the lua warning/etc. should be mapped to go as messages
 * to the monitoring session, as should a perror/atexit handler */
				close(pair[0]);
				fclose(stdout);
				fclose(stderr);
				monitor_parent = true;
				monitor_outf = fdopen(pair[1], "w");
				waitpid(p1, &status, 0);
			 	signal(SIGPIPE, SIG_IGN);	
			}
		}
		
		fullscreen = false;
	}
#endif
	
/* also used as restart point for switiching themes */
themeswitch:
	SDL_Init(SDL_INIT_VIDEO);
	char* dbname = arcan_expand_resource(dbfname, true);

/*
 * try to open the specified database,
 * if that fails, warn, try to create an empty 
 * database and if that fails, give up. 
 */
	dbhandle = arcan_db_open(dbname, arcan_themename);
	if (!dbhandle) {
		arcan_warning("Couldn't open database (requested: %s => %s),"
			"trying to create a new one.\n", dbfname, dbname);
		FILE* fpek = fopen(dbname, "a");
		if (fpek){
			fclose(fpek);
			dbhandle = arcan_db_open(dbname, arcan_themename);
		}

		if (!dbhandle)
			goto error;
	}
	free(dbname);

	const SDL_VideoInfo* vi = SDL_GetVideoInfo();
	if (!vi){
		arcan_fatal("SDL_GetVideoInfo() failed, broken display subsystem.");
		goto error;
	}

	if (winx != -1 || winy != -1){
		char windbuf[64] = {0};
		snprintf(windbuf, 63, "SDL_VIDEO_WINDOW_POS=%i,%i", winx >= 0 ?
			winx : 0, winy >= 0 ? winy : 0);
		putenv(strdup(windbuf));
	}

	if (width == 0 || height == 0) {
		width = vi->current_w;
		height = vi->current_h;
	}

	arcan_warning("Notice: [SDL] Video Info: %i, %i, hardware acceleration: %s, "
		"window manager: %s, scalemode: %i, VSYNC: %i, MSAA: %i\n",
			vi->current_w, vi->current_h, vi->hw_available ? "yes" : "no", 
			vi->wm_available ? "yes" : "no", scalemode, arcan_video_display.vsync,
			arcan_video_display.msasamples);
	arcan_video_default_scalemode(scalemode);

	if (windowed) {
		fullscreen = false;
	}

/* grab video, (necessary) */
	if (arcan_video_init(width, height, 32, fullscreen, windowed, conservative)
		!= ARCAN_OK) {
		arcan_fatal("Error; Couldn't initialize video system,"
			"try other windowing options (-f, -w, ...)\n");
	}
	
	errno = 0;
/* grab audio, (possible to live without) */
	if (ARCAN_OK != arcan_audio_setup(nosound))
		arcan_warning("Warning: No audio devices could be found.\n");

/* setup device polling, cleanup, ... */
	arcan_evctx* def = arcan_event_defaultctx();
	def->interactive = interactive;
	arcan_event_init( def );
	arcan_led_init();

/*
 * MINGW implements putenv, so use this to set
 * the system subpath path (BIOS, ..) 
 */
	if (getenv("ARCAN_SYSTEMPATH") == NULL){
		size_t len = strlen(arcan_resourcepath) + strlen("/games/system") + 15;
		char* const syspath = malloc(len);
		sprintf(syspath, "ARCAN_SYSPATH=%s/games/system", arcan_resourcepath);
		arcan_warning("Notice: Using default systempath (%s)\n", syspath);
		putenv(syspath);
	} else
		arcan_warning("Notice: Using systempath from environment (%s)\n", 
			getenv("ARCAN_SYSTEMPATH"));

	if (getenv("ARCAN_FRAMESERVER_LOGDIR") == NULL){
		size_t len = strlen(arcan_resourcepath) + strlen("/logs") + 27;
		char* const logpath = malloc(len);
		sprintf(logpath, "ARCAN_FRAMESERVER_LOGDIR=%s/logs", arcan_resourcepath);
		putenv(logpath);
	}

/* export what we know and load theme */
	lua_State* luactx = luaL_newstate();
	luaL_openlibs(luactx);

#ifndef _WIN32
	struct rlimit coresize = {0};

/* debuglevel 0, no coredumps etc.
 * debuglevel 1, maximum 1M coredump, should prioritize stack etc.
 * would so want to be able to hint to mmap which pages that should be avoided.
 * debuglevel > 1, dump everything */
	if (debuglevel == 0);
	else if (debuglevel == 1) coresize.rlim_max = 10 * 1024 * 1024;
	else coresize.rlim_max = RLIM_INFINITY;

	coresize.rlim_cur = coresize.rlim_max;
	setrlimit(RLIMIT_CORE, &coresize);
#endif

/* this one also sandboxes os/io functions (just by setting to nil) */
	arcan_lua_exposefuncs(luactx, debuglevel);
	arcan_lua_pushglobalconsts(luactx);

	if (argc > optind)
		arcan_lua_pushargv(luactx, argv + optind + 1);

	int err_func = 0;

	if (script_override && luaL_dofile(luactx, script_override) == 1){
		arcan_fatal("Fatal: main(), Error loading"
			"overridescript: (%s)\n", script_override);
		goto error;
	} 
	else if (!script_override) {
		char* themescr = (char*) malloc(strlen(arcan_themename) + 5);
		sprintf(themescr, "%s.lua", arcan_themename);
		char* fn = arcan_find_resource(themescr, ARCAN_RESOURCE_THEME);

		if ( luaL_dofile(luactx, fn) == 1 ){
			const char* msg = lua_tostring(luactx, -1);
			arcan_fatal("Fatal: main(), Error loading theme script"
				"(%s) : (%s)\n", themescr, msg);
			goto error;
		}
			
		free(fn);
		free(themescr);
	}

/* entry point follows the name of the theme,
 * hand over execution and begin event loop */
	arcan_lua_callvoidfun(luactx, arcan_themename);
	arcan_lua_callvoidfun(luactx, "show");

	bool done = false;
	float lastfrag = 0.0f;
	long long int lastflip = arcan_timemillis();
	int monitor_counter = monitor;
		
	while (!done) {
		arcan_event* ev;
		arcan_errc evstat;

/* pollfeed can actually populate event-loops, assuming we don't exceed a 
 * compile- time threshold */
		arcan_video_pollfeed();

/* NOTE: might be better if this terminates if we're closing in on a 
 * deadline as to not be saturated with an onslaught of I/O events. */
		while ((ev = arcan_event_poll(arcan_event_defaultctx(), 
			&evstat)) && evstat == ARCAN_OK){

/*
 * these events can typically be determined in video_tick(),
 * however there are so many hierarchical dependencies 
 * (linked objs, instances, ...)
 * that a full delete is not really safe there (e.g. event -> callback -> 
 */
			switch (ev->category){
			case EVENT_VIDEO:
				if (ev->kind == EVENT_VIDEO_EXPIRE)
					arcan_video_deleteobject(ev->data.video.source);
				else if (ev->kind == EVENT_VIDEO_ASYNCHIMAGE_LOADED ||
					ev->kind == EVENT_VIDEO_ASYNCHIMAGE_LOAD_FAILED)
				arcan_video_pushasynch(ev->data.video.source);
			break;

			case EVENT_SYSTEM:
/* note the LUA shutdown() call actually emits this event */
				if (ev->kind == EVENT_SYSTEM_EXIT)
					done = true;
				else if (ev->kind == EVENT_SYSTEM_SWITCHTHEME){
					lua_close(luactx);
					arcan_video_shutdown();
					arcan_audio_teardown();
					arcan_event_deinit(arcan_event_defaultctx());
					arcan_led_cleanup();
					arcan_db_close(dbhandle);

					arcan_themename = strdup(ev->data.system.data.message);
					goto themeswitch;
				}
				else if (ev->kind == EVENT_SYSTEM_LAUNCH_EXTERNAL){}
				else if (ev->kind == EVENT_SYSTEM_CLEANUP_EXTERNAL){}
/* don't want to propagate the others */
				else 
					continue;
			break;
			}

			arcan_lua_pushevent(luactx, ev);
		}

		unsigned nticks;
		float frag = arcan_event_process(arcan_event_defaultctx(), &nticks);

		if (debuglevel == 4)
			arcan_warning("main() event_process (%d, %f)\n", nticks, frag);

/* priority is always in maintaining logical clock and event processing */
		if (nticks > 0){
			arcan_video_tick(nticks);
			arcan_audio_tick(nticks);
			lastfrag = 0.0;
				
			if (monitor && monitor_parent){
				if (--monitor_counter == 0){
					monitor_counter = monitor;
					arcan_lua_statesnap(monitor_outf);
				}
			} 
		}

/* this is internally buffering and non-blocking, hence the fd use compared
 * to arcan_lua_statesnap above */
		if (monitor && !monitor_parent)
			arcan_lua_stategrab(luactx, "sample", monitor_outfd);
	
/*
 * difficult decision, should we flip or not?
 * a full- redraw can be costly, so should only really be done if 
 * enough things have changed or if we're closing in on the next 
 * deadline for the unknown video clock, this also depends on if 
 * the user favors energy saving (waitsleep) or responsiveness. 
 */
		const int min_respthresh = 9;

/* only render if there's enough relevant changes */
		if (nticks > 0 || frag - lastfrag > INTERP_MINSTEP){

/* separate between cheap (possibly vsync off or triple buffering) 
 * flip cost and expensive (vsync on) */
			if (arcan_video_display.vsync_timing < 8.0){
				arcan_video_refresh(frag, true);
				int delta = arcan_timemillis() - lastflip;
				lastflip += delta;
	
				if (waitsleep && delta < min_respthresh)
					arcan_timesleep(min_respthresh - delta);
			}
			else {
				int delta = arcan_timemillis() - lastflip;
				if (delta >= (float)arcan_video_display.vsync_timing * vfalign){
					arcan_video_refresh(frag, true);
					lastflip += delta;
				}
			}
		}

		arcan_audio_refresh();
	}

	arcan_lua_callvoidfun(luactx, "shutdown");
	arcan_led_cleanup();
	arcan_video_shutdown();

error:
	SDL_Quit();
	exit(1);

	return 0;
}
