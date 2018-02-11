/* See LICENSE file for copyright and license details */
#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "channels.h"
#include "log.h"
#include "strbuf.h"
#include "sys.h"

int channels_add(struct channels *ch, struct strbuf const *name);
int channels_remove(struct channels *ch, struct strbuf const *name);
void channels_log(struct strbuf const *name, struct strbuf const *msg);
static int channels_open_fifo(struct strbuf const *path);

/**
 * Opens a fifo file named "in" at the end of the given path.
 *
 * Return: The open fifo file descriptor. -1 if an error opening it occured.
 */
static int
channels_open_fifo(struct strbuf const *path)
{
	int fd = -1;
	char tmp[path.len + sizeof("/in")];

	memcpy(tmp, path.mem, path.len);
	strcpy(tmp + path.len, "/in");
	remove(tmp);
	if (0 > (fd = mkfifo(tmp, S_IRWXU))) {
		LOGERROR("Input fifo creation at \"%s\" failed.\n", tmp);
		return -1;
	}
	LOGINFO("Input fifo created at \"%s\" successfully.\n", tmp);
	return open(tmp, O_RDWR | O_NONBLOCK, 0);
}

/**
 * Add a channel to a Channels struct, resize it if necessary.
 */
int
channels_add(struct channels *ch, struct strbuf const *path)
{
	size_t diff;
	size_t nextsize;
	struct strbuf *sp;
	void *region;

	if (ch->len >= ch->size) {
		nextsize = npow2(ch->size + 1);
		diff = nextsize - ch->size;
		region = realloc(ch->fds,
				sizeof(*ch->fds) * nextsize
				+ sizeof(*ch->names) * nextsize);
		if (!region)
				return -1;
		ch->fds = region;
		ch->names = (struct strbuf *)(ch->fds + nextsize);
		// Zero initialize the added memory.
		memset(&ch->fds[ch->size], 0, sizeof(*ch->fds) * diff);
		memset(&ch->names[ch->size], 0, sizeof(*ch->names) * diff);
		ch->size = nextsize;
	}
	sb_cpy_sb(&ch->paths[ch->len], &name);
	if (0 > mkdirpath(&ch->paths[ch->len])) {
		LOGERROR("Directory creation for path \"%s\" failed.\n",
				ch->paths[ch->len].mem);
		return -1;
	}
	if (0 > (ch->fds[ch->len] = channels_open_fifo(&ch->paths[ch->len])))
		return -1;
	ch->len += 1;
	return 0;
}

/**
 * Remove a channel from a Channels struct.
 */
int
channels_remove(struct channels *ch, struct strbuf const *path)
{
	size_t i;

	for (i = 0; i < ch->len; ++i) {
		if (0 == stxcmp(stxref(ch->names + i), name)) {
			// Close any open OS resources.
			close(ch->fds[i]);
			// Swap last channel with removed channel.
			ch->fds[i] = ch->fds[ch->len - 1];
			ch->names[i] = ch->names[ch->len - 1];
			ch->len--;
			return 0;
		}
	}
	return -1;
}

/**
 * Log to the output file with the given channel name.
 */
void
channels_log(struct strbuf const *path, struct strbuf const *msg)
{
	FILE *fp;
	struct strbuf tmp = SB_INIT;

	sb_cpy_sb(&tmp, path);
	sb_cpy_str(&tmp, "/out");
	if (!(fp = fopen(tmp.mem, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", tmp.mem);
	} else {
		logtime(fp);
		fprintf(fp, " %.*s\n", (int)msg.len, msg.mem);
		fclose(fp);
	}
	sb_release(&tmp);
}
