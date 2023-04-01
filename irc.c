#define _BSD_SOURCE

#include <string.h>

#include "irc.h"

/* irc_parse parses msg of size n into irc_message.
 * irc_parse rewrites msg in place, and the output values are valid until msg
 * is edited or freed.
 */
int
irc_parse(char *msg, struct irc_message *out)
{
	// Wipe irc_message
	memset(out, 0, sizeof(struct irc_message));

	// Read tags
	if (msg[0] == '@') {
		out->tags = strsep(&msg, " ") + 1; // Pop off @
	}

	// msg must still be non-NULL
	if (!msg) return -1;

	// Read source
	if (msg[0] == ':') {
		out->source = strsep(&msg, " ") + 1; // Pop off :
	}

	// msg must still be non-NULL
	if (!msg) return -1;

	// Read command
	out->command = strsep(&msg, " ");

	// Read params
	for (int i = 0; msg && i < sizeof(out->params)/sizeof(*out->params); ++i) {
		if (msg[0] == ':') {
			out->params[i] = msg + 1;
			return 0;
		}
		out->params[i] = strsep(&msg, " ");
	}

	return 0;
}
