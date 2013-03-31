#ifndef NITRO_TEST_H
#define NITRO_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char buffer[5000];
static int l;
static int test_count;

#define FAILCODE (isatty(2) ? "\x1b[31m" : "")
#define ENDCODE (isatty(2) ? "\x1b[0m" : "")
#define OKAYCODE (isatty(2) ? "\x1b[32m" : "")
#define INFOCODE (isatty(2) ? "\x1b[34m" : "")

#define PRINTNAME(name) {\
    snprintf(buffer, 5000, "   {%s:%d:\"%s\"} ", __FILE__, __LINE__, name);\
    l = strlen(buffer);\
    fprintf(stderr, "%s", buffer);\
    while (65 - l > 0) {\
        fprintf(stderr, ".");\
        l++;\
    }\
}

#define SUMMARY(code) {\
    fprintf(stderr, "%s[%d tests]%s\n", INFOCODE, test_count, ENDCODE);\
    if (code) {\
        fprintf(stderr, "%sAborting on failure.%s\n", FAILCODE, ENDCODE);\
        exit(code);\
    } else {\
        fprintf(stderr, "%sAll tests passed.%s\n", OKAYCODE, ENDCODE);\
        exit(code);\
    }\
}

#define TEST(name, cond) {\
    test_count++;\
    if (!(cond)) {\
        PRINTNAME(name);\
        fprintf(stderr, " %sFAIL%s\n", FAILCODE, ENDCODE);\
        SUMMARY(1);\
    } else {\
        PRINTNAME(name);\
        fprintf(stderr, " %sOKAY%s\n", OKAYCODE, ENDCODE);\
    }\
}

#endif /* NITRO_TEST_H */
