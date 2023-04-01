# ICBM

ICBM is a tiny single-threaded single-user single-network IRC bouncer written in
C.
It is not meant to be a full featured bouncer with all of the bells and
whistles, rather a simple daemon that full featured clients/other daemons
connect to.

## Bouncing several networks

In order to bounce several networks, you need several instances of ICBM.
Each process maps to one network connection and to several client connections.
