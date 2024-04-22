CFLAGS  += -fPIC -Wno-unused-function -DRPL_HIST_IMPL_SQLITE
LDFLAGS += -lsqlite3
PREFIX   = /usr/local

all: librepline.a librepline.so example test_colors

librepline.a: repline.o
	$(AR) src $@ $<

librepline.so: repline.o
	$(CC) -shared $(LDFLAGS) -o $@ $<

example: example.o repline.o
	$(CC) -o $@ $^ $(LDFLAGS) 

test_colors: test_colors.c repline.o
	$(CC) -o $@ $^ $(LDFLAGS) 

clean:
	rm -rf *.o librepline.a librepline.so example test_colors

install: all
	mkdir -p $(DESTDIR)/$(PREFIX)/lib
	mkdir -p $(DESTDIR)/$(PREFIX)/include
	install -m 644 librepline.a  $(DESTDIR)/$(PREFIX)/lib
	install -m 644 librepline.so $(DESTDIR)/$(PREFIX)/lib
	install -m 644 repline.h $(DESTDIR)/$(PREFIX)/include

