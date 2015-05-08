/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in
 *  the LICENSE file in the root directory of this source tree. An
 *  additional grant of patent rights can be found in the PATENTS file
 *  in the same directory.
 *
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "util.h"
#include "chat.h"

__attribute__((noreturn))
static void chat_die(void)
{
    die(ECOMM, "lost connection to child");
}


struct chat*
chat_new(int to, int from)
{
    struct chat* cc = xcalloc(sizeof (*cc));
    cc->to = xfdopen(to, "w");
    cc->from = xfdopen(from, "r");
    return cc;
}

char
chat_getc(struct chat* cc)
{
    int c;

    {
        c = getc(cc->from);
    }

    if (c == EOF)
        chat_die();

    return c;
}

void
chat_expect(struct chat* cc, char expected)
{
    int c = chat_getc(cc);
    if ((char)c != expected) {
        die(ECOMM,
            "[child] expected 0x%02x %c, found 0x%02x %c",
            expected,
            isprint(expected) ? expected : '.',
            (char) c,
            isprint(c) ? c : '.');
    }
}

void
chat_expect_maybe(struct chat* cc, char expected)
{
    char c = chat_getc(cc);
    if (c != expected)
        ungetc(c, cc->from);
}

void
chat_swallow_prompt(struct chat* cc)
{
    /* 100% reliable prompt detection */
    char c;
    do {
        c = chat_getc(cc);
    } while (c != '#' && c != '$');

    chat_expect(cc, ' ');
}

void
chat_talk_at(struct chat* cc, const char* what)
{
    if (fputs(what, cc->to) == EOF)
        chat_die();

    if (putc('\n', cc->to) == EOF)
        chat_die();

    if (fflush(cc->to) == EOF)
        chat_die();

    /* We expect the child to echo us, so read back the echoed
     * characters.  */
    while (*what)
        chat_expect(cc, *what++);

    /* Yes, this is really what comes back after a \n.  */
    chat_expect(cc, '\r');
    chat_expect_maybe(cc, '\r');
    chat_expect(cc, '\n');
}

char*
chat_read_line(struct chat* cc)
{
    char line[512];
    line[0] = '\0';
    if (fgets(line, sizeof (line), cc->from) == NULL &&
        ferror(cc->from))
    {
        die(ECOMM, "lost connection to child");
    }

    size_t linesz = strlen(line);
    while (linesz > 0 && strchr("\r\n", line[linesz - 1]))
        linesz--;
    line[linesz] = '\0';
    return xstrdup(line);
}
