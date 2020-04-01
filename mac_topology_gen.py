# Author: Michael Conard
# Date: March 2020
# Purpose: Generate RIOT network topologies to be consumed by desvirt/vnet.

import argparse
import math
import os
import stat

# PARSING RULES
parser = argparse.ArgumentParser(description='Parsing for Michael\'s grid generator.')
parser.add_argument("--r", default=2, type=int, choices=range(1,27), help="Number of rows in the grid from 1 to 26 (only used with --t grid)")
parser.add_argument("--c", default=2, type=int, help="Number of cols in the grid (only used with --t grid)")
parser.add_argument("--s", default=4, type=int, help="Number of nodes in the network (not used with --t grid)")
parser.add_argument("--t", default="ring", type=str, choices=['ring', 'line', 'binary-tree', 'grid'], help="The topology to create for this network")
parser.add_argument("--d", default="bi", type=str, choices=['uni','bi'], help="Uni or bi-directional links (not used with --t grid")
parser.add_argument("--b", default="0.0", type=str, help="Percentage of broadcast loss given as a string (default \"0.0\")")
parser.add_argument("--l", default="0.0", type=str, help="Percentage of packet loss given as a string (default \"0.0\")")
parser.add_argument("--e", default="", type=str, help="Address of a compiled RIOT project .elf file to run on all the nodes")

# FUNCTIONS
def addNode(f, name, binary):
    f.write("            <node binary=\"%s\" name=\"%s\" type=\"riot_native\"/>\n" % (binary, name))
    return;

def addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, uni):
    f.write("            <link broadcast_loss=\"%s\" from_if=\"wlan0\" from_node=\"%s\" loss=\"%s\" to_if=\"wlan0\" to_node=\"%s\" uni=\"%s\"/>\n" % (blos, fromNode, loss, toNode, uni))
    existingLinks.append((fromNode,toNode))
    if uni == "false":
        existingLinks.append((toNode,fromNode))
    return;

def generateNodes(f, topo, size, binary="", rows=None, cols=None, treeNodes=None):
    letter = 97
    if topo == "grid":
        for x in range(0,rows):
            for y in range(0,cols):
                ident = str(chr(letter+x)) + str(y)
                addNode(f, ident, binary)
    elif topo == "ring" or topo == "line":
        for x in range(0,size):
            addNode(f, str(x), binary)
    elif topo == "binary-tree":
        depth = int(math.log(size+1,2))
        addNode(f, "root", binary)
        count = 1;
        for x in range(2,depth+2):
            for y in range(0,pow(2,x-1)):
                ident = str(chr(letter+x-2)) + str(y)
                if count < size:
                    addNode(f, ident, binary)
                    treeNodes.append(ident)
                    count = count+1
                else:
                    break
            if count >= size:
                break
    return;

def assignGridNeighbors(f, rows, cols, blos, loss, uni):
    existingLinks = []
    letter = 97
    for x in range(0,rows):
        for y in range(0,cols):
            fromNode = str(chr(letter+x)) + str(y)

            if x-1 >= 97 and x-1 <= 97+rows:     # neighbor above me
                toNode = str(chr(letter+x-1)) + str(y)
                if (fromNode,toNode) not in existingLinks:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, "false")

            if y+1 >= 0 and y+1 <= cols:         # neighbor to my right
                toNode = str(chr(letter+x)) + str(y+1)
                if (fromNode,toNode) not in existingLinks:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, "false")

            if x+1 >= 97 and x+1 <= 97+rows:     # neighbor below me
                toNode = str(chr(letter+x+1)) + str(y)
                if (fromNode,toNode) not in existingLinks:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, "false")

            if y-1 >= 0 and y-1 <= cols:         # neighbor to my left
                toNode = str(chr(letter+x)) + str(y-1)
                if (fromNode,toNode) not in existingLinks:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, "false")
    return;

# PARSING
args = parser.parse_args()
rows = args.r
cols = args.c
size = args.s
topo = args.t
dire = args.d
blos = args.b
loss = args.l
riot = args.e

numNodes = rows * cols  # number of grid nodes
letter = 97             # start of the alphabet

# DIRECTIONALITY
if dire == "bi":
    uni = "false"
else:
    uni = "true"

# NETWORK/FILE NAME
if topo == "grid":
    name = str(numNodes) + "grid" + str(rows) + "x" + str(cols)
    description = str(numNodes) + " nodes in a regular " + str(rows) + "x" + str(cols) + " grid"
elif topo == "ring":
    if dire == "uni":
        name = "uni-ring" + str(size)
        description = str(size) + " nodes in a uni-directional ring"
    else:
        name = "bi-ring" + str(size)
        description = str(size) + " nodes in a bi-directional ring"
elif topo == "binary-tree":
    if dire == "uni":
        name = "uni-tree" + str(size)
        description = str(size) + " nodes in a uni-directional binary-tree (root down)"
    else:
        name = "bi-tree" + str(size)
        description = str(size) + " nodes in a bi-directional binary-tree"
elif topo == "line":
    name = "line" + str(size)
    description = str(size) + " nodes in a line"
fName = name + ".xml"

# CREATE FILE
f = open(fName,"w+")

# WRITE INITIAL XML INFO
f.write("""<?xml version="1.0" encoding="UTF-8"?>
<topology version="1">
    <net description="%s" name="%s">
        <nodeTypes>
            <nodeType name="riot_native">
                <interfaces>
                    <interface name="wlan0" type="802.11bg"/>
                </interfaces>
            </nodeType>
        </nodeTypes>
        <nodes>\n""" % (description, name));

# GENERATE NODES 
treeNodes = []
generateNodes(f, topo, size, riot, rows, cols, treeNodes)

f.write("""        </nodes>
        <links>\n""")

# START NEIGHBOR ASSIGNMENT LOOPS

if topo == "grid":
    assignGridNeighbors(f, rows, cols, blos, loss, uni)
elif topo == "ring":
    existingLinks = []
    for x in range(0,size):
        fromNode = str(x)
        toNode = str(x+1)
        if x+1 == size:
            toNode = str(0)
        addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, uni)   
elif topo == "binary-tree":
    existingLinks = []
    depth = int(math.log(size+1,2))
    count = 1
    if size >= 2:
        addNeighbor(f, existingLinks, blos, "root", loss, "a1", uni)
    if size>= 3:
        addNeighbor(f, existingLinks, blos, "root", loss, "a2", uni)
    if size>= 4:
        for x in range(2,depth+1):
            for y in range(0,pow(2,x-1)):
                fromNode = str(chr(letter+x-2)) + str(y)
                leftChild = str(chr(letter+x-1)) + str(y*2)
                rightChild = str(chr(letter+x-1)) + str(y*2+1)
                if leftChild in treeNodes:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, leftChild, uni)
                if rightChild in treeNodes:
                    addNeighbor(f, existingLinks, blos, fromNode, loss, rightChild, uni)
                count = count+1
                if count >= size:
                    break
            if count >= size:
                break
elif topo == "line":
    existingLinks = []
    for x in range(0,size):
        fromNode = str(x)
        toNode = str(x+1)
        if x+1 != size:
            addNeighbor(f, existingLinks, blos, fromNode, loss, toNode, uni) 


# END NEIGHBOR ASSIGNMENT LOOPS

# WRITE CLOSING XML INFO
f.write("""        </links>
    </net>
</topology>""")

f.close()

# GENERATE CLEAN-UP SCRIPT
cName = name + "_cleanup.sh"
f = open(cName, "w+")

f.write("sudo ip link delete %s\n" % name)
if topo == "grid":
    for x in range(0,rows):
        for y in range(0,cols):
            ident = str(chr(letter+x)) + str(y)
            f.write("sudo ip link delete %s_%s\n" % (name,ident))
elif topo == "ring" or topo == "line":
    for x in range(0,size):
        f.write("sudo ip link delete %s_%s\n" % (name,str(x)))
elif topo == "binary-tree":
    depth = int(math.log(size+1,2))
    f.write("sudo ip link delete %s_%s\n" % (name,"root"))
    count = 1;
    for x in range(2,depth+2):
        for y in range(0,pow(2,x-1)):
            ident = str(chr(letter+x-2)) + str(y)
            if count < size:
                f.write("sudo ip link delete %s_%s\n" % (name,ident))
                count = count+1
            else:
                break
        if count >= size:
            break
f.write("make desvirt-undefine TOPO=%s\n" % name)

f.close()
st = os.stat(cName)
os.chmod(cName, st.st_mode | stat.S_IEXEC)

print("Created %s" % (fName))
print("Created %s\n" % (cName))

