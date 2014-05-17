# REL & DOL Loader plugins for IDA Pro

The REL and DOL files are found in Nintendo Gamecube/Wii games. This repository is focused on loading the REL files.

## Environment
* Visual C++ 2010 Express
* IDA Pro 6.1 SDK

## DOL Loader
A fork of the DOL loader by Stefan Esser, source from [here](http://hitmen.c02.at/html/gc_tools.html).

### Changes
* Fixed a bug where section sizes would be one less than their actual size.
* The default compiler is now set to GNU gcc.

## REL Loader
A rewrite/fork of the RSO loader by Stephen Simpson, source from [here](https://github.com/Megazig/rso_ida_loader).

### Features
* Creates segments/sections (.text, .data, .bss).
* Strips loader data from the binary.
* Identifies exported functions (prolog, epilog, unresolved).
* Treats relocations to external modules as imports.
* Reads other modules in the same folder as the target module to map ids to names and obtain correct import offsets.


### Planned (TODOs)
* Read exported `.map` files to give meaningful names to externals.
* Make imports appear in the imports tab.
* Allow some settings such as relocating to any base (?).
