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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>

#include "config.h"
#include "util/util.h"
#include "util/signal.h"
#include "util/fork.h"
#include "util/proc_notify.h"

static sigset_t old_mask;
static sigset_t sig_mask;

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

static void handle_sig_sigchld(siginfo_t *siginfo) {
    while(1) {
        if ( waitpid(-1, NULL, WNOHANG) <= 0 ) break;
    }
}

static void handle_sig_generic(siginfo_t *siginfo) {
    singularity_message(DEBUG, "Generic sig received: %d\n", siginfo->si_signo);
    if ( siginfo->si_signo != SIGALRM && siginfo->si_signo != SIGCONT ) {
        kill(-1,  siginfo->si_signo);
    }
}

void singularity_install_signal_handler() {
    singularity_message(DEBUG, "Creating signal handler\n");

    sigfillset(&sig_mask);

    if ( -1 == sigprocmask(SIG_SETMASK, &sig_mask, &old_mask) ) {
        singularity_message(ERROR, "Unable to block signals: %s\n", strerror(errno));
        ABORT(255);
    }
}

/* Never returns. Will always read from sig_fd and wait for signal events */
int singularity_handle_signals(siginfo_t *siginfo) {
    if ( sigwaitinfo(&sig_mask, siginfo) < 0 ) {
        singularity_message(ERROR, "Unable to get siginfo: %s\n", strerror(errno));
        return(-1);
    }

    if ( siginfo->si_signo == SIGCHLD ) {
        handle_sig_sigchld(siginfo);
    } else {
        handle_sig_generic(siginfo);
    }
    return(0);
}

void singularity_unblock_signals() {
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}
