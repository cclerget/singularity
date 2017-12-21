/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>

#include "config.h"
#include "util/util.h"
#include "util/signal.h"
#include "util/fork.h"
#include "util/proc_notify.h"

void singularity_set_parent_death_signal(int signo) {
    if ( prctl(PR_SET_PDEATHSIG, signo) < 0 ) {
        singularity_message(ERROR, "Failed to set parent death signal\n");
        ABORT(255);
    }

    /* check if parent is alive */
    if ( proc_notify_send(NOTIFY_OK) < 0 ) {
        singularity_message(ERROR, "Parent died, exiting\n");
        ABORT(255);
    }
}

/* Never returns. Will always read from sig_fd and wait for signal events */
int singularity_wait_signals(sigset_t mask, siginfo_t *siginfo) {
    if ( sigwaitinfo(&mask, siginfo) < 0 ) {
        singularity_message(ERROR, "Unable to get siginfo: %s\n", strerror(errno));
        return(-1);
    }
    return(0);
}

void singularity_mask_signals(sigset_t *mask, sigset_t *oldmask) {
    if ( sigprocmask(SIG_SETMASK, mask, oldmask) < 0 ) {
        singularity_message(ERROR, "Unable to mask signals: %s\n", strerror(errno));
        ABORT(255);
    }
    singularity_message(DEBUG, "Signal masked\n");
}

void singularity_set_signal_handler(int signo, void (*action)(int, siginfo_t *, void *)) {
    struct sigaction sa;
    sigset_t empty;
    sigset_t all;
    sigset_t old;

    sigemptyset(&empty);
    sigfillset(&all);

    sa.sa_sigaction = action;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_mask = empty;

    singularity_mask_signals(&all, &old);

    if ( sigaction(signo, &sa, NULL) < 0 ) {
        singularity_message(ERROR, "Failed to set signal handler\n");
        ABORT(255);
    }

    singularity_mask_signals(&old, NULL);
}

int singularity_set_signalfd(sigset_t mask) {
    int fd;

    fd = signalfd(-1, &mask, 0);
    if ( fd < 0 ) {
        return(-1);
    }
    return fd;
}

int singularity_wait_signalfd(int fd, struct signalfd_siginfo *siginfo) {
    if ( read(fd, siginfo, sizeof(struct signalfd_siginfo)) != sizeof(struct signalfd_siginfo) ) {
        return(-1);
    }
    return(0);
}
