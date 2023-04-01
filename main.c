#include <stdlib.h>

#include "log.h"
#include "irc.h"

int
main(void)
{
	char msg[] = "@some=tags :source command param1 param2 :trailing parameter";

	struct irc_message m = {0};
	if (irc_parse(msg, &m)) {
		errorf("Failed to parse IRC message");
		exit(EXIT_FAILURE);
	}

	if (m.tags)
		infof("tags: %s$", m.tags);
	if (m.source)
		infof("source: %s$", m.source);
	if (m.command)
		infof("command: %s$", m.command);
	if (m.params) {
		for (int i = 0; i < sizeof(m.params)/sizeof(*m.params); ++i) {
			if (m.params[i] == NULL) break;

			infof("param %d: %s$", i, m.params[i]);
		}
	}
}
