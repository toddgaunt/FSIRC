/* See LICENSE file for copyright and license details */
typedef struct {
	size_t size;
	size_t len;
	int *fds;
	stx *names;
} Channels;

int channels_add(Channels *ch, const spx name);
int channels_del(Channels *ch, const spx name);
void channels_log(const spx name, const spx msg);
