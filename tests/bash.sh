#!/bin/bash
export LD_PRELOAD=../cmake-build-debug/liblocalflock.so
echo "Starting new Shell. CTRL-D to exit!"
bash -l

