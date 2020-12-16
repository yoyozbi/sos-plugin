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

```json
{
  "wsRelay:info": "string",
  "game:update_state": {
    "event": "string",
    "game": {
      "arena": "string",
      "ballSpeed": "number",
      "ballTeam": "number",
      "hasTarget": "boolean",
      "hasWinner": "boolean",
      "isOT": "boolean",
      "isReplay": "boolean",
      "target": "string",
      "teams": {
        "0": {
          "name": "string",
          "score": "number"
        },
        "1": {
          "name": "string",
          "score": "number"
        }
      },
      "time": "number",
      "winner": "string"
    },
    "hasGame": "boolean",
    "players": {
      "PLAYER OBJECT": {
        "assists": "number",
        "attacker": "string",
        "boost": "number",
        "cartouches": "number",
        "goals": "number",
        "hasCar": "boolean",
        "id": "string",
        "isDead": "boolean",
        "isSonic": "boolean",
        "name": "string",
        "primaryID": "number",
        "saves": "number",
        "score": "number",
        "shots": "number",
        "speed": "number",
        "team": "number",
        "touches": "number"
      }
    }
  },
  "game:match_created": "string",
  "game:initialized": "string",
  "game:pre_countdown_begin": "string",
  "game:post_countdown_begin": "string",
  "game:statfeed_event": {
    "main_target": "string",
    "secondary_target": "string",
    "type": "string"
  },
  "game:goal_scored": "string",
  "game:replay_start": "string",
  "game:replay_will_end": "string",
  "game:replay_end": "string",
  "game:match_ended": {
    "winner_team_num": "number"
  },
  "game:podium_start": "string",
  "game:replay_created": "string"
}
```
Stateless means that the event has no useful information attached. It sends a string containing the name of the event

## Libraries Required

The following libraries can be retrieved from this submodule:
https://gitlab.com/bakkesplugins/sos/sos-plugin-includes

- websocketpp 0.8.1 https://github.com/zaphoyd/websocketpp
- asio https://think-async.com/Asio/
- SimpleJson https://github.com/nbsdx/SimpleJSON
- RenderingTools https://github.com/CinderBlocc/RenderingTools