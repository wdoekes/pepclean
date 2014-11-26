CFLAGS = -Wall -O3 -g
LDFLAGS = -Wall -O3 -g

.PHONY: all
all: pepclean test

pepclean: pepclean.o

.PHONY: test
test: pepclean
	# empty file
	printf '' > mangled
	printf '' > expected
	./pepclean mangled
	diff -pu mangled expected
	## CRs
	#printf 'X\nY\r\nZ\n' > mangled
	#printf 'X\nY\nZ\n' > expected
	#./pepclean mangled
	#diff -pu mangled expected
	## TABs
	#printf 'X  \t  X' > mangled
	#printf 'X            X' > expected
	#./pepclean mangled
	#diff -pu mangled expected
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
