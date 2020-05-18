#include "SOS.h"
#include "windows.h"
#include <ctime>

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
bool isCurrentlySpectating = false;
bool isInReplay = false;

std::time_t lastSendKeyTime = std::time(0);

void SOS::onLoad() {
	cvarManager->registerCvar("sos_state_flush_rate", "200", "Rate at which to send events to websocket (milliseconds)", true, true, 100.0f, true, 2000.0f).bindTo(update_cvar);
	cvarManager->registerCvar("SOS_autojoin_spectator", "0", "Autojoin Spectator", true, false, 0.0f, false, 0.0f, false).bindTo(autojoinSpectator_cvar);
	cvarManager->registerNotifier("SOS_c_reset_internal_state", [this](std::vector<string> params) {
		firstCountdownHit = false;
		matchCreated = false;
		isCurrentlySpectating = false;
		isInReplay = false;
		HookMatchEnded("match_ended");
	}, "Reset internal state", PERMISSION_ALL);

	//GAME TIME EVENTS
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", std::bind(&SOS::HookMatchCreated, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Countdown.BeginState", std::bind(&SOS::HookCountdownInit, this, _1));
	this->gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&SOS::HookMatchEnded, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", std::bind(&SOS::HookPodiumStart, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState", std::bind(&SOS::HookReplayStart, this, _1));
	this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", std::bind(&SOS::HookReplayEnd, this, _1));
	this->gameWrapper->HookEventPost("Function TAGame.PitchTekDrawingComponent_TA.QueueGoalExplosionDecal", std::bind(&SOS::HookReplayWillEnd, this, _1));
	//this->gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Car_TA.EventDemolishedd", std::bind(&SOS::HookCarDemolished, this, _1, _2, _3));

	//INGAME ACTIONS
	//this->gameWrapper->HookEventPost("Function TAGame.PRI_TA.PostBeginPlay", std::bind(&SOS::HookGameStarted, this, _1));
	//this->gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PostGoalScored.BeginState", std::bind(&SOS::HookGoalScored, this, _1));
	this->gameWrapper->HookEventPost("Function TAGame.ArenaSoundManager_TA.PlayGoalScoredSounds", std::bind(&SOS::HookGoalScored, this, _1));

	this->ws_connections = new ConnectionSet();
	this->UpdateGameState();
	WebsocketServerInit();
}

void SOS::HookMatchCreated(string eventName) {
	//if (matchCreated) {
	//	HookMatchEnded("match_ended");
	//}
	//firstCountdownHit = false;
	//matchCreated = false;
	//isCurrentlySpectating = false;
	//isInReplay = false;
	
	//No state
	matchCreated = true;
	this->SendEvent("game:match_created", "game_match_created");

	if (*autojoinSpectator_cvar && (matchCreated && !firstCountdownHit)) {
		auto server = gameWrapper->GetOnlineGame();
		if (!server.IsNull())
		{
			auto player = server.GetLocalPrimaryPlayer();

			if (!player.IsNull() && !isCurrentlySpectating)
			{
				std::time_t currentTime = std::time(0);
				if ((currentTime - lastSendKeyTime) > 5) {
					isCurrentlySpectating = true;
					player.Spectate();
					gameWrapper->SetTimeout(std::bind(&SOS::SendKeyH, this), 1.0f);
					lastSendKeyTime = std::time(0);
				}
			}
		}
	}
}

void SOS::HookMatchEnded(string eventName) {
	ServerWrapper server = gameWrapper->GetOnlineGame();
	json::JSON winnerData;
	winnerData["winner_team_num"] = NULL;
	this->UpdateTeamsState();
	this->UpdatePlayersState();
	winnerData["team_state"] = this->GetTeamsStateJson();
	if (!server.IsNull()) {
		TeamWrapper winner = server.GetMatchWinner();
		if (!winner.IsNull()) {
			winnerData["winner_team_num"] = winner.GetTeamNum();
		}
	}
	matchCreated = false;
	firstCountdownHit = false;
	isCurrentlySpectating = false;

	this->SendEvent("game:match_ended", winnerData);
	this->SendEvent("game:all_players_data_update", this->GetPlayersStateJson());
}

void SOS::HookCountdownInit(string eventName) {
	json::JSON initData;
	if (!firstCountdownHit && gameWrapper->IsInOnlineGame()) {
		firstCountdownHit = true;
		this->ClearPlayersState();
		this->UpdatePlayersState();
		this->UpdateTeamsState();

		initData["teams"] = this->GetTeamsStateJson();
		initData["players"] = this->GetPlayersStateJson();

		this->SendEvent("game:player_team_data", initData);
		this->SendEvent("game:initialized", "initialized");
	}
	if (firstCountdownHit && gameWrapper->IsInOnlineGame()) {
		this->UpdateTeamsState();
		initData["teams"] = this->GetTeamsStateJson();
		this->SendEvent("game:team_data", initData);
	}
	//No state
	this->SendEvent("game:pre_countdown_begin", "pre_game_countdown_begin");
	this->SendEvent("game:post_countdown_begin", "post_game_countdown_begin");
}

void SOS::HookPodiumStart(string eventName) {
	//No state
	this->SendEvent("game:podium_start", "game_podium_start");
}

void SOS::HookReplayStart(string eventName) {
	//No state
	isInReplay = true;
	this->SendEvent("game:replay_start", "game_replay_start");
}

void SOS::HookReplayEnd(string eventName) {
	//No state
	isInReplay = false;
	this->SendEvent("game:replay_end", "game_replay_end");
}

void SOS::HookReplayWillEnd(string eventName) {
	//No state
	if (isInReplay) {
		this->SendEvent("game:replay_will_end", "game_replay_will_end");
	}
}

void SOS::HookGoalScored(std::string eventName) {
	this->SendEvent("game:goal_scored", "game_goal_scored");
	if (this->gameWrapper->IsInOnlineGame()) {
		ArrayWrapper<PriWrapper> players = this->gameWrapper->GetOnlineGame().GetPRIs();
		for (int a = 0; a < players.Count(); a++ ) {
			if (!this->PlayersState[a].Dirty && this->PlayersState[a].Goals < players.Get(a).GetMatchGoals()) {
				this->UpdatePlayersState();
				this->UpdateTeamsState();
				this->SendEvent("game:goal_scored_player_data", this->PlayersState[a]);
				this->SendEvent("game:all_players_data_update", this->GetPlayersStateJson());
			}
		}
	}
}

void SOS::UpdateGameState() {
	if (this->gameWrapper->IsInOnlineGame() && firstCountdownHit) {
		this->ClearPlayersState();
		this->UpdatePlayersState();
		//this->SendEvent("game:update_tick", this->GetPlayersStateJson());
	}
	
	//if (*autojoinSpectator_cvar && (matchCreated && !firstCountdownHit)) {
	//	auto server = gameWrapper->GetOnlineGame();
	//	if (!server.IsNull())
	//	{
	//		auto player = server.GetLocalPrimaryPlayer();

	//		if (!player.IsNull() && !isCurrentlySpectating)
	//		{
	//			std::time_t currentTime = std::time(0);
	//			if ((currentTime - lastSendKeyTime) > 5) {
	//				isCurrentlySpectating = true;
	//				player.Spectate();
	//				gameWrapper->SetTimeout(std::bind(&SOS::SendKeyH, this), 1.0f);
	//				lastSendKeyTime = std::time(0);
	//			}
	//		}
	//	}
	//}

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
		TeamsState[a].TeamColor = SOS::UnrealColorToString(teams.Get(a).GetTeamColor());
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
	try {
		for (connection_hdl it : *ws_connections) {
			ws_server->send(it, payload, websocketpp::frame::opcode::text);
		}
	}
	catch (exception e) {
		cvarManager->log("An error occured sending websocket event");
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

void SOS::SendKeyH() {
	HWND hWndTarget = FindWindowA(NULL, "Rocket League (32-bit, DX9, Cooked)");

	if (SetForegroundWindow(hWndTarget))
	{
		keybd_event(0x48, 0, 0, 0);
		keybd_event(0x48, 0, KEYEVENTF_KEYUP, 0);
	}
}

std::string SOS::UnrealColorToString(UnrealColor uc) {
	std::string hex = int_to_hex(uc.R) +  int_to_hex(uc.G) +  int_to_hex(uc.B);
	return hex;
}