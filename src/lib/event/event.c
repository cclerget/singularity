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

static int epfd = -1;

static struct singularity_event_queue *seqhead = NULL;
static struct singularity_event_queue *seqtail = NULL;

int singularity_event_register(struct singularity_event *event) {
    struct epoll_event ee;
    char *name = event->name;
    struct singularity_event_queue *seq;

    if ( event == NULL ) {
        return(-1);
    }

    seq = (struct singularity_event_queue *)malloc(sizeof(struct singularity_event_queue));
    if ( seq == NULL ) {
        return(-1);
    }

    singularity_message(DEBUG, "Registering queue event %s\n", event->name);

    if ( event->fd >= 0 ) {
        ee.events = EPOLLIN;
        ee.data.ptr = (void *)event;
        if ( epoll_ctl(epfd, EPOLL_CTL_ADD, event->fd, &ee) < 0 ) {
            singularity_message(ERROR, "Failed to add %s to event queue\n", name);
            return(-1);
        }
    }

    seq->event = event;
    seq->next = NULL;

    if ( seqtail != NULL ) {
        seqtail->next = seq;
    }
    if ( seqhead == NULL ) {
        seqhead = seq;
        seqhead->prev = NULL;
    } else {
        seq->prev = seqtail;
    }
    seqtail = seq;

    return(0);
}

int singularity_event_init(pid_t child) {
    int retval = 0;

    epfd = epoll_create(1);
    if ( epfd < 0 ) {
        singularity_message(ERROR, "Failed to create event queue\n");
        return(-1);
    }

    retval += signal_event_init(child);
    retval += cleanup_event_init(child);
    retval += notify_event_init(child);

    if ( retval != 0 ) {
        singularity_message(ERROR, "Event queue init failed\n");
        return(-1);
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

    retval = sev->call(child);

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
    struct singularity_event_queue *current = seqhead;

    for ( current = seqhead; current != NULL; current = current->next ) {
        if ( current->event->exit && current->event->exit(child) < 0 ) {
            singularity_message(ERROR, "Failed to call exit for %s event\n", current->event->name);
        }
    }
    return(0);
}
