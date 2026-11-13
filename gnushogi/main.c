/*
 * FILE: main.c
 *
 * ----------------------------------------------------------------------
 * Copyright (c) 1993, 1994, 1995 Matthias Mutz
 * Copyright (c) 1999 Michael Vanier and the Free Software Foundation
 * Copyright (c) 2008, 2013, 2014 Yann Dirson and the Free Software Foundation
 *
 * GNU SHOGI is based on GNU CHESS
 *
 * Copyright (c) 1988, 1989, 1990 John Stanback
 * Copyright (c) 1992 Free Software Foundation
 *
 * This file is part of GNU SHOGI.
 *
 * GNU Shogi is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * GNU Shogi is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with GNU Shogi; see the file COPYING. If not, see
 * <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------
 *
 */

#include "gnushogi.h"

#include <signal.h>


#ifdef AV_PATHS
/*
 * Asciiverse: where the opening book and the player's files live, for a
 * program installed anywhere (see PATCHES.md).
 *
 * The text book is read from the path compiled in (BOOK), else from the
 * program's own folder. The binary book, which GNU Shogi writes, goes to
 * the player's state folder. When the program is started from its own
 * folder, as av starts it, it moves into the state folder, so games
 * saved with "save" and "list" land there instead of beside the program.
 */
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#elif defined(_WIN32)
#  include <direct.h>
#  define mkdir(p, m) _mkdir(p)
#endif

static int av_book_given, av_binbook_given;
static char av_bookpath[MAXPATHLEN];
static char av_binbookpath[MAXPATHLEN];

/* The folder the running program is in, or "" if it cannot be found. */
static void
av_exe_dir(char *dir, size_t n)
{
    char *slash;

    dir[0] = '\0';
#if defined(__APPLE__)
    {
        uint32_t size = (uint32_t)n;
        if (_NSGetExecutablePath(dir, &size) != 0)
            dir[0] = '\0';
    }
#elif defined(_WIN32)
    if (GetModuleFileNameA(NULL, dir, (DWORD)n) == 0)
        dir[0] = '\0';
#else
    {
        ssize_t len = readlink("/proc/self/exe", dir, n - 1);
        dir[len > 0 ? len : 0] = '\0';
    }
#endif
    slash = strrchr(dir, '/');
#ifdef _WIN32
    if (strrchr(dir, '\\') > slash)
        slash = strrchr(dir, '\\');
#endif
    if (slash)
        *slash = '\0';
    else
        dir[0] = '\0';
}

/* Make each folder of path that is missing. */
static void
av_mkdirs(char *path)
{
    char *p;

    for (p = path + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
            mkdir(path, 0755);
            *p = c;
        }
    }
    mkdir(path, 0755);
}

/* The player's state folder for GNU Shogi, made if missing; "" if no
 * home folder is known. */
static void
av_state_dir(char *dir, size_t n)
{
    const char *base;

    dir[0] = '\0';
#if defined(_WIN32)
    if ((base = getenv("LOCALAPPDATA")) && *base)
        snprintf(dir, n, "%s\\gnushogi", base);
#elif defined(__APPLE__)
    if ((base = getenv("HOME")) && *base)
        snprintf(dir, n, "%s/Library/Application Support/gnushogi", base);
#else
    if ((base = getenv("XDG_STATE_HOME")) && *base == '/')
        snprintf(dir, n, "%s/gnushogi", base);
    else if ((base = getenv("HOME")) && *base)
        snprintf(dir, n, "%s/.local/state/gnushogi", base);
#endif
    if (dir[0])
        av_mkdirs(dir);
}

static int
av_same_dir(const char *a, const char *b)
{
    struct stat sa, sb;

    if (stat(a, &sa) != 0 || stat(b, &sb) != 0)
        return 0;
#ifdef _WIN32
    {
        char ra[MAXPATHLEN], rb[MAXPATHLEN];
        if (!_fullpath(ra, a, sizeof ra) || !_fullpath(rb, b, sizeof rb))
            return 0;
        return _stricmp(ra, rb) == 0;
    }
#else
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
#endif
}

/* The file name at the end of a path. */
static const char *
av_base(const char *path)
{
    const char *slash = strrchr(path, '/');

    return slash ? slash + 1 : path;
}

/* Called once the command line is read: a book given with -b or -B is
 * left as it is. */
static void
av_paths(int book_given, int binbook_given)
{
    char exe[MAXPATHLEN], state[MAXPATHLEN], cwd[MAXPATHLEN];

    av_exe_dir(exe, sizeof exe);
    av_state_dir(state, sizeof state);

    if (!book_given && (bookfile == NULL || access(bookfile, R_OK) != 0)
        && exe[0]) {
        snprintf(av_bookpath, sizeof av_bookpath, "%s/%s", exe,
                 av_base(BOOK));
        bookfile = av_bookpath;
    }
    if (!book_given && !binbook_given && state[0]) {
        snprintf(av_binbookpath, sizeof av_binbookpath, "%s/%s", state,
                 av_base(BINBOOK));
        binbookfile = av_binbookpath;
    }
    if (state[0] && exe[0] && getcwd(cwd, sizeof cwd)
        && av_same_dir(cwd, exe))
        if (chdir(state) != 0)
            ; /* stay where we are */
}
#endif /* AV_PATHS */


void print_arglist(int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++)
        printf("argv[%d] = %s\n", i, argv[i]);

    printf("\n");
}


int
main (int argc, char **argv)
{
    /*
     * Process command-line arguments.
     */

    /* Get rid of the program name. */

    argc--;
    argv++;

    /* CHECKME: get rid of the '+' syntax? */

    while ((argc > 0) && ((argv[0][0] == '-') || (argv[0][0] == '+')))
    {
        switch (argv[0][1])
        {
        case 'a':
            /* Need the "+" syntax here... */
            ahead = ((argv[0][0] == '-') ? false : true);
            break;


        case 'b':
            argc--;
            argv++;

            if (argc > 0)
            {
                bookfile = argv[0];
#ifdef BINBOOK
                binbookfile = NULL;
#endif
#ifdef AV_PATHS
                av_book_given = 1;
#endif
            }
            break;

#ifdef BINBOOK
        case 'B':
            argc--;
            argv++;
            if (argc > 0)
            {
                binbookfile = argv[0];
#ifdef AV_PATHS
                av_binbook_given = 1;
#endif
            }
            break;
#endif

#ifdef HAVE_LIBCURSES
        case 'C':
            /* Curses interface. */
            display_type = DISPLAY_CURSES;
            dsp = &curses_display;
            break;
#endif

        case 'h':
            /* Need the "+" syntax here... */
            hash = ((argv[0][0] == '-') ? false : true);
            break;

        case 'l':
            argc--;
            argv++;

            if (argc > 0)
                Lang = argv[0];
            break;

        case 'L':
            argc--;
            argv++;

            if (argc > 0)
                strcpy(listfile, argv[0]);
            break;

        case 's':
            argc--;
            argv++;

            if (argc > 0)
                strcpy(savefile, argv[0]);
            break;

        case 'P':
            argc--;
            argv++;

            if (argc > 0)
                bookmaxply = atoi(argv[0]);
            break;

        case 'R':
            /* Raw text interface. */
            display_type = DISPLAY_RAW;
            dsp = &raw_display;
            break;

        case 'S':
            argc--;
            argv++;

            if (argc > 0)
                booksize = atoi(argv[0]);
            break;

#if ttblsz
        case 'r':
            argc--;
            argv++;

            if (argc > 0)
                rehash = atoi(argv[0]);
            if (rehash > MAXrehash)
                rehash = MAXrehash;
            break;

        case 'T':
            argc--;
            argv++;

            if (argc > 0)
                ttblsize = atoi(argv[0]);
            if (ttblsize <= MINTTABLE)
                ttblsize = (MINTTABLE) + 1;
            break;

#ifdef HASHFILE
        case 'c':   /* Create or test persistent transposition table. */
            argc--;
            argv++;

            if (argc > 0)
                filesz = atoi(argv[0]);
            else
                filesz = vfilesz;

            if ((filesz > 0) && (filesz < 24))
                filesz = (1 << filesz) - 1 + MAXrehash;
            else
                filesz = filesz + MAXrehash;

            if ((hashfile = fopen(HASHFILE, RWA_ACC)) == NULL)
                hashfile = fopen(HASHFILE, WA_ACC);

            if (hashfile != NULL)
            {
                long j;
                struct fileentry n;

                fputs("Filling transposition file, wait!\n", stdout);
                n.f = n.t = 0;
                n.flags = 0;
                n.depth = 0;
                n.sh = n.sl = 0;

                for (j = 0; j < filesz + 1; j++)
                    fwrite(&n, sizeof(struct fileentry), 1, hashfile);

                fclose(hashfile);
            }
            else
            {
                printf("Create failed for %s\n", HASHFILE);
            }

            return 0;

        case 't':   /* Create or test persistent transposition table. */
            hashfile = fopen(HASHFILE, RWA_ACC);

            if (hashfile)
            {
                fseek(hashfile, 0L, SEEK_END);
                filesz = (ftell(hashfile) / (sizeof(struct fileentry))) - 1;
            }

            if (hashfile != NULL)
            {
                long i, j;
                int nr[MAXDEPTH];
                struct fileentry n;

                fputs("Counting transposition file entries, wait!\n", stdout);

                for (i = 0; i < MAXDEPTH; i++)
                    nr[i] = 0;

                fseek(hashfile, 0L, SEEK_END);
                i = ftell(hashfile) / (sizeof(struct fileentry));
                fseek(hashfile, 0L, SEEK_SET);

                for (j = 0; j < i + 1; j++)
                {
                    fread(&n, sizeof(struct fileentry), 1, hashfile);

                    if (n.depth > MAXDEPTH)
                    {
                        printf("ERROR\n");
                        exit(1);
                    }

                    if (n.depth)
                    {
                        nr[n.depth]++;
                        nr[0]++;
                    }
                }

                printf("The file contains %d entries out of max %ld\n", nr[0], i);

                for (j = 1; j < MAXDEPTH; j++)
                    printf("%d ", nr[j]);

                printf("\n");
            }

            return 0;

#endif /* HASHFILE */
#endif /* ttblsz */

        case 'v':
            fprintf(stderr, "gnushogi version %s\n", PACKAGE_VERSION);
            exit(1);


        case 'X':
            /* X interface. */
            display_type = DISPLAY_X;
            dsp = &raw_display;
            break;

        case 'x':
            argc--;
            argv++;

            if (argc > 0)
                xwin = argv[0];
            break;

        default:
            fputs("Usage: gnushogi [-a] [-t] [-c size] [-s savefile][-l listfile] [-x xwndw]\n", stderr);
            exit(1);
        }

        argc--;
        argv++;
    }

    if (argc == 2)
    {
        char *p;

        MaxResponseTime = 100L * strtol(argv[1], &p, 10);

        if (*p == ':')
        {
            MaxResponseTime = 60L * MaxResponseTime +
                100L * strtol(++p, (char **) NULL, 10);
        }

        TCflag    = false;
        TCmoves   = 0;
        TCminutes = 0;
        TCseconds = 0;
    }

    if (argc >= 3)
    {
        char *p;

        if (argc > 9)
        {
            printf("Time Control Error\n");
            exit(1);
        }

        TCmoves   = atoi(argv[1]);
        TCminutes = (short)strtol(argv[2], &p, 10);

        if (*p == ':')
            TCseconds = (short)strtol(p + 1, (char **) NULL, 10);
        else
            TCseconds = 0;

        TCflag = true;
        argc -= 3;
        argv += 3;

        while (argc > 1)
        {
            XCmoves[XC]   = atoi(argv[0]);
            XCminutes[XC] = (short)strtol(argv[1], &p, 10);

            if (*p == ':')
                XCseconds[XC] = (short)strtol(p + 1, (char **) NULL, 10);
            else
                XCseconds[XC] = 0;

            if (XCmoves[XC] && (XCminutes[XC] || XCseconds[XC]))
                XC++;
            else
            {
                printf("Time Control Error\n");
                exit(1);
            }

            argc -= 2;
            argv += 2;
        }

        if (argc)
        {
            /*
             * If we got here, there are unknown arguments, so issue
             * an error message and quit.
             */

            printf("Invalid command-line arguments:\n");
            print_arglist(argc, argv);
            exit(1);
        }
    }

#ifdef AV_PATHS
    av_paths(av_book_given, av_binbook_given);
#endif

    if (InitMain() != 0)
        exit(1);

    while (!flag.quit)
    {
        oppptr = (oppptr + 1) % MINGAMEIN;

        if (flag.bothsides && !flag.mate)
            SelectMove(opponent, FOREGROUND_MODE);
        else
            InputCommand(NULL);

        if (opponent == white)
        {
            if (flag.gamein || TCadd)
            {
                TimeCalc();
            }
            else if (TimeControl.moves[opponent] == 0)
            {
                if (XC)
                {
                    if (XCmore < XC)
                    {
                        TCmoves   = XCmoves[XCmore];
                        TCminutes = XCminutes[XCmore];
                        TCseconds = XCseconds[XCmore];
                        XCmore++;
                    }
                }

                SetTimeControl();
            }
        }

        compptr = (compptr + 1) % MINGAMEIN;

        if (!(flag.quit || flag.mate || flag.force))
        {
#ifdef INTERRUPT_TEST
            printf("starting search...\n");
#endif
            SelectMove(computer, FOREGROUND_MODE);

            if (computer == white)
            {
                if (flag.gamein)
                {
                    TimeCalc();
                }
                else if (TimeControl.moves[computer] == 0)
                {
                    if (XC)
                    {
                        if (XCmore < XC)
                        {
                            TCmoves = XCmoves[XCmore];
                            TCminutes = XCminutes[XCmore];
                            TCseconds = XCseconds[XCmore];
                            XCmore++;
                        }
                    }

                    SetTimeControl();
                }
            }
        }
    }

    ExitMain();

    return 0;
}


