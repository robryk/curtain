#include "lastline.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

static void* reader_thread(void* param) {
	struct lastline* ll = (struct lastline*)param;
	char buf[512];
	while (1) {
		if (fgets(buf, sizeof(buf), ll->file) == NULL) {
			pthread_mutex_lock(&ll->mutex);
			ll->eof = 1;
			if (ll->stopping)
				free(ll);
			pthread_mutex_unlock(&ll->mutex);
			return NULL;
		}
		buf[sizeof(buf)-1] = '\0';
		pthread_mutex_lock(&ll->mutex);
		if (ll->stopping) {
			free(ll);
			return NULL;
		}
		strcpy(ll->buf, buf);
		pthread_mutex_unlock(&ll->mutex);
	}
}

struct lastline* lastline_start(FILE* file) {
	struct lastline* ll = (struct lastline*)malloc(sizeof(struct lastline));
	if (ll == NULL)
		goto exit;
	pthread_mutex_init(&ll->mutex, NULL);
	ll->stopping = 0;
	ll->eof = 0;
	ll->buf[0] = '\0';
	ll->file = file;
	pthread_t reader;
	if (pthread_create(&reader, NULL, &reader_thread, ll) != 0)
		goto free;
	pthread_detach(reader);
	return ll;

free:
	free(ll);
exit:
	return NULL;
}

void lastline_stop(struct lastline* ll) {
	pthread_mutex_lock(&ll->mutex);
	if (ll->eof) {
		free(ll);
		return;
	}
	ll->stopping = 1;
	pthread_mutex_unlock(&ll->mutex);
}

void lastline_get(struct lastline* ll, char* buf, int n) {
	pthread_mutex_lock(&ll->mutex);
	strncpy(buf, ll->buf, n);
	pthread_mutex_unlock(&ll->mutex);
}

int lastline_eof(struct lastline* ll) {
	pthread_mutex_lock(&ll->mutex);
	int r = ll->eof;
	pthread_mutex_unlock(&ll->mutex);
	return r;
}

