Michael's Leader Election Project
================
This application run's leader election on RIOT OS nodes.

To do this, the application uses the `shell` and `shell_commands` modules and all the driver modules each board supports.

`shell` is a very simple interactive command interpreter that can be used to call functions.  Many of RIOT's modules define some generic shell commands. These are included via the `shell_commands` module.

Usage
=====

Build, flash and start the application:
```
make all term PORT=your_port
```

The `term` make target starts a terminal emulator for your node. It connects to a default port so you can interact with the shell.

For hardware, probably: `PORT=/dev/ttyUSB0`. 
For virtual native linux nodes, create taps/tuns and set the `PORT=yourTap` variable in the make statement.


RIOT Shell
==============

The shell commands come with online help. Call `help` to see which commands exist and what they do.

RIOT Specific
=============

The `ps` command is used to see and analyze all the threads' states and memory statuses.

Networking
==========

The `ifconfig` command will help you to configure all available network interfaces. Particularly, note the HWaddr and IPv6 addresses in the output.

Type `ifconfig help` to get an online help for all available options (e.g. setting the radio channel via `ifconfig 4 set chan 12`).

The `txtsnd` command allows you to send a simple string directly over the link layer using unicast or multicast. The application will also automatically print information about any received packet over the serial. This will look like.

My Protocols
==========

To initiate leader election, run the `elect_leader start` command in the RIOT shell. Note that this is not implemented yet; it just starts a custom UDP server and waits.

My Scripts
==========
1. `mac_topology_gen.py` - my python script that can generate line, ring, tree, and grid topologies while specifying size, broadcast packet loss, incoming packet loss, and RIOT executable files

### Command Line Arguments
--r, default=2, type=int, choices=range(1,27), Number of rows in the grid from 1 to 26 (only used with --t grid)  
--c, default=2, type=int, Number of cols in the grid (only used with --t grid)  
--s, default=4, type=int, Number of nodes in the network (not used with --t grid)  
--t, default="ring", type=str, choices=['ring', 'line', 'binary-tree', 'grid'], The topology to create for this network  
--d, default="bi", type=str, choices=['uni','bi'], Uni or bidirectional links (not used with --t grid)  
--b, default="0.0", type=str, Percentage of broadcast loss given as a string (default "0.0")  
--l, default="0.0", type=str, Percentage of packet loss given as a string (default "0.0")  
--e, default="", type=str, Address of a compiled RIOT project .elf file to run on all the nodes  

### Output
- A topology .XML file intended to be consumed by RIOT's desvirt/vnet tools.  
- A cleanup .sh script that will delete all the taps/tuns that desvirt/vnet will create for the network.

2. `install_topology.sh` - a simple script that moves the output file from the topology generator to the desvirt working directory.

Sample Use
==========

```
> python mac_topology_gen.py --s 5 --t ring --d uni --l 20.0 --e /my/project/binary.elf
Created uni-ring5.xml
Created uni-ring5_cleanup.sh
> ./install_topology.sh uni-ring5.xml
uni-ring5.xml has been installed for desvirt to use
> TOPO=uni-ring5 make desvirt-define
cd /home/michael/Documents/Software/RIOT/dist/tools/desvirt/desvirt && ./vnet -d /home/michael/Documents/Software/RIOT/dist/tools/desvirt/desvirt/.desvirt/ -n uni-ring5
vnet           : Loaded statefile .desvirt/lib/uni-ring5.macs.
vnet           : Network Name: uni-ring5
vnet           : Setting up virtual topology uni-ring5...
> TOPO=uni-ring5 make desvirt-start
cd /path/to/RIOT/dist/tools/desvirt/desvirt && ./vnet -s -n uni-ring5
...output from creating node taps
...output from defining node links/neighbors
...output from initiating RIOT processes
```
> make desvirt-list
cd /path/to/RIOT/dist/tools/desvirt/desvirt && ./vnet -l
Network Name         State
----------------------------
uni-ring5            running
> TOPO=uni-ring5 make desvirt-stop
cd /path/to/RIOT/dist/tools/desvirt/desvirt && ./vnet -q -n uni-ring5
vnet           : Loaded statefile .desvirt/lib/uni-ring5.macs.
vnet           : Loaded statefile .desvirt/lib/uni-ring5.taps.
vnet           : Loaded statefile .desvirt/lib/uni-ring5.pids.
vnet           : Network Name: uni-ring5
vnet           : Shutting down bridge and links...
vnet           : Shutting down nodes...
riotnative     : Kill the RIOT: /my/project/binary.elf (5996)
> TOPO=uni-ring5 make desvirt-undefine
cd /path/to/RIOT/dist/tools/desvirt/desvirt && ./vnet -u /path/to/RIOT/dist/tools/desvirt/desvirt/.desvirt/ -n uni-ring5
vnet           : Loaded statefile .desvirt/lib/uni-ring5.macs.
vnet           : Loaded statefile .desvirt/lib/uni-ring5.taps.
vnet           : Loaded statefile .desvirt/lib/uni-ring5.pids.
vnet           : Network Name: uni-ring5
vnet           : Undefining network...
vnet           : Done.
> ./uni-ring5_cleanup.sh

