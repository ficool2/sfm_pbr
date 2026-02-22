# SFM PBR

Plugin that adds a true PBR shader to Source Filmmaker, based on the PBR shader made by the [Zombie Master: Reborn](https://github.com/zm-reborn) team.

Check out the [Steam Workshop page](https://steamcommunity.com/sharedfiles/filedetails/?id=3671463307)!

Compared to ZMR's implementation, there is additional fixes for SFM compatibility, as well as new additions such as MRAO factor parameters.

This repository is a stripped version of the [Alien Swarm SDK](https://github.com/Nican/swarm-sdk).

## Building
	
To build the plugin, open the .sln in Visual Studio 2022 or newer and build. Place the compiled DLL into SFM's `addons` folder.

To build the shaders, run the `buildsfmshaders.bat` in `src/materialsystem/stdshaders`. Place the compiled FXC files into SFM's `shaders/fxc/` folder.