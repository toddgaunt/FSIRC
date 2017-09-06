/* See LICENSE file for copyright and license details */
/* npow2() - Calculate the nearest power of two greater than or equal to "max".
 */
static inline size_t
npow2(const size_t max)
{
	size_t next = 1;

	if (max >= SIZE_MAX / 2) {
		next = SIZE_MAX;
	} else {
		while (next < max)
			next <<= 1;
	}
	return next;
}

