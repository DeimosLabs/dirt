#!/bin/sh
echo "#define BUILD_TIMESTAMP \"$(date +%Y%m%d_%H%M%S)\"" > "$1"
