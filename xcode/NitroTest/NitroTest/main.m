//
//  main.m
//  NitroTest
//
//  Created by Seth Raphael on 6/7/13.
//  Copyright (c) 2013 Bump. All rights reserved.
//


#import <UIKit/UIKit.h>

#import "BMPAppDelegate.h"

#include "nitro.h"
#include <unistd.h>

static int MESSAGES;
static int SIZE;

struct test_state {
    nitro_socket_t *s_r;
    nitro_socket_t *s_s;
    double start;
    double end;
};

void *do_recv(void *baton) {
    struct test_state *ts = (struct test_state *)baton;
    nitro_socket_t *s = ts->s_r;
    
    int i;
    for (i=0; i < MESSAGES; ++i) {
        nitro_frame_t *fr = nitro_recv(s, 0);
        nitro_frame_destroy(fr);
    }
    struct timeval mark;
    gettimeofday(&mark, NULL);
    ts->end = ((double)mark.tv_sec +
               (mark.tv_usec / 1000000.0));
    
    return NULL;
}

void *do_send(void *baton) {
    struct test_state *ts = (struct test_state *)baton;
    nitro_socket_t *s = ts->s_s;
    int i = 0;
    
    char *buf = alloca(SIZE);
    bzero(buf, SIZE);
    
    sleep(1);
    nitro_frame_t *out = nitro_frame_new_copy(buf, SIZE);
    
    struct timeval mark;
    
    gettimeofday(&mark, NULL);
    for (i=0; i < MESSAGES; ++i) {
        nitro_send(&out, s, NITRO_REUSE);
    }
    
    nitro_frame_destroy(out);
    
    ts->start = ((double)mark.tv_sec +
                 (mark.tv_usec / 1000000.0));
    return NULL;
}

void print_report(struct test_state *ts, char *name) {
    double delt = ts->end - ts->start;
    
    fprintf(stderr, "{%s} %d messages in %.3f seconds (%d/s)\n",
            name, MESSAGES, delt, (int)(MESSAGES / delt));
}
void * do_test(void * nothing) {
    MESSAGES = 100000;
    SIZE = 40;
    nitro_runtime_start();
    
    nitro_socket_t *r = nitro_socket_bind("tcp://127.0.0.1:4444", NULL);
    nitro_socket_t *c = nitro_socket_connect("tcp://127.0.0.1:4444", NULL);
    //
    pthread_t t1, t2;
    void *res;
    struct test_state test_1 = {r, c, 0, 0};
    
    pthread_create(&t1, NULL, do_recv, &test_1);
    pthread_create(&t2, NULL, do_send, &test_1);
    pthread_join(t1, &res);
    pthread_join(t2, &res);
    
    print_report(&test_1, "tcp");
    
    sleep(1);
    nitro_socket_close(r);
    nitro_socket_close(c);
    
//    nitro_sockopt_t *opt = nitro_sockopt_new();
//    nitro_sockopt_set_secure(opt, 1);
//    nitro_sockopt_t *opt2 = nitro_sockopt_new();
//    nitro_sockopt_set_secure(opt2, 1);
//    
//    r = nitro_socket_bind("tcp://127.0.0.1:4445", opt);
//    c = nitro_socket_connect("tcp://127.0.0.1:4445", opt2);
//    
//    struct test_state test_2 = {r, c, 0, 0};
//    
//    pthread_create(&t1, NULL, do_recv, &test_2);
//    pthread_create(&t2, NULL, do_send, &test_2);
//    pthread_join(t1, &res);
//    pthread_join(t2, &res);
//    
//    print_report(&test_2, "tcp-secure");
//    sleep(1);
//    
//    nitro_socket_close(r);
//    nitro_socket_close(c);
//    
    r = nitro_socket_bind("inproc://foobar", NULL);
    c = nitro_socket_connect("inproc://foobar", NULL);
    
    struct test_state test_3 = {r, c, 0, 0};
    
    pthread_create(&t1, NULL, do_recv, &test_3);
    pthread_create(&t2, NULL, do_send, &test_3);
    pthread_join(t1, &res);
    pthread_join(t2, &res);
    
    print_report(&test_3, "inproc");
    
    nitro_socket_close(r);
    nitro_socket_close(c);
    
    nitro_runtime_stop();
    return NULL;

}
int main(int argc, char **argv) {
    
//    if (argc != 3) {
//        fprintf(stderr, "two arguments: MESSAGE_COUNT MESSAGE_SIZE\n");
//        return -1;
//    }
    
    pthread_t p;

    pthread_create(&p, NULL, do_test, NULL);
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([BMPAppDelegate class]));
    }
}

//
//#import <UIKit/UIKit.h>
//
//#import "BMPAppDelegate.h"
//
//int main(int argc, char *argv[])
//{
//    @autoreleasepool {
//        return UIApplicationMain(argc, argv, nil, NSStringFromClass([BMPAppDelegate class]));
//    }
//}
