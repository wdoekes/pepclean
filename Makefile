CFLAGS = -Wall -O3 -g
LDFLAGS = -Wall -O3 -g
PREFIX = /usr/local

.PHONY: all
all: pepclean test

.PHONY: clean
clean:
	$(RM) pepclean.o pepclean mangled expected

.PHONY: install
install: $(PREFIX)/bin/pepclean

.PHONY: uninstall
uninstall:
	$(RM) $(PREFIX)/bin/pepclean

$(PREFIX)/bin/pepclean: pepclean
	install -t $(PREFIX)/bin pepclean

pepclean: pepclean.o

.PHONY: test
test: pepclean
	@echo
	@echo '****** RUNNING TESTS ******'
	@echo
	# empty file
	printf '' > mangled
	printf '' > expected
	./pepclean mangled
	diff -pu mangled expected
	# CRs
	printf 'X\nY\r\nZ\n' > mangled
	printf 'X\nY\nZ\n' > expected
	./pepclean mangled
	diff -pu mangled expected
	# TABs
	printf 'X  \t  X\n' > mangled
	printf 'X            X\n' > expected
	./pepclean mangled
	diff -pu mangled expected
	# trailing spaces
	printf 'abc  \n' > mangled
	printf 'abc\n' > expected
	./pepclean mangled
	diff -pu mangled expected
	# one or more LFs
	printf 'a\n' > expected
	for x in `seq 1 40`; do \
		printf 'a' > mangled; \
		for y in `seq $$x`; do printf '\n' >> mangled; done; \
		./pepclean mangled; \
		if ! diff -pu mangled expected; then \
			echo "error at 'a' + $$x LFs" >&2; \
			exit 1; \
		fi; \
	done
	# single LF
	printf '\n' > mangled
	printf '' > expected
	./pepclean mangled
	diff -pu mangled expected
	# zero LFs
	printf 'abcdef' > mangled
	printf 'abcdef\n' > expected
	./pepclean mangled
	diff -pu mangled expected
	# giant file without LF
	printf '' > mangled
	for x in `seq 2048`; do \
		printf 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' >> mangled; \
	done
	cp mangled expected
	printf '\n' >> expected
	./pepclean mangled
	diff -pu mangled expected
	@echo
	@echo '****** FINISHED TESTS ******'
	@echo
	$(RM) mangled expected
