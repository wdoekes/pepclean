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
