# SdFat Volume Explorer

Simple shell that I created to help me debugging file contents during app development.

**Disclaimer**

This code have been barely tested and you should not use it if you care about the content of your SD cards.

**Requirements**

You must have SdFat library installed : https://github.com/greiman/SdFat

## Facts

* single volume
* wildcards supported on rm and cp src
* relatives path supported on all commands : yes should be able to cd /d1/d2/d3 and cp ../* ../../../d4 - With a bit of luck, it could work !

## Commands

**Changing directory**

*cd path*

**Creating a directory**

*mkdir path*

intermediate directories will not be created

**Removing a directory**

*rmdir path*

path must be empty, rmdir is NOT recursive

**Renaming a file**

*mv filename new_filename*

**Deleting a file**

*rm filename*

wildcards are allowed, you'll be warned before execution

**Copying file**

*cp src dest*

src can use wildcards

if using wildcards dest MUST BE a directory

destination files will be overwritten without warning

**Creating empty file**

*touch filename*

**Dumping hex content**

*dump filename*

Display HEX and ASCII file's content with a 8 cols layout

**Viewing text file**

*cat filename*

## XModem transferts

Xmodem communication is not supported under Arduino IDE serial monitor, you must use a standalone terminal application.
Only the most minimal and simplistic version of the protocol is supported.
Which means :

- 128 bytes buffer
- 8 bits checksum

To initiate a file transfert two commands are at your disposal

*recv filename*

and

*send filename*

Once the command have been issued you must close your terminal session (ctrl/c with tycmd monitor, ctrl/a k for screen, and so on)

Then launch the sx or rx command like

*rx filename > /dev/mydevice < /dev/mydevice*

or

*sx filename > /dev/mydevice < /dev/mydevice*

Once the transfert is done, you can relaunch the terminal session, if everything went right the shell will be responsive.

## How to use FileExplorer ?

Copy the .c, .cpp and .h files in your project's directory.

```
#include "volume_explorer.h"

VolumeExplorer explorer(&Serial);

void setup() {
    while(!Serial())
    ;
    explorer.init();
}

void loop() {
    explorer.update();
}
```

## Misc Informations

- Issuing a ctrl/q will stop volume explorer to consumme data from Serial (or any Stream)
- Some #defines in volume_explorer.h can be useful to disable XMODEM or the use of ANSI codes (for terminal cursor back pos)
- This code has only been "tested" on Teensy 3.6, code size might not fit on platforms where Flash & Ram are too much limited
- There's no strings size checks, default path is 256 long, be careful.
- Under Arduino / TeensyDuino IDE serial monitor set the line ending setting to "carriage return"
- Should work with any terminal app (tested with iTerm2 + tycmd monitor - also tested with screen) - I did not tried with Linux but there's not reason it would not work.


**vintage shells are so cool ;-)
enjoy !**
