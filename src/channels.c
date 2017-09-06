#include <stdlib.h>

#include "channels.h"

int channels_add(Channels *ch, const spx name);
int channels_del(Channels *ch, const spx name);
void channels_log(const spx name, const spx msg);
static int channels_open_fifo(const spx path);

/**
 * Opens a fifo file named "in" at the end of the given path.
 *
 * Return: The open fifo file descriptor. -1 if an error opening it occured.
 */
static int
channels_open_fifo(const spx path)
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
channels_add(Channels *ch, const spx name)
{
	size_t diff;
	size_t nextsize;
	stx *sp;
	void *tmp;

	if (ch->len >= ch->size) {
		nextsize = nearpowerof2(ch->size + 1);
		diff = nextsize - ch->size;
		tmp = realloc(ch->fds,
				sizeof(*ch->fds) * nextsize
				+ sizeof(*ch->names) * nextsize);
		if (!tmp)
				return -1;
		ch->fds = tmp;
		ch->names = (stx *)ch->fds + nextsize;
		// Zero initialize the added memory.
		memset(&ch->fds[ch->size], 0, sizeof(*ch->fds) * diff);
		memset(&ch->names[ch->size], 0, sizeof(*ch->names) * diff);
		ch->size = nextsize;
	}
	sp = &ch->names[ch->len];
	if (0 > stxensuresize(sp, name.len)) {
		LOGERROR("Allocation of path buffer for channel \"%.*s\" failed.\n",
				(int)name.len, name.mem);
		return -1;
	}
	stxcpy_spx(sp, name);
	if (0 > mkdirpath(stxref(sp))) {
		LOGERROR("Directory creation for path \"%.*s\" failed.\n",
				(int)sp->len, sp->mem);
		return -1;
	}
	if (0 > (ch->fds[ch->len] = channels_open_fifo(stxref(sp))))
		return -1;
	ch->len += 1;
	return 0;
}

/**
 * Remove a channel from a Channels struct.
 */
int
channels_del(Channels *ch, const spx name)
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
channels_log(const spx name, const spx msg)
{
	FILE *fp;
	char tmp[name.len + sizeof("/out")];

	memcpy(tmp, name.mem, name.len);
	strcpy(tmp + name.len, "/out");
	if (!(fp = fopen(tmp, "a"))) {
		LOGERROR("Output file \"%s\" failed to open.\n", tmp);
		return;
	}
	logtime(fp);
	fprintf(fp, " %.*s\n", (int)msg.len, msg.mem);
	fclose(fp);
}
