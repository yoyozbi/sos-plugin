#pragma once
#pragma comment( lib, "BakkesMod.lib" )

#include <set>
#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"
#include "json.hpp"
#include "ApiPlayer.h"
#include "ApiTeam.h"
#include "ApiGame.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
using websocketpp::connection_hdl;

class SOS : public BakkesMod::Plugin::BakkesModPlugin
{
	typedef websocketpp::server<websocketpp::config::asio> SOSServer;
	typedef std::set<connection_hdl, std::owner_less<connection_hdl>> ConnectionSet;

public:
	virtual void onLoad();
	virtual void onUnload();
private:
	// Hook logic
	void HookMatchCreated(std::string eventName);
	void HookMatchEnded(std::string eventName);
	void HookCountdownInit(std::string eventName);
	void HookGameStarted(std::string eventName);
	void HookPodiumStart(std::string eventName);
	void HookGoalScored(std::string eventName);
	void HookCarDemolished(CarWrapper cw, void* params, std::string eventName);
	
	//Commands
	void CommandResetTeamCards();
	void CommandResetPlayerCards();
	void CommandPlayerCardsForceUpdate();

	void CvarUpdateTeamNameLeft(std::string str);
	void CvarUpdateTeamNameRight(std::string str);

	void CvarUpdateBestOfSeriesCount(int newCount);
	void CvarUpdateBestOfCurrentGame(int currentGame);
	void CvarUpdateBestOfGamesWonLeft(int winCount);
	void CvarUpdateBestOfGamesWonRight(int winCount);

	void WebsocketServerInit();
	void WebsocketServerClose();

	//SOS_best_of_series_count
	//SOS_best_of_series_current_number
	//SOS_best_of_series_games_won_left
	//SOS_best_of_series_games_won_right

	// Player state logic
	ApiPlayer PlayersState[16];
	ApiPlayer CurrentPlayer;
	void UpdatePlayersState();
	void ClearPlayersState();
	json::JSON GetPlayersStateJson();

	// Team state logic
	ApiTeam TeamsState[4];
	void UpdateTeamsState();
	void ClearTeamsState();
	json::JSON GetTeamsStateJson();

	// Game state logic
	ApiGame GameState;
	void UpdateGameState();
	void ClearGameState();
	json::JSON GetGameStateJson();

	// Event logic
	void SendEvent(string eventName, ApiGame game);
	void SendEvent(string eventName, ApiPlayer player);
	//void SendEvent(string eventName, string testString);
	void SendEvent(string eventName, json::JSON jsawn);
	//void SendEvent(string eventName, ApiPlayer player, ApiTeam team);
	//void SendEvent(string eventName, ApiPlayer player[], ApiTeam team[]);
	
	// Server logic
	SOSServer* ws_server;
	ConnectionSet* ws_connections;
	void SendWebSocketPayload(string payload);
	void OnWsOpen(connection_hdl hdl) { this->ws_connections->insert(hdl); }
	void OnWsClose(connection_hdl hdl) { this->ws_connections->erase(hdl); }
	
	// Generic game type helper
	enum GAMETYPE {
		GAMETYPE_REPLAY = 0,
		GAMETYPE_ONLINE = 1,
		GAMETYPE_FREEPLAY = 2,
		GAMETYPE_TRAINING = 3,
		GAMETYPE_SPECTATE = 4,
		GAMETYPE_MENU = 5
	};
	ServerWrapper* GetCurrentGameType() {
		if (gameWrapper->IsInReplay()) {
			return &this->gameWrapper->GetGameEventAsReplay();
		}
		else if (gameWrapper->IsInOnlineGame()) {
			return &this->gameWrapper->GetOnlineGame();
		}
		else if (gameWrapper->IsInFreeplay()) {
			return &this->gameWrapper->GetGameEventAsServer();
		}
		else if (gameWrapper->IsInCustomTraining()) {
			return &this->gameWrapper->GetGameEventAsServer();
		}
		else if (gameWrapper->IsSpectatingInOnlineGame()) {
			return &this->gameWrapper->GetOnlineGame();
		}
		else {
			return NULL;
		}
	}
};