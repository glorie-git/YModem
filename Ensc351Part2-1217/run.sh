#!/bin/bash

# Copyright (c) 2020 W. Craig Scratchley

#have the program run on CPU 0 at realtime priority 99
taskset -c 0 chrt 99 Debug/Ensc351Part2-1217
