APPNAME=my
LIBDIRNAME=libmy

EXTRA_LDFLAGS=
	
SUBDIRS	=
OBJS    = my.o uloop.o usock.o ustream.o ustream-fd.o utils.o
include $(SWITCH_BASE)/configs/rules.app

CC_OPTIM=
