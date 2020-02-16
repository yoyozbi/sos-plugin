#pragma once
#include "json.hpp"

class ApiTeam {
public:
	int TeamNum;
	int TeamIndex;
	std::string TeamColor;

	bool Dirty = true;
	std::string Name;
	int Goals;
	json::JSON getJson() {
		json::JSON json_team;
		json_team["Name"] = Name;
		json_team["Goals"] = Goals;
		json_team["TeamColor"] = TeamColor;
		return json_team;
	};
};