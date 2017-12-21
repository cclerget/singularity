/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#ifndef __SINGULARITY_SIGNAL_H_
#define __SINGULARITY_SIGNAL_H_

#include <signal.h>
#include <sys/signalfd.h>

int singularity_wait_signals(sigset_t, siginfo_t *);
void singularity_mask_signals(sigset_t *, sigset_t *);
void singularity_set_parent_death_signal(int);
void singularity_set_signal_handler(int, void (*)(int, siginfo_t *, void *));
int singularity_set_signalfd(sigset_t);
int singularity_wait_signalfd(int, struct signalfd_siginfo *);

#endif
