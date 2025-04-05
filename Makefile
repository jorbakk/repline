CFLAGS  += -fPIC -Wno-unused-function -DRPL_HIST_IMPL_SQLITE
LDFLAGS += -lsqlite3
PREFIX  ?= /usr/local

.PHONY: all test clean install

ifeq ($(DEBUG), 1)
  CFLAGS += -g # -DRPL_DEBUG_TO_FILE
endif

SRCS = attr.c bbcode.c bbcode_colors.c common.c completers.c completions.c editline.c editline_completion.c editline_help.c editline_history.c example.c highlight.c history.c history_sqlite.c repline.c stringbuf.c term.c term_color.c test_colors.c tty.c tty_esc.c undo.c wcwidth.c
HDRS = attr.h bbcode.h common.h completions.h env.h highlight.h history.h repline.h stringbuf.h term.h tty.h undo.h

all: cscope.out librepline.a librepline.so example test_colors

repline.o: $(SRCS) $(HDRS)

librepline.a: repline.o
	$(AR) src $@ $<
librepline.so: repline.o
	$(CC) -shared $(LDFLAGS) -o $@ $<

example: example.o repline.o
	$(CC) -o $@ $^ $(LDFLAGS) 
test_colors: test_colors.c repline.o
	$(CC) -o $@ $^ $(LDFLAGS) 
test/completion: test/completion.c $(SRCS)
	$(CC) -o $@ $< $(LDFLAGS)

test: test/completion
	cd test && ./completion

clean:
	rm -rf *.o librepline.a librepline.so example test_colors test/completion

cscope.out: $(SRCS)
	cscope -b $(SRCS)

install: all
	mkdir -p $(DESTDIR)/$(PREFIX)/lib
	mkdir -p $(DESTDIR)/$(PREFIX)/include
	install -m 644 librepline.a  $(DESTDIR)/$(PREFIX)/lib
	install -m 644 librepline.so $(DESTDIR)/$(PREFIX)/lib
	install -m 644 repline.h $(DESTDIR)/$(PREFIX)/include

