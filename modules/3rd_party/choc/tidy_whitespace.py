#!/usr/bin/env python3

# This little script just checks all the files in the choc repo for
# any dodgy whitespace that might have snuck in.

import os

def stripfile(file):
    newContent = ""
    oldContent = ""

    with open (file, 'r') as fin:
        oldContent = fin.read()

    lines = oldContent.splitlines()

    while (len (lines) > 0 and lines[-1].rstrip() == ""):
        lines.pop()

    for l in lines:
        newContent += l.rstrip().replace("\t", "    ") + "\n"

    if (newContent != oldContent):
        print ("Cleaning " + file)
        with open (file, "w") as fout:
           fout.write (newContent)

def stripfolder(folder):
    print ("Checking " + folder)
    for root, dirs, files in os.walk (folder):
        for file in files:
            if (file.endswith (".cpp") or file.endswith (".h")):
                stripfile (os.path.join(root, file))

stripfolder (os.path.dirname (os.path.realpath(__file__)))
