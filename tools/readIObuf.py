#!/usr/bin/env python
import os
import time
import sys

while True:
    core = sys.argv[1]
    cmd = "ybug spin3 < iobuf{}.ybug".format(core)
    os.system(cmd)
    time.sleep(1)    
