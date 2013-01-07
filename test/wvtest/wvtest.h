/* -*- Mode: C++ -*-
 * WvTest:
 *   Copyright (C)1997-2012 Net Integration Technologies and contributors.
 *       Licensed under the GNU Library General Public License, version 2.
 *       See the included file named LICENSE for license information.
 *       You can get wvtest from: http://github.com/apenwarr/wvtest
 */
#ifndef __WVTEST_H
#define __WVTEST_H

#ifndef WVTEST_CONFIGURED
# error "Missing settings: HAVE_VALGRIND_MEMCHECK_H HAVE_WVCRASH WVTEST_CONFIGURED"
#endif

#include <time.h>
#include <string.h>
#include <stdbool.h>

typedef void wvtest_mainfunc();

struct WvTest {
	const char *descr, *idstr;
	wvtest_mainfunc *main;
	int slowness;
	struct WvTest *next;
};

void wvtest_register(struct WvTest *ptr);
int wvtest_run_all(char * const *prefixes);
void wvtest_start(const char *file, int line, const char *condstr);
void wvtest_check(bool cond, const char *reason);
static inline bool wvtest_start_check(const char *file, int line,
				      const char *condstr, bool cond)
{ wvtest_start(file, line, condstr); wvtest_check(cond, NULL); return cond; }
bool wvtest_start_check_eq(const char *file, int line,
			   int a, int b, bool expect_pass);
bool wvtest_start_check_lt(const char *file, int line,
			   int a, int b);
bool wvtest_start_check_eq_str(const char *file, int line,
			       const char *a, const char *b, bool expect_pass);
bool wvtest_start_check_lt_str(const char *file, int line,
			       const char *a, const char *b);


#define WVPASS(cond) \
    wvtest_start_check(__FILE__, __LINE__, #cond, (cond))
#define WVPASSEQ(a, b) \
    wvtest_start_check_eq(__FILE__, __LINE__, (a), (b), true)
#define WVPASSLT(a, b) \
    wvtest_start_check_lt(__FILE__, __LINE__, (a), (b))
#define WVPASSEQSTR(a, b) \
    wvtest_start_check_eq_str(__FILE__, __LINE__, (a), (b), true)
#define WVPASSLTSTR(a, b) \
    wvtest_start_check_lt_str(__FILE__, __LINE__, (a), (b))
#define WVFAIL(cond) \
    wvtest_start_check(__FILE__, __LINE__, "NOT(" #cond ")", !(cond))
#define WVFAILEQ(a, b) \
    wvtest_start_check_eq(__FILE__, __LINE__, (a), (b), false)
#define WVFAILEQSTR(a, b) \
    wvtest_start_check_eq_str(__FILE__, __LINE__, (a), (b), false)
#define WVPASSNE(a, b) WVFAILEQ(a, b)
#define WVPASSNESTR(a, b) WVFAILEQSTR(a, b)
#define WVFAILNE(a, b) WVPASSEQ(a, b)
#define WVFAILNESTR(a, b) WVPASSEQSTR(a, b)

#define WVTEST_MAIN3(_descr, ff, ll, _slowness)				\
	static void _wvtest_main_##ll();				\
	struct WvTest _wvtest_##ll = \
	{ .descr = _descr, .idstr = ff ":" #ll, .main = _wvtest_main_##ll, .slowness = _slowness }; \
	static void _wvtest_register_##ll() __attribute__ ((constructor)); \
	static void _wvtest_register_##ll() { wvtest_register(&_wvtest_##ll); } \
	static void _wvtest_main_##ll()
#define WVTEST_MAIN2(descr, ff, ll, slowness)	\
	WVTEST_MAIN3(descr, ff, ll, slowness)
#define WVTEST_MAIN(descr) WVTEST_MAIN2(descr, __FILE__, __LINE__, 0)
#define WVTEST_SLOW_MAIN(descr) WVTEST_MAIN2(descr, __FILE__, __LINE__, 1)


#endif // __WVTEST_H
