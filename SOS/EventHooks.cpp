#include "SOS.h"
#include "json.hpp"

/*
    Many of these hooks need to have `if (!*cvarEnabled) { return; }` added
    But I'm not sure which
*/

void SOS::HookAllEvents()
{
    //UPDATE GAMESTATE EVERY TICK
    gameWrapper->HookEvent("Function Engine.GameViewportClient.Tick", std::bind(&SOS::HookViewportTick, this));

    //FLOAT CLOCK EVENTS (Clock.cpp)
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated", std::bind(&SOS::UpdateClock, this));
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.StartOvertime", std::bind(&SOS::PauseClockOnOvertimeStarted, this));
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnHitBall", std::bind(&SOS::UnpauseClockOnBallTouch, this));

    //GAME EVENTS
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", std::bind(&SOS::HookMatchCreated, this));
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", std::bind(&SOS::HookMatchDestroyed, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Countdown.BeginState", std::bind(&SOS::HookCountdownInit, this));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&SOS::HookBallExplode, this));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal", std::bind(&SOS::HookOnHitGoal, this));
    gameWrapper->HookEvent("Function TAGame.GameInfo_Replay_TA.InitGame", std::bind(&SOS::HookReplayCreated, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState", std::bind(&SOS::HookReplayStart, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", std::bind(&SOS::HookReplayEnd, this));
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&SOS::HookMatchEnded, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", std::bind(&SOS::HookPodiumStart, this));


    //STATEVENT FEED
    gameWrapper->HookEventWithCaller<ServerWrapper>("Function TAGame.PRI_TA.ClientNotifyStatTickerMessage", std::bind(&SOS::HookStatEvent, this, _1, _2));

    //LAST TOUCH INFO
    gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.EventHitBall", std::bind(&SOS::HookCarBallHit, this, _1, _2, _3));
}

void SOS::HookViewportTick()
{
    if (!*cvarEnabled) { return; }

    //Clamp number of events per second
    static steady_clock::time_point lastCallTime = steady_clock::now(); // this line only fires first time function is called
    float timeSinceLastCall = duration_cast<duration<float>>(steady_clock::now() - lastCallTime).count();

    //MAIN FUNCTION (GameState.cpp)
    if (timeSinceLastCall >= (*cvarUpdateRate / 1000.f))
    {
        UpdateGameState();
        lastCallTime = steady_clock::now();
    }
    else
    {
        LOGC(std::to_string(timeSinceLastCall) + " - " + std::to_string((*cvarUpdateRate / 1000.f)) + " - Too early to send gamestate update");
    }
    
    //Get ball speed every tick (for goal speed)
    GetCurrentBallSpeed();
}

void SOS::HookMatchCreated()
{
    isClockPaused = true;
    matchCreated = true;
    diff = 0;
    SendEvent("game:match_created", "game_match_created");
}

void SOS::HookMatchDestroyed()
{
    isInReplay = false;
    matchCreated = false;
    firstCountdownHit = false;
    isCurrentlySpectating = false;
    isClockPaused = true;

    SendEvent("game:match_destroyed", "game_match_destroyed");
}

void SOS::HookCountdownInit()
{
    if (!firstCountdownHit && gameWrapper->IsInOnlineGame())
    {
        firstCountdownHit = true;
        SendEvent("game:initialized", "initialized");
    }

    SendEvent("game:pre_countdown_begin", "pre_game_countdown_begin");
    SendEvent("game:post_countdown_begin", "post_game_countdown_begin");
}

void SOS::HookBallExplode()
{
    if (!*cvarEnabled || !matchCreated) { return; }

    PauseClockOnGoal();
    HookReplayWillEnd();
}

void SOS::HookOnHitGoal()
{
    goalSpeed = ballCurrentSpeed;
}

void SOS::HookReplayCreated()
{
    //No state
    isClockPaused = true;
    matchCreated = true;
    diff = 0;
    SendEvent("game:replay_created", "game_replay_created");
}


void SOS::HookReplayStart()
{
    isInReplay = true;
    SendEvent("game:replay_start", "game_replay_start");
}

void SOS::HookReplayWillEnd()
{
    //This function is called by the HookBallExplode event
    if (isInReplay)
    {
        LOGC("Sending ReplayWillEnd Event");
        SendEvent("game:replay_will_end", "game_replay_will_end");
    }
}

void SOS::HookReplayEnd()
{
    isInReplay = false;
    SendEvent("game:replay_end", "game_replay_end");
}

void SOS::HookMatchEnded()
{
    isInReplay = false;
    matchCreated = false;
    firstCountdownHit = false;
    isCurrentlySpectating = false;
    isClockPaused = true;

    json::JSON winnerData;
    winnerData["winner_team_num"] = NULL;

    ServerWrapper server = gameWrapper->GetOnlineGame();
    if (!server.IsNull())
    {
        TeamWrapper winner = server.GetMatchWinner();
        if (!winner.IsNull())
        {
            winnerData["winner_team_num"] = winner.GetTeamNum();
        }
    }

    SendEvent("game:match_ended", winnerData);
}

void SOS::HookPodiumStart()
{
    SendEvent("game:podium_start", "game_podium_start");
}

void SOS::HookCarBallHit(CarWrapper car, void * params, std::string funcName)
{
    //Wait until next tick to get info.
    //Getting the info on the tick the hook was called is usually inaccurate.
    //(i.e. on kickoff the ball speed would be 0 even though the car has touched it)
    gameWrapper->SetTimeout(std::bind(&SOS::GetLastTouchInfo, this, car), 0.01f);
}

void SOS::HookStatEvent(ServerWrapper caller, void* args)
{
    auto tArgs = (DummyStatEventContainer*)args;

    std::wstring ws(tArgs->StatEvent->Label);
    std::string eventStr = std::string(ws.begin(), ws.end());

    //Victim info
    auto victim = PriWrapper(tArgs->Victim);
    std::string victimName, victimID;
    GetNameAndID(victim, victimName, victimID);
    
    //Receiver info
    auto receiver = PriWrapper(tArgs->Receiver);
    std::string receiverName, receiverID;
    GetNameAndID(receiver, receiverName, receiverID); 

    //General statfeed event
    json::JSON statfeed;
    statfeed["type"] = eventStr;
    statfeed["main_target"]["name"] = receiverName;
    statfeed["main_target"]["id"] = receiverID;
    statfeed["secondary_target"]["name"] = victimName;
    statfeed["secondary_target"]["id"] = victimID;
    SendEvent("game:statfeed_event", statfeed);

    //Goal statfeed event
    if (eventStr == "Goal")
    {
        json::JSON goalScoreData;
        goalScoreData["goalspeed"] = goalSpeed;
        goalScoreData["scorer"]["name"] = receiverName;
        goalScoreData["scorer"]["id"] = receiverID;
        goalScoreData["ball_last_touch"]["player"] = lastTouch.playerID;
        goalScoreData["ball_last_touch"]["speed"] = lastTouch.speed;
        SendEvent("game:goal_scored", goalScoreData);
    }
}
