#define _BSD_SOURCE

#include <stdio.h>
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

/* irc_string converts msg to its string representation, excluding the trailing
 * "\r\n" bytes.
 *
 * irc_string returns the amount of bytes it has written, or -1 if the buffer
 * was too small or is less than 512 bytes.
 *
 * When msg is not a valid IRC message, the behaviour is undefined.
 */
int
irc_string(struct irc_message *msg, char *buf, size_t n)
{
	int ptr = 0;

	// if (n < 512) return -1;

	/* Note that snprintf won't fail here, and if the length is below or
	 * equal to zero then the invalid address buf+ptr will never be
	 * accessed nor written to. */

	if (msg->tags)
		ptr = snprintf(buf, n, "@%s ", msg->tags);
	if (msg->source)
		ptr += snprintf(buf+ptr, n-ptr, ":%s ", msg->source);
	if (msg->command)
		ptr += snprintf(buf+ptr, n-ptr, "%s", msg->command);
	for (int i = 0; i < sizeof(msg->params)/sizeof(*msg->params); ++i) {
		if (!msg->params[i]) break;

		if (strchr(msg->params[i], ' ')) {
			ptr += snprintf(buf+ptr, n-ptr, " :%s", msg->params[i]);
			break;
		}

		ptr += snprintf(buf+ptr, n-ptr, " %s", msg->params[i]);
	}

	return ptr > n ? -1 : ptr;
}
