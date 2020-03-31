Michael's Leader Election Project
================
This application run's leader election on RIOT OS nodes.

To do this, the application uses the `shell` and `shell_commands`
modules and all the driver modules each board supports.

`shell` is a very simple interactive command interpreter that can be
used to call functions.  Many of RIOT's modules define some generic
shell commands. These are included via the `shell_commands` module.

Additionally, the `ps` module which provides the `ps` shell command is
included.

Usage
=====

Build, flash and start the application:
```
make all term PORT=your_port
```

The `term` make target starts a terminal emulator for your node. It
connects to a default port so you can interact with the shell.

For hardware, probably: `PORT=/dev/ttyUSB0`. 
For virtual native linux nodes, create taps/tuns and set the `PORT=yourTap` variable in the make statement.


RIOT Shell
==============

The shell commands come with online help. Call `help` to see which commands
exist and what they do.

RIOT Specific
=============

The `ps` command is used to see and analyze all the threads' states and memory
statuses.

Networking
==========

The `ifconfig` command will help you to configure all available network
interfaces. Particularly, note the HWaddr and IPv6 addresses in the output.

Type `ifconfig help` to get an online help for all available options (e.g.
setting the radio channel via `ifconfig 4 set chan 12`).

The `txtsnd` command allows you to send a simple string directly over the link
layer using unicast or multicast. The application will also automatically print
information about any received packet over the serial. This will look like.

My Protocols
==========

To initiate leader election, run the `elect_leader start` command in the RIOT shell. Note that this is not implemented yet; it just starts a custom UDP server and waits.

My Scripts
==========
`mac_topology_gen.py` - my python script that can generate line, ring, tree, and grid topologies while specifying size, broadcast packet loss, incoming packet loss, and RIOT executable files

# Command Line Arguments
--r, default=2, type=int, choices=range(1,27), Number of rows in the grid from 1 to 26 (only used with --t grid)
--c, default=2, type=int, Number of cols in the grid (only used with --t grid)
--s, default=4, type=int, Number of nodes in the network (not used with --t grid)
--t, default="ring", type=str, choices=['ring', 'line', 'binary-tree', 'grid'], The topology to create for this network
--d, default="bi", type=str, choices=['uni','bi'], Uni or bidirectional links (not used with --t grid
--b, default="0.0", type=str, Percentage of broadcast loss given as a string (default "0.0")
--l, default="0.0", type=str, Percentage of packet loss given as a string (default "0.0")
--e, default="", type=str, Address of a compiled RIOT project .elf file to run on all the nodes

# Output
- A topology .XML file intended to be consumed by RIOT's desvirt/vnet tools.
- A cleanup .sh script that will delete all the taps/tuns that desvirt/vnet will create for the network.

`install_topology.sh` - a simple script that moves the output file from the topology generator to the desvirt working directory.
