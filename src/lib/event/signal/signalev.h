/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */


#ifndef __SINGULARITY_EVENT_SIGNAL_H_
#define __SINGULARITY_EVENT_SIGNAL_H_

#include "../event.h"

int signal_event_init(struct singularity_event *, pid_t);
int signal_event_call(struct singularity_event *, pid_t);

#endif
