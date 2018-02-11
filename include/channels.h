/* See LICENSE file for copyright and license details */
struct channels {
	size_t size;
	size_t len;
	int *fds;
	struct strbuf *paths;
};

int channels_add(struct channels *ch, struct strbuf const *path);
int channels_del(struct channels *ch, struct strbuf const *path);
void channels_log(struct strbuf const *path, struct strbuf const *msg);
