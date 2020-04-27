#!/bin/bash
export LD_PRELOAD=../cmake-build-debug/liblocalflock.so
export LOCALFLOCK_DEBUG=TRUE
echo "Starting new Shell. CTRL-D to exit!"
bash -l
