# REL & DOL Loader plugins for IDA Pro

The REL and DOL files are found in Nintendo Gamecube/Wii games. This repository is focused on loading the REL files.

## Environment
* Visual C++ 2010 Express
* IDA Pro 6.1 SDK

## DOL Loader
A fork of the DOL loader by Stefan Esser, source from [here](http://hitmen.c02.at/html/gc_tools.html).

### Changes
* Currently none (will consider bug fixes)

## REL Loader
A rewrite/fork of the RSO loader by Stephen Simpson, source from [here](https://github.com/Megazig/rso_ida_loader).

### Features
* Creates segments/sections (.text, .data, .bss)
* Strips loader data from the binary
* Identifies functions (prolog, epilog, unresolved)

### Planned (TODOs)
* Place header information at the top
* Load other relocations and treat them as imports (?)

