#! /usr/bin/env python3
# -*- coding: utf-8 -*-
import sys

line = sys.stdin.readline().strip()
while (line):
    if line[0:6] != "$L6FRM":
        line = sys.stdin.readline().strip()
        continue
    t_data = line.split(',')[5]
    sys.stdout.buffer.write(bytes.fromhex(t_data))
    sys.stdout.flush()
    line = sys.stdin.readline().strip()
