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

int signal_event_init(pid_t);
void singularity_event_signal_ignore_child(pid_t);

#endif
