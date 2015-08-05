PREFIX = /usr/local

CFLAGS = -std=c11 -g -Wall -Werror -Wextra
LDLIBS = -lm

FASTER = -O2 -march=native -mtune=native -fomit-frame-pointer -DNDEBUG
CFLAGS += $(FASTER)

OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))

all: bayes_fss doc/bayes_fss.pdf

bayes_fss: $(OBJS)
	$(CC) $(CFLAGS) $(LDLIBS) $^ -o $@

src/help_screen.h: src/help_screen.txt
	./scripts/mkcstring.py < $< > $@

doc/bayes_fss.pdf: doc/bayes_fss.1
	groff -mandoc -K UTF-8 -f H -t -T ps $< | ps2pdf - $@

include src/Makefile.dep

depend:
	gcc -MM src/*.c | sed 's/^.*o:/src\/\0/g' > src/Makefile.dep

check: bayes_fss
	cd test && ./check_search.sh
	cd test && ./check_eval.sh ./data/empty.tsv
	cd test && ./check_eval.sh ./data/sbd.tsv

clean:
	rm -f bayes_fss $(OBJS)

install: bayes_fss doc/bayes_fss.1
	install -spm 0755 bayes_fss $(PREFIX)/bin/bayes_fss
	install -pm 0644 doc/bayes_fss.1 $(PREFIX)/share/man/man1

uninstall:
	rm -f $(PREFIX)/bin/bayes_fss
	rm -f $(PREFIX)/share/man/man1/bayes_fss.1

.PHONY: all depend check clean install uninstall
