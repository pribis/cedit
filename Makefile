CC = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic
LDFLAGS = -lncurses

OBJS = main.o editor.o buffer.o browser.o syntax.o util.o popup.o

cedit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS) cedit

.PHONY: clean
