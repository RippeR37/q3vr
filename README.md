# Quake 3 VR

**Quake 3 VR** (a.k.a. `q3vr`) is a PCVR port of Quake 3 Arena game based on:

* `ioquake3` - community maintained fork of idTech3 engine,
* `Quake3Quest` - port of `ioquake3` to Quest 2/3 mobile headsets.

It includes several bugfixes, improvements and QoL features built specifically
for VR.

### Main features

* Full Single-Player campaign
  * including full 6DoF support
* Multiplayer support
  * including crossplay with PC and Quest players
  * support for playing on servers with _simpler_ mods like OSP, unfreeze, CA,
    etc.
* Custom Virtual Screen for displaying 2D content
* Lots of VR comfort and QoL features including:
  * full haptic effects support
  * weapon selection wheel
  * zoom with reticle for railgun
  * comfort options like vignette, height adjustements, weapon lasers, etc.

> [!CAUTION]
> Please bear in mind that this port is currently in **beta** state, which
> means that there may be some bugs and compatibility issues that will be fixed
> over time.


## How to play

To download and set up the game:

* Go to [Releases](https://github.com/RippeR37/q3vr/releases) page, download the
  most recent release and install or extract it to directory of your choice
* Download [latest Release Point (v1.32) PAK files](https://files.ioquake3.org/quake3-latest-pk3s.zip)
  and extract them to `baseq3/` subdirectory
  * Be sure to read [EULA](https://ioquake3.org/extras/patch-data/) first
  * There should already be `pakQ3VR.pk3` file in that directory
* Place `pak0.pk3` file from original game in `baseq3/` subdirectory
  * If you don't own the full game, you can also use [`pak0.pk3` from the demo](https://github.com/Team-Beef-Studios/ioq3quest/raw/dba25f1d528253ad9ff8638931726d06bffcf0b0/android/app/src/main/assets/pak0.pk3)

then any time you want to play simply:

* Start your VR runtime (e.g. SteamVR)
* Run `q3vr.exe`

> [!TIP]
> It's highly recommended to go through configuration options in `Setup` menu -
> before jumping into the game - to set controls and settings to your liking.


## Feedback / bug reporting

> [!IMPORTANT]
> This project is currently in **beta**. Feedback from users is required to make
> any further improvements and fix bugs. If you've spotted any problems or have
> any improvement ideas, please open GitHub issue with details and/or reach out
> on Discord.

For your convienience this repository comes with a pre-defined template for
creating issues which should make the process easier.

Please remember to include any details that may help reproduce and analyze the
problem such as logs, your computer configuration, etc.


## Controls

By default the game provides default bindings for common actions based on
your controlers.

### Controls in the menu

* `B` on left hand opens/closes menu or goes one level up (like `ESC`)
* `B` on right hand resets position of Virtual Screen
* `Trigger` on active hand - selects currently selected option (like `LMB`)
* `Trigger` on inactive hand - makes this hand active

### Controls as spectator

* Free cam
  * Movement forward/backward/left/right is based done with left thumbstick
  * The exact orientation is based on left controller's orientation
* First person spectator
  * `B` on right hand resets the position of Virtual Screen
  * `A` on left hand changes spectate mode (free cam/first person/...)
  * `Trigger` on left or right hand changes the person thay is spectated

### Controls in the game

See table below for default mapping. Some of these will be affected by changes
made in the in-game `Setup` configuration menus.

### Changing bindings/setting custom actions

If you wish to change bindings, you can try doing so from your
VR runtime's input configuration screen (e.g. Steam VR input bindings) or
manually set specific actions to given keys. To do so, you need to create
`autoexec.cfg` file in the `baseq3/` subdirectory and set custom bindings with:

```
set vr_button_map_<key> "<action>"
```

These are the default mappings:

| Key | Action | Notes |
|-----|--------|-------|
| `PRIMARYTRIGGER` | `"+attack"` | Fire weapon|
| `SECONDARYTRIGGER` | `"+moveup"` | Jump |
| `PRIMARYTHUMBSTICK` | `""` |  |
| `SECONDARYTHUMBSTICK` | `"+scores"` | Player list |
| `PRIMARYTRACKPAD` | `"+scores"` | Player list |
| `SECONDARYTRACKPAD` | `"togglemenu"` | Menu/back |
| `PRIMARYGRIP` | `"+weapon_select"` | Opens weapon selection wheel (if enabled) |
| `SECONDARYGRIP` | `"+weapon_stabilise"` | Triggers weapon stabilization (if enabled) |
| `RTHUMBLEFT` | `"turnleft"` | Snap/smooth turn left |
| `RTHUMBRIGHT` | `"turnright"` | Snap/smooth turn right |
| `RTHUMBFORWARD` | `""` |  |
| `RTHUMBBACK` | `""` |  |
| `Y` | `"togglemenu"` | Menu/back |
| `X` | `"+button2"` | Use item |
| `B` | `"+movedown"` | Crouch |
| `A` | `"+moveup"` | Jump |

Please refer to [Quake 3 Quest website](https://quake3.quakevr.com/) for more
details.


## Troubleshooting

If you're having some problems consider:

* Verifying that all necessary PAK files are located in `baseq3/` subdirectory,
  i.e. `pak0.pk3`, ..., `pak8.pk3` and `pakQ3VR.pk3`
* Removing all user config files from your profile's directory
  * On Windows this will be `%appdata%\Quake3\` (usually
    `C:\Users\$user\AppData\Roaming\Quake3`)
  * NOTE: this will remove your local configuration along with all the mods and
    map files you downloaded. Consider making a backup
* Checking if the problem occurs only in Single Player campaign, Skirmish mode,
  Multiplayer or in all of these modes
* Checking [previous issue reports](https://github.com/RippeR37/q3vr/issues)
* Updating drivers for your GPU, etc.
* Checking if there isn't a newer verions of `Q3VR` available which may already
  have a fix for your problem
  * You can also check older versions to see if it worked before
* **[BETA]** During beta, the game will open a console in the background with
  logs - they may contain something that will hint what's the problem

If you're unable to find solution to your problem yourself, please reach out to
the community (see section below) or
[create an issue in this repository](https://github.com/RippeR37/q3vr-private/issues/new/choose).
Please remember to provide as much details as you can which will help us
understand the problem and find a solution for it.


## Community

If you want to reach out to other Quake 3 Arena players - either desktop or VR -
consider these joining these communities:

* Quake 3 community
  * [`ioquake3` discord](https://discord.gg/YY9UvMHGYb) - `#ioquake3` channel
  * [`ioquake3` discourse forum](https://discourse.ioquake.org/)
* Quake3 VR-specific community
  * [`Quake3Quest` discord](https://discord.gg/wcbspgPHpx) - `#q3-pcvr` channel


## How to build

This project uses CMake so building it on supported platforms should be
straightforward, e.g. on Windows with MSVC you can build it with:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Check out [GitHub Actions workflow](.github/workflows/build.yml) for reference.

> [!NOTE]
> Currently only Windows is officially supported. Other platforms _may_ be
> supported in the future.


## Contributions

Any contributions are welcome! Before making bigger changes, please
[discuss](https://github.com/RippeR37/q3vr/discussions) them first to ensure
that they align with our goals.


## Support

If you like what I'm working on and would like to support me or my future work
on VR games, ports or tools you can do so via:

* [GitHub Sponsors](https://github.com/sponsors/RippeR37/)
* [Buy Me a Coffe](https://buymeacoffee.com/RippeR37)

Please also consider supporting authors of `ioquake3` and `Quake3Quest`
projects.
