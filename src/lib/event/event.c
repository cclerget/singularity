/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>

#include "util/util.h"
#include "util/registry.h"
#include "util/message.h"
#include "util/config_parser.h"

#include "./signal/signalev.h"
#include "./cleanup/cleanup.h"
#include "./notify/notify.h"

static int epfd;

struct singularity_event events[] = {
    {
        .name   = "signal",
        .init   = signal_event_init,
        .call   = signal_event_call,
        .exit   = NULL,
        .fd     = -1,
    },{
        .name   = "cleanup",
        .init   = NULL,
        .call   = NULL,
        .exit   = cleanup_event_exit,
        .fd     = -1,
    },{
        .name   = "notify",
        .init   = notify_event_init,
        .call   = notify_event_call,
        .exit   = notify_event_exit,
        .fd     = -1,
    }
};

int singularity_event_init(pid_t child) {
    int nbevents = (sizeof(events) / sizeof(struct singularity_event)) - 1;

    epfd = epoll_create(1);
    if ( epfd < 0 ) {
        singularity_message(ERROR, "Failed to create event queue\n");
        return(-1);
    }

    for ( ; nbevents >= 0; nbevents-- ) {
        struct epoll_event ee;
        char *name = events[nbevents].name;

        if ( events[nbevents].init == NULL ) {
            continue;
        }
        if ( events[nbevents].init(&events[nbevents], child) < 0 ) {
            singularity_message(ERROR, "Failed to call init for %s event\n", name);
            return(-1);
        }
        if ( events[nbevents].fd >= 0 ) {
            ee.events = EPOLLIN;
            ee.data.ptr = (void *)&events[nbevents];
            if ( epoll_ctl(epfd, EPOLL_CTL_ADD, events[nbevents].fd, &ee) < 0 ) {
                singularity_message(ERROR, "Failed to add %s to event queue\n", name);
                return(-1);
            }
        }
    }
    return(0);
}

int singularity_event_call(pid_t child) {
    struct epoll_event ev;
    struct singularity_event *sev = NULL;
    int retval;

    if ( epoll_wait(epfd, &ev, 1, -1) < 0 ) {
        singularity_message(ERROR, "Failed to wait on event queue\n");
        return EVENT_FAIL(255);
    }
    sev = (struct singularity_event *)ev.data.ptr;
    
    retval = sev->call(sev, child);

    /* a call which return -1 is automatically discarded from event queue */
    if ( retval < 0 ) {
        if ( epoll_ctl(epfd, EPOLL_CTL_DEL, sev->fd, NULL) < 0 ) {
            singularity_message(ERROR, "Failed to remove %s event from event queue\n", sev->name);
            return EVENT_FAIL(255);
        }
        singularity_message(DEBUG, "Discard %s event from event queue\n", sev->name);
        return(0);
    }
    return(retval);
}

int singularity_event_exit(pid_t child) {
    int nbevents = (sizeof(events) / sizeof(struct singularity_event)) - 1;

    for ( ; nbevents >= 0; nbevents-- ) {
        if ( events[nbevents].exit == NULL ) {
            continue;
        }
        if ( events[nbevents].exit(&events[nbevents], child) < 0 ) {
            singularity_message(ERROR, "Failed to call exit for %s event\n", events[nbevents].name);
            return(-1);
        }
    }
    return(0);
}
