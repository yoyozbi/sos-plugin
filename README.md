# SOS Overlay System
SOS-Plugin aims to provide an easy to use event relay in use for OBS streams, particularily in the Tournament sector. Personal streams are able to use this, but it is not officially supported at this moment.

Also provided is a base that can be compiled and used in OBS:
https://gitlab.com/bakkesplugins/sos/sos-obs-base

- Demo video(s) coming soon

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
    "pre_countdown_begin": [Stateless],
    "post_countdown_begin": [Stateless],
    "goal_scored" [Object of scorer Player state],
    "initialized": [Stateless],
    "match_created": [Stateless],
    "match_ended": [Object of winnerTeamNumber],
    "player_team_data": [Object of Player and Team state arrays],
    "podium_start": [Stateless],
    "update_tick": [Object of Player and Team state arrays]
],
"sos": [
    "best_of_series_count": [Object of new bestOfSeriesCount],
    "best_of_series_current_number": [Object of new bestOfSeriesCurrentNumber],
    "best_of_series_games_won_left": [Object of new winCountLeft],
    "best_of_series_games_won_right": [Object of new winCountRight],
    "player_cards_force_update": [Array of Player states],
    "reset_player_cards": [Stateless],
    "reset_team_cards": [Stateless],
    "team_name_update_left": [String newTeamName],
    "team_name_update_right": [String newTeamName]
]
```
Stateless means that the event has no useful information attached. It sends a string containing the name of the event

## Libraries Required

The following libraries can be retrieved from this submodule:
https://gitlab.com/bakkesplugins/sos/sos-plugin-includes

- websocketpp 0.8.1 https://github.com/zaphoyd/websocketpp
- asio https://think-async.com/Asio/
- SimpleJson https://github.com/nbsdx/SimpleJSON