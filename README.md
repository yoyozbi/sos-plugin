# SOS Overlay System
SOS-Plugin aims to provide an easy to use event relay in use for OBS streams, particularily in the Tournament sector. Personal streams are able to use this, but it is not officially supported at this moment.

Want some real life examples? Check out the `Codename: COVERT` assets:
https://gitlab.com/bakkesplugins/sos/codename-covert

**SOS has been used by the following productions**
- Codename: COVERT by iDazerin
- Disconnect's 3v3 monthly tournaments
- GoldRushGG
- Minor League Esports
- Rocket Soccar Confederation
- Women's Car Ball Championships by CrimsonGaming
- Rocket Baguette

## BakkesMod SDK Setup
The solution contains $(BAKKESMODSDK) environment variable references for the SDK. Create your own environment variable (use Google) point to the SDK base and the configuration will handle the rest

Example variable configuration:
1. My BakkesMod base installation is located at `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod`
2. Point the variable to `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod\bakkesmodsdk`

## BakkesMod Settings File
1. In the root directory of this repo, there's a file named `sos.set`. This is the BakkesMod Settings File that must be inserted into the following location to be able to control update frequency and SOS events
  - `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod\plugins\settings`

2. Open `bakkesmod/cfg/plugins.cfg` in a text editor. At the bottom on its own line, add `plugin load sos`

## Websocket Server
Address: ws://localhost:49122

Most event names are fairly self explanatory, but it is still recommended to listen to the WebSocket server for a game or two to get a feel for when events are fired
The websocket reports the following events in `channel:event` format:

```
"game": [
    "goal_scored" [Object of scorer Player state],
    "initialized": [Stateless],
    "match_created": [Stateless],
    "match_ended": [Object of winnerTeamNumber],
    "player_team_data": [Object of Player and Team state arrays],
    "podium_start": [Stateless],
    "post_countdown_begin": [Stateless],
    "pre_countdown_begin": [Stateless],
    "replay_end": [Stateless],
    "replay_start": [Stateless],
    "replay_will_end": [Stateless],
    "update_tick": [Object of Player and Team state arrays],
]
```
Stateless means that the event has no useful information attached. It sends a string containing the name of the event

## Libraries Required

The following libraries can be retrieved from this submodule:
https://gitlab.com/bakkesplugins/sos/sos-plugin-includes

- websocketpp 0.8.1 https://github.com/zaphoyd/websocketpp
- asio https://think-async.com/Asio/
- SimpleJson https://github.com/nbsdx/SimpleJSON