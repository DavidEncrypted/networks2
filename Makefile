# Skeleton Computer networks, Leiden University

# Submission by: David Schep s2055961

CC      ?= gcc
SRC      = src
OBJS     = obj

WARNINGS = -Wall -Wextra -Wno-unused-variable -pedantic -g
IDIRS    = -I$(SRC)
LDIRS    =  -lm -lasound -pthread
CFLAGS   = $(IDIRS) -std=gnu99 $(WARNINGS) $(LDIRS)

find = $(shell find $1 -type f ! -path $3 -name $2 -print 2>/dev/null)

SERVERSRCS := $(call find, $(SRC)/, "*.c", "*/client/*")
CLIENTSRCS := $(call find, $(SRC)/, "*.c", "*/server/*")
SERVEROBJECTS := $(SERVERSRCS:%.c=$(OBJS)/%.o)
CLIENTOBJECTS := $(CLIENTSRCS:%.c=$(OBJS)/%.o)

all: server client

server: $(SERVEROBJECTS)
	$(CC) $(SERVEROBJECTS) -o $@ $(CFLAGS)

client: $(CLIENTOBJECTS)
	$(CC) $(CLIENTOBJECTS) -o $@ $(CFLAGS)

$(OBJS)/%.o: %.c
	@$(call echo,$(CYAN)Compiling $<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@echo Cleaning...
	@rm -rf $(OBJS) server client
	@echo Done!

c: clean

.PHONY: c clean
