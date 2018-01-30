[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fwdoekes%2Fpepclean.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2Fwdoekes%2Fpepclean?ref=badge_shield)

pepclean
========

*Quickly cleaning up excess space from Python and HTML files.*

Before:

    $ cat hello.py
    def hello(i):
        print 'Hello',
        if i == 0:
            print 'world'
        else:
            print 'there'

But, if we look closer:

    $ cat -A hello.py
    def hello(i):$
        print 'Hello',^M$
        if i == 0:$
    ^Iprint 'world'$
        else:    $
            print 'there'$

Somehow we snuck some stupid whitespace cruft in there.

*pepclean to the rescue!*

    $ cp hello.py backup.py
    $ pepclean hello.py

    $ cat -A hello.py
    def hello(i):$
        print 'Hello',$
        if i == 0:$
            print 'world'$
        else:$
            print 'there'$

Nice, no more cruft in there.

    $ diff -pu backup.py hello.py
    --- backup.py   2014-11-26 02:27:26.922717358 +0100
    +++ hello.py    2014-11-26 02:27:33.498765839 +0100
    @@ -1,6 +1,6 @@
     def hello(i):
    -    print 'Hello',
    +    print 'Hello',
         if i == 0:
    -       print 'world'
    -    else:
    -        print 'there'
    \ No newline at end of file
    +        print 'world'
    +    else:
    +        print 'there'


Enjoy,

Walter Doekes


Caveats
-------

BEWARE: Do not run pepclean on (binary) files with embedded NULs.


Explanation
-----------

The goal was creating a pre-commit hook that cleaned up my HTML and
Python files:

* Get rid of those pesky trailing spaces at EOL and blank lines at EOF.
* Clean up and get rid of TABs and CRs, while we're at it.

I started out doing this in my `Makefile`:

    pepclean:
            @# Replace tabs with spaces, remove trailing spaces.
            find . '(' -name '*.py' -o -name '*.html' ')' -print0 | \
                    xargs --no-run-if-empty -0 sed -s -i -e "\
                            s/`printf '\t'`/    /g; \
                            s/ \+\$$//; \
                    "
            @# Remove trailing newlines.
            find . '(' -name '*.py' -o -name '*.html' ')' -print0 | \
                    xargs --no-run-if-empty -0 \
                            sed -s -i -e :a -e '/^\n*$$/{$$d;N;};/\n$$/ba'

But that touched every file in my repo upon every commit. Not only did
it mess up my timestamps, the git commit message also told me that all
those files had been modified but not included in this commit.

My second attempt convoluted the script by searching for the issues
before actually patching them.  A two pass approach if you will.

    pepclean:
            @# Replace tabs with spaces, remove trailing spaces.
            find . '(' -name '*.py' -o -name '*.html' ')' -print0 | \
                    xargs --no-run-if-empty -0 \
                    grep -lE "`printf '\t'`| \$$" | \
                    xargs --no-run-if-empty -d\\n sed -s -i -e "\
                            s/`printf '\t'`/        /g; \
                            s/ \+\$$//; \
                    "
            @# Remove trailing newlines.
            for f in `find . '(' -name '*.py' -o -name '*.html' ')'`; do \
                    if sed -ne '$$s/^$$/whatever/p' "$$f" | grep -q ''; then \
                            sed -s -i -e :a -e '/^\n*$$/{$$d;N;};/\n$$/ba' "$$f"; \
                    fi; \
            done

Because of the two pass approach, it is actually faster than the first
version.  But it looks horrible and it still takes about 2 or 3 seconds
on 1000 files.

*Hence, these 500 lines of C code.*

It now looks like this:

    pepclean:
            @# Replace tabs with spaces, remove trailing spaces, remove trailing newlines.
            find . '(' -name '*.py' -o -name '*.html' ')' -print0 | \
                    xargs --no-run-if-empty -0 pepclean

That went from 3 seconds system (2s real) time to about 50 ms (100 ms
real) on a sample of about 1000 files.  And the C version also fixes
the files with missing trailing newlines.


## License
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fwdoekes%2Fpepclean.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2Fwdoekes%2Fpepclean?ref=badge_large)