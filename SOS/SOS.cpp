#include "SOS.h"
using websocketpp::connection_hdl;
using namespace std;
using placeholders::_1;
using placeholders::_2;
using placeholders::_3;
BAKKESMOD_PLUGIN(SOS, "SimpleRj Overlay System", "1.0.0", PLUGINTYPE_THREADED)

std::shared_ptr<float> update_cvar = std::make_shared<float>(200.0f);
std::shared_ptr<bool> autojoinSpectator_cvar = std::make_shared<bool>(false);
bool firstCountdownHit = false;
bool matchCreated = false;

void SOS::onLoad() {
	cvarManager->registerCvar("sos_state_flush_rate", "200", "Rate at which to send events to websocket (milliseconds)", true, true, 100.0f, true, 2000.0f).bindTo(update_cvar);
	cvarManager->registerCvar("SOS_autojoin_spectator", "0", "Autojoin Spectator", true, false, 0.0f, false, 0.0f, false).bindTo(autojoinSpectator_cvar);
	cvarManager->registerCvar("SOS_team_name_left", "Orange", "Set the left team's name. Changing this will send an event through the websocket server").addOnValueChanged([this](std::string str, CVarWrapper cvar) {CvarUpdateTeamNameLeft(cvar.getStringValue()); });
	cvarManager->registerCvar("SOS_team_name_right", "Blue", "Set the right team's name. Changing this will send an event through the websocket server").addOnValueChanged([this](std::string str, CVarWrapper cvar) {CvarUpdateTeamNameRight(cvar.getStringValue()); });
	cvarManager->registerCvar("SOS_best_of_series_count", "1", "Best of Series number (BO3, BO5...)", true, false, 0.0f, false, 0.0f, false).addOnValueChanged([this](std::string str, CVarWrapper cvar) {
		int val = cvar.getIntValue();
		if (val < 1) {
			val = 1;
		}
		else if (val > 9) 
		{
			val = 9;
		}
		else if (val % 2 != 1)
		{
			val += 1;
		}
		if (val != cvar.getIntValue()) {
			cvar.setValue(val);
		}
		CvarUpdateBestOfSeriesCount(cvar.getIntValue());
	});
	cvarManager->registerCvar("SOS_best_of_series_current_game", "1", "Best of Series current game number", true, false, 0.0f, false, 0.0f, false).addOnValueChanged([this](std::string str, CVarWrapper cvar) {
		int val = cvar.getIntValue();
		if (val < 1) {
			val = 1;
		}
		else if (val > 9)
		{
			val = 9;
		}
		if (val != cvar.getIntValue()) {
			cvar.setValue(val);
		}
		CvarUpdateBestOfCurrentGame(cvar.getIntValue());
	});
	cvarManager->registerCvar("SOS_best_of_series_games_won_left", "0", "Best of Series left team wins", true, false, 0.0f, false, 0.0f, false).addOnValueChanged([this](std::string str, CVarWrapper cvar) {
		int val = cvar.getIntValue();
		if (val < 0) {
			val = 0;
		}
		else if (val > 9)
		{
			val = 9;
		}
		if (val != cvar.getIntValue()) {
			cvar.setValue(val);
		}
		CvarUpdateBestOfGamesWonLeft(cvar.getIntValue());
	});
	cvarManager->registerCvar("SOS_best_of_series_games_won_right", "0", "Best of Series right team wins", true, false, 0.0f, false, 0.0f, false).addOnValueChanged([this](std::string str, CVarWrapper cvar) {
		int val = cvar.getIntValue();
		if (val < 0) {
			val = 0;
		}
		else if (val > 9)
		{
			val = 9;
		}
		if (val != cvar.getIntValue()) {
			cvar.setValue(val);
		}
		CvarUpdateBestOfGamesWonRight(cvar.getIntValue());
	});

	cvarManager->registerNotifier("SOS_c_send_reset_teams_event", [this](std::vector<string> params) {CommandResetPlayerCards(); }, "Send a reset teams event to connected websockets", PERMISSION_ALL);
	cvarManager->registerNotifier("SOS_c_send_reset_player_cards_event", [this](std::vector<string> params) {CommandResetPlayerCards(); }, "Send a reset player card event to connect websockets", PERMISSION_ALL);
	cvarManager->registerNotifier("SOS_c_send_player_cards_force_update_event", [this](std::vector<string> params) {CommandPlayerCardsForceUpdate(); }, "Send new player data to fill player cards with", PERMISSION_ALL);
	cvarManager->registerNotifier("SOS_c_reset_internal_state", [this](std::vector<string> params) {
		HookMatchEnded("match_ended");
	}, "Reset internal state", PERMISSION_ALL);

	//GAME TIME EVENTS
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", std::bind(&SOS::HookMatchCreated, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Countdown.BeginState", std::bind(&SOS::HookCountdownInit, this, _1));
	this->gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&SOS::HookMatchEnded, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", std::bind(&SOS::HookPodiumStart, this, _1));
	//this->gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Car_TA.EventDemolishedd", std::bind(&SOS::HookCarDemolished, this, _1, _2, _3));

	//INGAME ACTIONS
	//this->gameWrapper->HookEventPost("Function TAGame.PRI_TA.PostBeginPlay", std::bind(&SOS::HookGameStarted, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PostGoalScored.BeginState", std::bind(&SOS::HookGoalScored, this, _1));

	this->ws_connections = new ConnectionSet();
	this->UpdateGameState();
	WebsocketServerInit();
}

void SOS::HookMatchCreated(string eventName) {
	//No state
	matchCreated = true;
	this->SendEvent("game:match_created", "game_match_created");
}

void SOS::HookMatchEnded(string eventName) {
	ServerWrapper server = gameWrapper->GetOnlineGame();
	json::JSON winnerData;
	winnerData["winner_team_num"] = NULL;
	if (!server.IsNull()) {
		TeamWrapper winner = server.GetMatchWinner();
		winnerData["winner_team_num"] = winner.GetTeamNum();
	}
	matchCreated = false;
	firstCountdownHit = false;

	this->SendEvent("game:match_ended", winnerData);
}

void SOS::CvarUpdateTeamNameLeft(string teamName) {
	this->SendEvent("sos:team_name_update_left", teamName);
}

void SOS::CvarUpdateTeamNameRight(string teamName) {
	this->SendEvent("sos:team_name_update_right", teamName);
}

void SOS::CvarUpdateBestOfSeriesCount(int newCount) {
	json::JSON data;
	data["new_count"] = newCount;
	this->SendEvent("sos:best_of_series_count", data);
}

void SOS::CvarUpdateBestOfCurrentGame(int currentGame) {
	json::JSON data;
	data["current_game_num"] = currentGame;
	this->SendEvent("sos:best_of_series_current_number", data);
}

void SOS::CvarUpdateBestOfGamesWonLeft(int winCount) {
	json::JSON data;
	data["win_count"] = winCount;
	this->SendEvent("sos:best_of_series_games_won_left", data);
}

void SOS::CvarUpdateBestOfGamesWonRight(int winCount) {
	json::JSON data;
	data["win_count"] = winCount;
	this->SendEvent("sos:best_of_series_games_won_right", data);
}

void SOS::HookCountdownInit(string eventName) {
	if (!firstCountdownHit && gameWrapper->IsInOnlineGame()) {
		firstCountdownHit = true;
		this->UpdatePlayersState();
		this->UpdateTeamsState();

		json::JSON initData;
		initData["players"] = this->GetPlayersStateJson();
		initData["teams"] = this->GetTeamsStateJson();

		this->SendEvent("game:player_team_data", initData);
		this->SendEvent("game:initialized", "initialized");
	}
	//No state
	this->SendEvent("game:countdown_begin", "game_countdown_begin");
}

void SOS::HookPodiumStart(string eventName) {
	//No state
	this->SendEvent("game:podium_start", "game_podium_start");
}

void SOS::HookGoalScored(std::string eventName) {
	if (this->gameWrapper->IsInOnlineGame()) {
		ArrayWrapper<PriWrapper> players = this->gameWrapper->GetOnlineGame().GetPRIs();
		for (int a = 0; a < players.Count(); a++ ) {
			if (!this->PlayersState[a].Dirty && this->PlayersState[a].Goals < players.Get(a).GetMatchGoals()) {
				this->UpdatePlayersState();
				this->UpdateTeamsState();
				this->SendEvent("game:goal_scored", this->PlayersState[a]);
			}
		}
	}
}

void SOS::UpdateGameState() {
	if (this->gameWrapper->IsInOnlineGame()) {
		this->ClearPlayersState();
		this->UpdatePlayersState();
		this->SendEvent("game:update_tick", this->GetPlayersStateJson());
	}
	
	if (*autojoinSpectator_cvar && (matchCreated && !firstCountdownHit)) {
		auto server = gameWrapper->GetOnlineGame();
		if (!server.IsNull())
		{
			auto player = server.GetLocalPrimaryPlayer();
			if (!player.IsNull())
			{
				player.Spectate();
			}
		}
	}

	// I'll be back
	this->gameWrapper->SetTimeout(std::bind(&SOS::UpdateGameState, this), (*update_cvar)/1000.f*1.0001f);
}

void SOS::ClearPlayersState() {
	for(ApiPlayer &PlayerState : PlayersState) {
		PlayerState.Dirty = true; 
	}
}

void SOS::UpdatePlayersState() {
	ArrayWrapper<PriWrapper> players = this->gameWrapper->GetOnlineGame().GetPRIs();
	for (int a = 0; a < players.Count(); a++ ) {
		if (players.Get(a).GetbIsSpectator())
		{
			continue;
		}
		PlayersState[a].Dirty = false;
		PlayersState[a].Assists = players.Get(a).GetMatchAssists();
		PlayersState[a].BallTouches = players.Get(a).GetBallTouches();
		PlayersState[a].Demolishes = players.Get(a).GetMatchDemolishes();
		PlayersState[a].Goals = players.Get(a).GetMatchGoals();
		PlayersState[a].IsBot = !players.Get(a).IsPlayer();
		PlayersState[a].Kills = players.Get(a).GetKills();
		PlayersState[a].MMR = 1337;
		PlayersState[a].Ping = players.Get(a).GetExactPing();
		PlayersState[a].PlayerID = players.Get(a).GetPlayerID();
		PlayersState[a].PlayerName = players.Get(a).GetPlayerName().ToString();
		PlayersState[a].PlayerUniqueID = players.Get(a).GetUniqueId().ID;
		PlayersState[a].Saves = players.Get(a).GetMatchSaves();
		PlayersState[a].Score = players.Get(a).GetMatchScore();
		PlayersState[a].Shots = players.Get(a).GetMatchShots();
		PlayersState[a].TeamNum = players.Get(a).GetTeamNum();
		if (!players.Get(a).GetCar().IsNull() && !players.Get(a).GetCar().GetBoostComponent().IsNull()) {
			PlayersState[a].CurrentBoostAmount = players.Get(a).GetCar().GetBoostComponent().GetCurrentBoostAmount();
		}
		if (players.Get(a).GetUniqueId().ID == this->gameWrapper->GetSteamID()) {
			CurrentPlayer = PlayersState[a];
		}
	}
}

json::JSON SOS::GetPlayersStateJson() {
	json::JSON json_players;
	for (ApiPlayer &PlayerState : PlayersState) {
		if (!PlayerState.Dirty) {
			json_players.append(PlayerState.getJson());
		}
	}
	return json_players;
}

void SOS::ClearTeamsState() {
	for (ApiTeam &TeamState : TeamsState) {
		TeamState.Dirty = true;
	}
}

json::JSON SOS::GetTeamsStateJson() {
	json::JSON json_teams;
	for (ApiTeam &TeamState : TeamsState) {
		if (!TeamState.Dirty) {
			json_teams.append(TeamState.getJson());
		}
	}
	return json_teams;
}

void SOS::UpdateTeamsState() {
	ArrayWrapper<TeamWrapper> teams = this->gameWrapper->GetOnlineGame().GetTeams();
	for (int a = 0; a < teams.Count(); a++) {
		TeamsState[a].Dirty = false;
		TeamsState[a].Goals = teams.Get(a).GetScore();
		TeamsState[a].TeamIndex = teams.Get(a).GetTeamIndex();
		TeamsState[a].TeamNum = teams.Get(a).GetTeamNum();
		if (!teams.Get(a).GetSanitizedTeamName().IsNull()) {
			TeamsState[a].Name = teams.Get(a).GetSanitizedTeamName().ToString();
		}
		else if (TeamsState[a].TeamIndex == 1) {
			TeamsState[a].Name = "Orange";
		}
		else {
			TeamsState[a].Name = "Blue";
		}
	}
}

void SOS::SendEvent(string eventName, json::JSON jsawn) {
	json::JSON event;
	event["event"] = eventName;
	event["data"] = jsawn;
	this->SendWebSocketPayload(
		event.dump()
	);
}

void SOS::SendEvent(string eventName, ApiPlayer player) {
	json::JSON event;
	event["event"] = eventName;
	event["data"] = player.getJson();
	this->SendWebSocketPayload(
		event.dump()
	);
}

void SOS::SendEvent(string eventName, ApiGame game) {
	json::JSON event;
	event["event"] = eventName;
	event["data"] = game.getJson();
	this->SendWebSocketPayload(
		event.dump()
	);
}

void SOS::SendWebSocketPayload(std::string payload) {
	// broadcast to all connections
	for (connection_hdl it : *ws_connections) {
		ws_server->send(it, payload , websocketpp::frame::opcode::text);
	}
}

void SOS::onUnload() {
	WebsocketServerClose();
}

bool websocketServerInitialized = false;
void SOS::WebsocketServerInit() {
	if (!websocketServerInitialized) {
		cvarManager->log("[SOS] Starting WebSocket server");
		ws_server = new SOSServer();
		cvarManager->log("[SOS] Starting asio");
		ws_server->init_asio();
		ws_server->set_open_handler(websocketpp::lib::bind(&SOS::OnWsOpen, this, _1));
		ws_server->set_close_handler(websocketpp::lib::bind(&SOS::OnWsClose, this, _1));
		cvarManager->log("[SOS] Starting listen on port 49122");
		ws_server->listen(49122);
		cvarManager->log("[SOS] Starting accepting connections");
		ws_server->start_accept();
		ws_server->run();
		cvarManager->log("[SOS] WebSocket Server initialized");
		websocketServerInitialized = true;
	}
	else {
		cvarManager->log("[SOS] WebSocket server already initialized. Stop it or restart RL");
	}
}

void SOS::WebsocketServerClose() {
	// Init websocket server
	if (ws_server != NULL) {
		ws_server->stop();
		ws_server->stop_listening();
	}
	ws_connections->clear();
	websocketServerInitialized = false;
}

void SOS::CommandResetPlayerCards() {
	this->SendEvent("sos:reset_player_cards", "sos_reset_player_cards");
}

void SOS::CommandResetTeamCards() {
	this->SendEvent("sos:reset_team_cards", "sos_reset_team_cards");
}

void SOS::CommandPlayerCardsForceUpdate() {
	this->UpdatePlayersState();
	this->SendEvent("sos:player_cards_force_update", this->GetPlayersStateJson());
}