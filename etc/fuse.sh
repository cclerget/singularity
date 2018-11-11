#!/bin/sh

#####################################################
#                                                   #
# binary: the path to fuse binary server            #
# destination: destination mount point in container #
# options: options to pass to fuse server           #
# privileged: 0 or 1                                #
#                                                   #
#####################################################

binary="/path/to/fuse/binary"
options="-o allow_other"
privileged=0

#########################################
# after this line should not be touched #
#########################################

if [ "$#" -eq 0 ]; then
    exit $privileged
fi

exec $binary $1 -f $options
