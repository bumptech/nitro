/*
 * WvTest:
 *   Copyright (C)1997-2012 Net Integration Technologies and contributors.
 *       Licensed under the GNU Library General Public License, version 2.
 *       See the included file named LICENSE for license information.
 *       You can get wvtest from: http://github.com/apenwarr/wvtest
 */
#include "wvtest.h"
#ifdef HAVE_WVCRASH
# include "wvcrash.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

static bool fd_is_valid(int fd)
{
#ifdef _WIN32
    if ((HANDLE)_get_osfhandle(fd) != INVALID_HANDLE_VALUE) return true;
#endif
    int nfd = dup(fd);
    if (nfd >= 0)
    {
	close(nfd);
	return true;
    }
    return false;

}


static int fd_count(const char *when)
{
    int count = 0;
    int fd;
    printf("fds open at %s:", when);

    for (fd = 0; fd < 1024; fd++)
    {
	if (fd_is_valid(fd))
	{
	    count++;
	    printf(" %d", fd);
	    fflush(stdout);
	}
    }
    printf("\n");

    return count;
}


int main(int argc, char **argv)
{
    char buf[200];
#if defined(_WIN32) && defined(HAVE_WVCRASH)
    setup_console_crash();
#endif

    // test wvtest itself.  Not very thorough, but you have to draw the
    // line somewhere :)
    WVPASS(true);
    WVPASS(1);
    WVFAIL(false);
    WVFAIL(0);
    int startfd, endfd;
    char * const *prefixes = NULL;

    if (argc > 1)
	prefixes = argv + 1;

    startfd = fd_count("start");
    int ret = wvtest_run_all(prefixes);

    if (ret == 0) // don't pollute the strace output if we failed anyway
    {
	endfd = fd_count("end");

//	WVPASS(startfd == endfd); // jamwt - disabled for now; libuv makes a few pipes?
#ifndef _WIN32
	if (0 && startfd != endfd)
	{
	    sprintf(buf, "ls -l /proc/%d/fd", getpid());
	    system(buf);
	}
#endif
    }

    // keep 'make' from aborting if this environment variable is set
    if (getenv("WVTEST_NO_FAIL"))
	return 0;
    else
	return ret;
}
