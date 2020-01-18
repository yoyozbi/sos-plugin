#pragma once
#include "json.hpp"

class ApiPlayer {
public:
	bool Dirty = true;
	bool IsBot;
	float CurrentBoostAmount;
	float MMR;
	float Ping;
	int Assists;
	int BallTouches;
	int Demolishes;
	int Goals;
	int Kills;
	int PlayerID;
	int Saves;
	int Score;
	int Shots;
	int TeamNum;
	std::string PlayerName;
	unsigned long long PlayerUniqueID;
	json::JSON getJson() {
		json::JSON json_player;
		json_player["Assists"] = Assists;
		json_player["BallTouches"] = BallTouches;
		json_player["CurrentBoostAmount"] = CurrentBoostAmount;
		json_player["Demolishes"] = Demolishes;
		json_player["Goals"] = Goals;
		json_player["IsBot"] = IsBot;
		json_player["Kills"] = Kills;
		json_player["MMR"] = MMR;
		json_player["Ping"] = Ping;
		json_player["PlayerID"] = PlayerID;
		json_player["PlayerName"] = PlayerName;
		json_player["PlayerUniqueID"] = std::to_string(PlayerUniqueID);
		json_player["Saves"] = Saves;
		json_player["Score"] = Score;
		json_player["Shots"] = Shots;
		json_player["TeamNum"] = TeamNum;
		return json_player;
	};
};