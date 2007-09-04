#!/usr/bin/python
#
# This needs to be run from the top of the tree!

import sys
import os
import string

def confirm(_prompt=None, _default=False):
    promptstr = _prompt
    if (not promptstr):
        promptstr = "Confirm"

    if (_default):
        prompt = "%s [Y/n]: " % promptstr
    else:
        prompt = "%s [y/N]: " % promptstr

    while (True):
        ans = raw_input(prompt)
        if (not ans):
            return _default
        if ((ans == "y") or (ans == "Y")):
            return True
        if ((ans == "n") or (ans == "N")):
            return False

        print "Please respond with 'y' or 'n'."

def find_coyotos_top():
    # Find the root of the coyotos tree:
    cwdlist = os.getcwd().split('/')

    while len(cwdlist) >= 2 and cwdlist[-2:] != ['coyotos', 'src']:
        cwdlist.pop()

    if len(cwdlist) < 2:
        return None

    cwdlist.pop()
    cwdlist.pop()

    return string.join(cwdlist, '/')


top = find_coyotos_top()
if (top == None):
    print "Coyotos tree root not found."
    sys.exit(1)

trees = [
    ("coyotos/src/tutorial", "coyotos tutorials", "newcomers", True),
    ("coyotos/src/ccs-xenv", "cross-compilation tools", "experts only", False)
    ]

for (tree, descrip, recommend, dflt) in trees:
    treepath = os.path.join(top, tree)
    treetip = os.path.basename(treepath)
    url = "http://dev.eros-os.com/hg/" + treetip

    #print treepath, treetip, url

    print
    if (os.path.lexists(treepath)):
        print "%s is already present" % tree
        continue
    
    print """%s provides %s.
    This subtree is recommended for %s""" % (tree, descrip, recommend)
    if confirm("    Fetch it?", dflt):
        print
        os.spawnvp(os.P_WAIT, 'echo', ['echo', 'hg', 'clone', url, treepath ])
        print
        os.spawnvp(os.P_WAIT, 'hg', ['hg', 'clone', url, treepath ])
