# realice

engine todo:
- [x] support FAKK2 BSP version 12 maps
- [x] support Alice BSP version 42 maps
- [ ] (huge) add tiki model support, some part of it could be probably taken from OpenMOHAA
- [ ] (tedious) remove vm syscalls, as fakk2 directly calls the DLL functions like Quake2 or Half-Life
- [ ] (huge) integrate fakk2-sdk libraries (they are not under GPL btw but probably nobody cares anymore as Ritual imploded)
- [ ] get rid of legacy unix/win32/macos code, port to SDL3

renderer todo:
- [ ] support FAKK2 BSP version 12 maps, in progress
- [ ] support Alice BSP version 42 maps, in progress
- [x] implement missing shader options (see shader_manual_new.doc from fakk2-sdk)
- [x] implement multitexturing path (if mtex in shaders)
- [ ] implement actual drawing of these shader options (in progress)

UI todo:
- [ ] because renderer now misses stuff like setcolor/drawstretch, ALL 2D rendering is broken
	- [ ] port UI lib from openmohaa?

How to run:
What I basically do at this point is copy selected files from FAKK2 or Alice game data into Q3A folder,
this way I don't have to implement all stuff at once by relying on Q3A assets temporarily.
