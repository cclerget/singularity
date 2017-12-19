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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "./cleanup.h"

#include "util/file.h"
#include "util/registry.h"
#include "util/message.h"
#include "util/config_parser.h"


int cleanup_event_exit(struct singularity_event *event, pid_t child) {
    char *dir = singularity_registry_get("CLEANUPDIR");
    if ( dir ) {
        if ( s_rmdir(dir) < 0 ) {
            singularity_message(WARNING, "Can't delete cleanup dir %s\n", dir);
            return(-1);
        }
    }
    return(0);
}
