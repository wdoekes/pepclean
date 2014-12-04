/* vim: set ts=8 sw=4 sts=4 et ai: */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int pepclean(const char *filename);
static int needs_work(const char *filename, int *checks_to_run);
static int do_work(const char *filename, const int *checks_to_run);

static int has_line_issues(FILE *in);
static int fix_line_issues(const char *filename);
static int fix_line_issues_2(FILE *in, FILE *out);

static int has_tail_issues(FILE *in);
static int fix_tail_issues(const char *filename);

struct checks_t {
    /* The detect functions are supposed to fseek to start/end
     * themselves. */
    int (*detect)(FILE*);
    int (*fixfun)(const char *filename);
};

struct checks_t checklist[] = {
    {has_line_issues, fix_line_issues},
    {has_tail_issues, fix_tail_issues},
};
#define ARRAY_LEN(object) (sizeof(object) / sizeof(object[0]))
const int checklist_len = ARRAY_LEN(checklist);


int main(int argc, const char **argv)
{
    int i;
    int ret = 0;

    if (argc == 1) {
        printf("pepclean: Checks and cleans up files according to a "
               "few basic rules:\n"
               "- no CRs, no TABs\n"
               "- no trailing space at EOL\n"
               "- a trailing LF at EOF unless the file is empty\n"
               "- not more than one trailing LF at EOF\n"
               "Pass one or more files as arguments to be modified inline.\n"
               "\n"
               "The idea is that this basic filter is (a) much faster than a "
               "bunch of\n"
               "concatenated sed scripts and (b) does not touch (modify) any "
               "files that\n"
               "do not need any modification.\n"
               "\n"
               "BEWARE: pepclean will truncate binary files because it does "
               "not play\n"
               "well with embedded NULs!\n"
               "\n"
               "Common invocation:\n"
               "\n"
               "    find . '(' -name '*.html' -o -name '*.py' ')' -print0 |\n"
               "      xargs --no-run-if-empty -0 pepclean\n"
               "\n"
               "Returns value 0 if nothing was changed, 1 on error and 2 if "
               "anything\n"
               "was changed. The non-zero return value makes it easier for "
               "pre-commit\n"
               "hooks to abort early.\n"
               "\n"
               "Public Domain, Walter Doekes, 2014\n");
        return 0;
    }

    for (i = 1; i < argc; ++i) {
        ret = pepclean(argv[i]) | ret;
    }

    return ret == 0 ? 0 : (ret == -1 ? 1 : 2);
}

static int pepclean(const char *filename)
{
    int ret;
    int checks_to_run[checklist_len];

    /* First check if we need to do anything at all. If we don't, we
     * don't need to do any writing. */
    ret = needs_work(filename, checks_to_run);
    if (ret < 0)
        return -1;
    if (ret == 0)
        return 0;

    /* We apparently need to do something. Start work. */
    ret = do_work(filename, checks_to_run);
    if (ret < 0)
        return -1;

    /* Ignore other return value. The caller is only interested in
     * failures. */
    return 1;
}

static int needs_work(const char *filename, int *checks_to_run)
{
    FILE *fp;
    int anything = 0;
    int i;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s: fopen: %s\n", filename, strerror(errno));
        return -1;
    }

    for (i = 0; i < checklist_len; ++i) {
        int ret = checklist[i].detect(fp);
        if (ret < 0) {
            fclose(fp);
            return -1;
        }
        checks_to_run[i] = ret;
        anything = anything || ret;
    }

    fclose(fp);
    return anything;
}

static int do_work(const char *filename, const int *checks_to_run)
{
    int i;

    for (i = 0; i < checklist_len; ++i) {
        if (checks_to_run[i]) {
            int ret = checklist[i].fixfun(filename);
            if (ret < 0)
                return -1;
        }
    }

    return 0;
}

static int has_line_issues(FILE *in)
{
    char buf[BUFSIZ + 1];
    buf[BUFSIZ] = '\0'; /* no overflows, ever */

    if (fseek(in, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "has_line_issues:fseek: %s\n", strerror(errno));
        return -1;
    }

    while (1) {
        const char *p;
        if (fgets(buf, BUFSIZ, in) == 0) {
            return 0;
        }

        p = buf;
        while (*p != '\0') {
            /* CRs or TABs? */
            if (*p == '\r' || *p == '\t')
                return 1;
            ++p;
        }

        /* If we get here at all, we have at least some data. In that
         * case all lines must end with a newline. Check it. */
        if (p == buf)
            return 1; /* impossible */
        if (p[-1] != '\n')
            return 1; /* no LF at EOL or crazy long line */
        if (p - 1 > buf && p[-2] == ' ')
            return 1; /* space at EOL (before LF) */
    }

    /* Never gets here. */
    return 1;
}

static int fix_line_issues(const char *filename)
{
    /* Create temp file in same directory (to stay on same FS),
     * overwrite the original when done. Don't forget to fix the
     * permissions/ownership. */
    char tmp_filename[strlen(filename) + 6 + 1];
    char *p;
    FILE *in = NULL;
    FILE *out = NULL;
    int fd;

    p = tmp_filename;
    strcpy(p, filename); /* safe */
    p += strlen(filename);
    strcpy(p, "XXXXXX"); /* safe */

    fd = mkstemp(tmp_filename);
    if (fd < 0) {
        fprintf(stderr, "%s: mkstemp: %s\n", filename, strerror(errno));
        return -1;
    }

    in = fopen(filename, "r");
    if (in == NULL) {
        fprintf(stderr, "%s: fopen: %s\n", filename, strerror(errno));
        goto error;
    }

    out = fdopen(fd, "w");
    if (out == NULL) {
        fprintf(stderr, "%s/%d: fdopen: %s\n", tmp_filename, fd,
                strerror(errno));
        goto error;
    }

    /* Update ownership/permissions. Don't bail out on failure. */
    {
        struct stat st;
        if (fstat(fileno(in), &st) != 0) {
            fprintf(stderr, "%s/%d: fstat: %s\n", filename, fileno(in),
                    strerror(errno));
        } else {
            if (fchmod(fileno(out), st.st_mode) != 0) {
                fprintf(stderr, "%s/%d: fchmod: %s\n", tmp_filename,
                        fileno(out), strerror(errno));
            }
            if (fchown(fileno(out), st.st_uid, st.st_gid) != 0) {
                fprintf(stderr, "%s/%d: fchown: %s\n", tmp_filename,
                        fileno(out), strerror(errno));
            }
        }
    }

    /* Do the actual work. */
    if (fix_line_issues_2(in, out) < 0) {
        fprintf(stderr, "%s: fputs: %s\n", filename, strerror(errno));
        goto error;
    }

    /* Ok. All is good. Overwrite the file. It is important that we
     * check the return value of fclose(out) because we may run into
     * file save (out of disk) issues. */
    if (fclose(out) != 0)
        goto error;
    out = NULL;
    if (fclose(in) != 0)
        goto error;
    in = NULL;

    if (unlink(filename) != 0) {
        fprintf(stderr, "%s: unlink: %s\n", filename, strerror(errno));
        goto error;
    }
    if (rename(tmp_filename, filename) != 0) {
        fprintf(stderr, "%s: rename: %s\n", tmp_filename, strerror(errno));
        goto error;
    }

    return 0;

error:
    if (out)
        fclose(out);
    if (in)
        fclose(in);
    if (unlink(tmp_filename) != 0)
        fprintf(stderr, "%s: unlink: %s\n", tmp_filename, strerror(errno));

    return -1;
}

static int fix_line_issues_2(FILE *in, FILE *out)
{
    char inbuf[BUFSIZ + 1];
    char outbuf[BUFSIZ + 8 + 1];
    char *pin, *pout;
    int i;
    size_t len;
    inbuf[BUFSIZ] = outbuf[BUFSIZ + 8] = '\0'; /* no overflows, ever */

    while (1) {
        if (fgets(inbuf, BUFSIZ, in) == 0) {
            return 0;
        }

        /* A couple of things to do here:
         * - remove trailing space
         * - remove all CRs
         * - replace TABs with spaces
         */
        len = strlen(inbuf);
        if (!len) {
            if (fputs("\n", out) < 0)
                return -1;
            return 0;
        }

        /* Missing trailing LF? */
        if (inbuf[len - 1] != '\n') {
            if (len == BUFSIZ - 1) {
                /* Long line... not touching that. */
                if (fputs(inbuf, out) < 0)
                    return -1;
                continue;
            }
            inbuf[len] = '\n';
            len += 1;
        }

        /* Single LF? */
        if (len == 1) {
            if (fputs("\n", out) < 0)
                return -1;
            continue;
        }

        /* Trailing spaces or TABs or CRs? */
        for (i = len - 2; i >= 0; --i) {
            if (inbuf[i] != ' ' && inbuf[i] != '\t' && inbuf[i] != '\r')
                break;
        }
        if (i < 0) {
            if (fputs("\n", out) < 0)
                return -1;
            continue;
        }
        inbuf[i + 1] = '\n';
        inbuf[i + 2] = '\0';

        /* Are there any CRs or TABs? If not, then continue quickly. */
        if (strchr(inbuf, '\r') == NULL && strchr(inbuf, '\t') == NULL) {
            if (fputs(inbuf, out) < 0)
                return -1;
            continue;
        }

        /* Bah. Look at this stuff byte-by-byte. */
        pin = inbuf;
        pout = outbuf;
        while (1) {
            if (*pin == '\r') {
                /* Skip. */
                ++pin;
                continue;
            }
            if (*pin == '\t') {
                strcpy(pout, "        "); /* safe */
                /* Ok. That was safe, but now we shall flush things. */
                if (fputs(outbuf, out) < 0)
                    return -1;
                pout = outbuf;
                ++pin;
                continue;
            }
            if ((*pout++ = *pin++) == '\0') {
                break;
            }
        }

        /* Flush it. */
        if (fputs(outbuf, out) < 0) {
            return -1;
        }
    }

    /* Never gets here. */
    return 0;
}

static int has_tail_issues(FILE *in)
{
    char buf[2];
    int ret;

    if (fseek(in, -2L, SEEK_END) != 0) {
        if (errno == EINVAL) {
            if (fseek(in, 0L, SEEK_SET) == 0) {
                if (fread(buf, 1, 1, in) == 1 && buf[0] == '\n') {
                    /* 1 byte file with single LF */
                    return 1;
                }
            }
            return 0; /* short file */
        }
        fprintf(stderr, "has_tail_issues:fseek: %s\n", strerror(errno));
        return -1;
    }

    ret = fread(buf, 1, 2, in);
    if (ret != 2) {
        fprintf(stderr, "has_tail_issues:fread: %s (%d)\n", strerror(errno),
                ret);
        return -1;
    }

    /* We only need to check for double LF here -- which means trailing
     * newlines -- and nothing else. The other tests have caught other
     * problems. */
    if (buf[0] == '\n' && buf[1] == '\n') {
        /* Technically, we only need to check buf[0], but checking both
         * doesn't hurt. */
        return 1;
    }

    return 0;
}

static int fix_tail_issues(const char *filename)
{
    /* We can fix this inline. Just seek backwards until there is only
     * one LF left. */
    FILE *fp;
    const int bufsize = 16; /* really small buffer */
    char buf[bufsize + 1];
    int i, j;
    buf[bufsize] = '\0';

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s: fopen: %s\n", filename, strerror(errno));
        return -1;
    }

    /* Seek backwards a bit. If the file is really short or if the file
     * contains many trailing LFs, this will not be efficient. */
    while (1) {
        int ret;
        long pos;

        for (i = bufsize; i > 0; i /= 2) {
            /* Hopefully this only gets called once. */
            if (fseek(fp, -i, SEEK_END) == 0)
                break;
        }

        /* If i is 0, the file is sized 0. */
        if (i == 0) {
            break;
        }

        ret = fread(buf, 1, i, fp);
        if (ret != i) {
            fprintf(stderr, "%s: Modified?\n", filename);
            fclose(fp);
            return -1;
        }

        /* Check backwards until we find a non-LF. */
        for (j = i - 1; j >= 0; --j) {
            if (buf[j] != '\n') {
                break;
            }
        }
        j += 2;
        if (j > i) {
            break;
        }

        /* If we fetched 8 bytes and j is 5, that means that we should
         * truncate the file at -3: [ABCD\n\n\n\n] */
        if (fseek(fp, -(i - j), SEEK_END) != 0) {
            fprintf(stderr, "%s: Modified?\n", filename);
            fclose(fp);
            return -1;
        }
        pos = ftell(fp);

        /* Exception: if pos is 1 then this is an empty file, kill the
         * last LF too. */
        if (pos == 1) {
            pos = 0;
        }

        /* We must close fp when truncating. The stdio functions won't
         * cope. */
        fclose(fp);
        fp = NULL;

        if (truncate(filename, pos) != 0) {
            fprintf(stderr, "%s: truncate: %s\n", filename, strerror(errno));
            return -1;
        }

        /* Are we done? We are if the file size is 1 or less or if we
         * truncated at position 2 or more. */
        if (i <= 1 || j > 1) {
            break;
        }

        /* Re-open if we're not done. */
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "%s: fopen: %s\n", filename, strerror(errno));
            return -1;
        }
    }

    if (fp != NULL) {
        fclose(fp);
    }
    return 0;
}
