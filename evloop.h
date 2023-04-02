extern int acceptfd;

void init_evloop(void);
void evloop(void);

void ev_set_writeout(int fd, int val);
