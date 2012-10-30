#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "lastline.h"

Display* display;

struct screen_info {
	unsigned long white_color;
	unsigned long black_color;
	Window window;
	int width;
	int height;
	GC gc;
};

// Create a fullscreen window on screen "screen", and fill out "output" with
// values used by a.o. draw_window
void create_window(int screen, struct screen_info* output) {
	output->white_color = XWhitePixel(display, screen);
	output->black_color = XBlackPixel(display, screen);
	output->width = XDisplayWidth(display, screen);
	output->height = XDisplayHeight(display, screen);
	XSetWindowAttributes xswa;
	xswa.override_redirect = True;
	xswa.border_pixel = output->white_color;
	xswa.background_pixel = output->white_color;
	xswa.event_mask = StructureNotifyMask | ExposureMask;
	output->window = XCreateWindow(
		display,
		XRootWindow(display, screen),
		0,
		0,
		output->width,
		output->height,
		0,
		CopyFromParent,
		CopyFromParent,
		CopyFromParent,
		CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
		&xswa);
	XMapWindow(display, output->window);
	XGCValues gcva;
	gcva.foreground = output->black_color;
	output->gc = XCreateGC(display, output->window, GCForeground, &gcva);
}

// Grab keyboard and mouse, redirect inputs to "lock_to" and confine mouse
// pointer to that window.
void lock_input(Window lock_to) {
	int err = XGrabKeyboard(
		display,
		lock_to,
		True,
		GrabModeAsync,
		GrabModeAsync,
		CurrentTime);
	if (err != GrabSuccess) {
		fprintf(stderr, "XGrabKeyboard failed: %d\n", err);
		exit(1);
	}
	err = XGrabPointer(
		display,
		lock_to,
		True,
		0,
		GrabModeAsync,
		GrabModeAsync,
		lock_to,
		None,
		CurrentTime);
	if (err != GrabSuccess) {
		fprintf(stderr, "XGrabPointer failed: %d\n", err);
		exit(1);
	}
}

// Draw a black-on-white text "text" with font "font" in window described by "info".
void draw_window(struct screen_info* info, char* text, XFontStruct* font) {
	int x, y, direction, ascent, descent;
	XCharStruct overall;
	XTextExtents(font, text, strlen(text), &direction, &ascent, &descent, &overall);
	x = (info->width - overall.width) / 2;
	y = (info->height + ascent) / 2;

	XClearWindow(display, info->window);
	XSetFont(display, info->gc, font->fid);
	XDrawString(display, info->window, info->gc, x, y, text, strlen(text));
}

// Load font specified by "font_name", or a fixed font if that is missing.
XFontStruct* load_font(char* font_name) {
	XFontStruct* font = XLoadQueryFont(display, font_name);
	if (!font) {
		fprintf(stderr, "Warning: Can't load font %s\n", font_name);
		font = XLoadQueryFont(display, "fixed");
		if (!font) {
			fprintf(stderr, "Can't load fixed font.\n");
			exit(1);
		}
	}
	return font;
}

int main(int argc, char** argv) {
	int nolock = 0;
	char *fontname = NULL;

	while (1) {
		char c = getopt(argc, argv, "nf:");
		if (c == -1)
			break;
		switch (c) {
			case 'f':
				fontname = optarg;
				break;
			case 'n':
				nolock = 1;
				break;
			default:
				fprintf(stderr, "Usage: %s [-n] [-f fontname]\n", argv[0]);
				return 2;
		}
	}

	display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "Can't open display\n");
		return 1;
	}

	int screen_count = XScreenCount(display);
	fprintf(stderr, "Got %d screens.\n", screen_count);
	if (screen_count == 0) {
		fprintf(stderr, "No screens found.\n");
		return 1;
	}
	
	struct screen_info *screen_infos = (struct screen_info*)malloc(sizeof(struct screen_info)*screen_count);
	if (screen_infos == NULL) {
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}
	
	for(int screen = 0; screen < screen_count; screen++) {
		create_window(screen, &screen_infos[screen]);
	}

	struct lastline* ll = lastline_start(stdin);
	
	if (!nolock)
		lock_input(screen_infos[XDefaultScreen(display)].window);

	XFontStruct* font = load_font(fontname ? fontname : "fixed");

	do {
		if (XPending(display) > 0) {
			while (XPending(display) > 0) {
				XEvent ev;
				XNextEvent(display, &ev);
			}
		} else {
			usleep(200 * 1000);
		}

		char buf[512];
		lastline_get(ll, buf, sizeof(buf));
		buf[sizeof(buf)-1] = '\0';
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		for(int screen = 0; screen < screen_count; screen++) {
			draw_window(&screen_infos[screen], buf, font);
		}
	} while (!lastline_eof(ll));
	lastline_stop(ll);

	return 0;
}

