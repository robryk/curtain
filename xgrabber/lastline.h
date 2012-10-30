#ifndef __GUARD_LASTLINE_H
#define __GUARD_LASTLINE_H

#include <pthread.h>
#include <stdio.h>

struct lastline {
	pthread_mutex_t mutex;
	int stopping; // guarded by mutex
	int eof; // guarded by mutex
	char buf[512]; // guarded by mutex
	FILE* file; // accessed only by the associated reader_thread
};
	
struct lastline* lastline_start(FILE* file);
void lastline_stop(struct lastline* ll);
void lastline_get(struct lastline* ll, char* buf, int n);
int lastline_eof(struct lastline* ll);

#endif // __GUARD_LASTLINE_H
