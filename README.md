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

RIOT specific
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

To initiate leader election, run the `elect_leader start` command in the RIOT shell.
