# SOS Overlay System
The goal is to have a Rocket League stats/event web API to be used : 

- External statistics gathering
- Bots
- Desktop/Mobile applications
- HTML/CSS/JS widgets to be used in OBS (or any streaming solution supporting transparent HTML).
  - see https://www.twitch.tv/videos/481056339 for a demo.

## BakkesMod SDK Setup
The solution contains $(BAKKESMODSDK) environment variable references for the SDK. Create your own environment variable (use Google) point to the SDK base and the configuration will handle the rest

Example variable configuration:
1. My BakkesMod base installation is located at `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod`
2. Point the variable to `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod\bakkesmodsdk`

## BakkesMod Settings File
In the root directory of this repo, there's a file named `sos.set`. This is the BakkesMod Settings File that must be inserted into the following location to be able to control update frequency and SOS events
- `J:\SteamLibrary\steamapps\common\rocketleague\Binaries\Win32\bakkesmod\plugins\settings`

## Websocket Server
Address: ws://localhost:49122

Most event names are fairly self explanatory, but it is still recommended to listen to the WebSocket server for a game or two to get a feel for when events are fired
The websocket reports the following events in `channel:event` format:
```
"game": [
    "match_created",
    "match_ended",
    "countdown_begin",
    "podium_start",
    "initialized",
    "goal_scored",
    "player_team_data",
    "update_tick"
],
"sos": [
    "reset_player_cards",
    "reset_team_cards",
]
```

## Libraries Required

The following libraries can be retrieved from this submodule:
https://gitlab.com/bakkesplugins/sos/sos-plugin-includes

- websocketpp 0.8.1 https://github.com/zaphoyd/websocketpp
- asio https://think-async.com/Asio/
- SimpleJson https://github.com/nbsdx/SimpleJSON