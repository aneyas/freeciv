#!/bin/bash - 
#===============================================================================
#
#          FILE: my_configure.sh
# 
#         USAGE: ./my_configure.sh 
# 
#   DESCRIPTION: 
# 
#       OPTIONS: ---
#  REQUIREMENTS: ---
#          BUGS: ---
#         NOTES: ---
#        AUTHOR: D. Wang (DW), rand_wang@163.com
#  ORGANIZATION: XX
#       CREATED: 01/10/2015 22:09
#      REVISION:  ---
#===============================================================================

set -o nounset                              # Treat unset variables as an error


./configure --enable-aimodules=experimental --enable-shared --disable-nls
