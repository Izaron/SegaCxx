#!/usr/bin/env python3
import os
import subprocess
import sys

if __name__ == "__main__":
    for entry in os.scandir(sys.path[0] + "/dumps"):
        path = entry.path
        if path.endswith(".bin"):
            subprocess.run([sys.argv[1], path, path[:-3] + "png"])
