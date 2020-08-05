#include "SOS.h"

using websocketpp::connection_hdl;
using namespace std::chrono;
using namespace std::placeholders;


/*
    This is a tweaked version of DanMB's GameStateApi: https://github.com/DanMB/GameStateApi
    A lot of features merged in from the original SOS plugin: https://gitlab.com/bakkesplugins/sos/sos-plugin
    
    - Tweaked by CinderBlock for version 1.0.1
    - Thanks to Martinn for the Stat Feed code (and inadvertently, demolitions)
*/


#define SHOULDLOG 0
#if SHOULDLOG
#define LOGC(x) cvarManager->log(x)
#else
#define LOGC(x)
#endif


BAKKESMOD_PLUGIN(SOS, "Simple Overlay System", "1.4.0", PLUGINTYPE_THREADED)

void SOS::onLoad()
{
    //New cvars
    enabled = std::make_shared<bool>(false);
    port = std::make_shared<int>(49122);
    cvarManager->registerCvar("SOS_Enabled", "1", "Enable SOS plugin", true, true, 0, true, 0).bindTo(enabled);
    cvarManager->getCvar("SOS_Enabled").addOnValueChanged(std::bind(&SOS::EnabledChanged, this));
    cvarManager->registerCvar("SOS_Port", "49122", "Websocket port for SOS overlay plugin", true).bindTo(port);

    //Original SOS cvars and notifiers
    update_cvar = std::make_shared<float>(100.0f);
    cvarManager->registerCvar("sos_state_flush_rate", "100", "Rate at which to send events to websocket (milliseconds)", true, true, 100.0f, true, 2000.0f).bindTo(update_cvar);
    cvarManager->registerNotifier("SOS_c_reset_internal_state", [this](std::vector<std::string> params) { HookMatchEnded(); }, "Reset internal state", PERMISSION_ALL);

    //Hook to update JSON every render frame
    gameWrapper->HookEvent("Function Engine.GameViewportClient.Tick", std::bind(&SOS::UpdateGameState, this));

    //Hooks to handle calculations of float gametime
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated", std::bind(&SOS::UpdateClock, this));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&SOS::HookBallExplode, this));
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnHitBall", std::bind(&SOS::UnpauseClockOnBallTouch, this));
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.StartOvertime", std::bind(&SOS::PauseClockOnOvertimeStarted, this));


    //GAME TIME EVENTS
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.WaitingForPlayers.BeginState", std::bind(&SOS::HookMatchCreated, this));
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&SOS::HookMatchEnded, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Countdown.BeginState", std::bind(&SOS::HookCountdownInit, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", std::bind(&SOS::HookPodiumStart, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState", std::bind(&SOS::HookReplayStart, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", std::bind(&SOS::HookReplayEnd, this));

    //EVENT FEED
    gameWrapper->HookEventWithCaller<ServerWrapper>("Function TAGame.PRI_TA.ClientNotifyStatTickerMessage", std::bind(&SOS::OnStatEvent, this, _1, _2));

    #ifdef USE_NAMEPLATES
    //GET NAMEPLATES
    gameWrapper->RegisterDrawable(std::bind(&SOS::GetNameplateInfo, this, _1));
    #endif

    //Run websocket server
    ws_connections = new ConnectionSet();
    RunWsServer();
}
void SOS::onUnload()
{
    if (ws_server != NULL)
    {
        ws_server->stop();
        ws_server->stop_listening();
    }
    ws_connections->clear();
}
void SOS::EnabledChanged()
{
    //If mod has been disabled, stop any potentially remaining clock calculations
    if (!(*enabled))
    {
        isClockPaused = true;
        newRoundActive = false;
    }
}

/* ORIGINAL SOS HOOKS */
void SOS::HookBallExplode()
{
    if (!(*enabled) || !matchCreated)
    {
        return;
    }

    //cvarManager->log("BALL GO BOOM " + std::to_string(clock()));
    PauseClockOnGoal();
    HookReplayWillEnd();
    //HookGoalScored();
}
void SOS::HookMatchCreated()
{
    //No state
    isClockPaused = true;
    matchCreated = true;
    diff = 0;
    SendEvent("game:match_created", "game_match_created");
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
    //winnerData["team_state"] = GetTeamsStateJson();

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
    //SendEvent("game:all_players_data_update", GetPlayersStateJson());
}
void SOS::HookCountdownInit()
{
    if (!firstCountdownHit && gameWrapper->IsInOnlineGame())
    {
        firstCountdownHit = true;
        SendEvent("game:initialized", "initialized");
    }

    //No state
    SendEvent("game:pre_countdown_begin", "pre_game_countdown_begin");
    SendEvent("game:post_countdown_begin", "post_game_countdown_begin");
}
void SOS::HookPodiumStart()
{
    //No state
    SendEvent("game:podium_start", "game_podium_start");
}
void SOS::HookReplayStart()
{
    //No state
    isInReplay = true;
    SendEvent("game:replay_start", "game_replay_start");
}
void SOS::HookReplayEnd()
{
    //No state
    isInReplay = false;
    SendEvent("game:replay_end", "game_replay_end");
}
void SOS::HookReplayWillEnd()
{
    //cvarManager->log("HookReplayWillEnd " + std::to_string(clock()));
    //No state
    if (isInReplay)
    {
        //cvarManager->log("Sending ReplayWillEnd Event");
        SendEvent("game:replay_will_end", "game_replay_will_end");
    }
}
void SOS::HookGoalScored()
{
    //cvarManager->log("HookGoalScored " + std::to_string(clock()));
    //No state
    if (!isInReplay)
    {
        //cvarManager->log("Sending GoalScored Event");
        SendEvent("game:goal_scored", "game_goal_scored");
    }
}

/* SEND EVENTS */
void SOS::SendEvent(std::string eventName, json::JSON jsawn)
{
    json::JSON event;
    event["event"] = eventName;
    event["data"] = jsawn;
    SendWebSocketPayload(event.dump());
}

/* MAIN FUNCTION */
void SOS::UpdateGameState()
{
    if (!(*enabled)) { return; } // Cancel function call from hook

    //Clamp number of events per second
    static steady_clock::time_point lastCallTime = steady_clock::now();
    float timeSinceLastCall = duration_cast<duration<float>>(steady_clock::now() - lastCallTime).count();
    if (timeSinceLastCall < (*update_cvar / 1000.f)) {
        //cvarManager->log("Too early to send update");
        return;
    }
    lastCallTime = steady_clock::now();

    json::JSON state;
    state["event"] = "gamestate";
    state["players"] = json::Object();
    state["game"] = json::Object();

    bool isInGame = false;

    if (gameWrapper->IsInOnlineGame()) {
        //cvarManager->log("IsInOnlineGame: (need true) true");
        if (gameWrapper->GetLocalCar().IsNull()) {
            //cvarManager->log("GetLocalCar().IsNull(): (need true) true");
            if (!gameWrapper->GetOnlineGame().IsNull()) {
                //cvarManager->log("GetOnlineGame().IsNull(): (need false) false");
                ServerWrapper server = gameWrapper->GetOnlineGame();
                if(server.GetPlaylist().memory_address != NULL) {
                    if (server.GetPlaylist().GetPlaylistId() == 6 || server.GetPlaylist().GetPlaylistId() == 24)
                    {
                        isInGame = true;
                        //cvarManager->log("isInGame: (need true) true");

                        ArrayWrapper<PriWrapper> PRIs = server.GetPRIs();
                        for (int i = 0; i < PRIs.Count(); ++i)
                        {
                            PriWrapper pri = PRIs.Get(i);
                            if (pri.IsNull()) { continue; }
                            if (pri.IsSpectator() || pri.GetTeamNum() == 255) { continue; }

                            GetPlayerInfo(state, pri);
                        }

                        GetTeamInfo(state, server);
                        GetGameTimeInfo(state, server);
                        GetBallInfo(state, server);
                        GetWinnerInfo(state, server);
                        GetCameraInfo(state);
                    }
                    else {
                        //cvarManager->log("isInGame: (need true) false");
                    }
                }
                else {
                    //cvarManager->log("server.GetPlaylist() IsNull: (need false) true");
                }
            }
            else {
                //cvarManager->log("GetOnlineGame().IsNull(): (need false) false");
            }
        }
        else {
            //cvarManager->log("GetLocalCar().IsNull(): (need true) false");
        }
    }
    else 
    {
        //cvarManager->log("IsInOnlineGame: (need true) false");
    }

    state["hasGame"] = isInGame;

    //Send the payload
    if (isInGame)
    {
        SendEvent("game:update_state", state);
    }
    else {
        cvarManager->log("No Game");
    }
}

/* INDIVIDUAL FUNCTIONS */
void SOS::GetPlayerInfo(json::JSON& state, PriWrapper pri)
{
    int key = pri.GetSpectatorShortcut();
    std::string name = pri.GetPlayerName().IsNull() ? "" : pri.GetPlayerName().ToString();
    std::string id = name + "_" + std::to_string(key);


    state["players"][id] = json::Object();

    state["players"][id]["name"] = name;
    state["players"][id]["id"] = id;
    state["players"][id]["primaryID"] = std::to_string(pri.GetUniqueId().ID);
    state["players"][id]["team"] = pri.GetTeamNum();
    state["players"][id]["score"] = pri.GetMatchScore();
    state["players"][id]["goals"] = pri.GetMatchGoals();
    state["players"][id]["shots"] = pri.GetMatchShots();
    state["players"][id]["assists"] = pri.GetMatchAssists();
    state["players"][id]["saves"] = pri.GetMatchSaves();
    state["players"][id]["touches"] = pri.GetBallTouches();
    state["players"][id]["cartouches"] = pri.GetCarTouches();
    state["players"][id]["demos"] = pri.GetMatchDemolishes();

    CarWrapper car = pri.GetCar();

    //Car is null, return
    if (car.IsNull())
    {
        state["players"][id]["hasCar"] = false;
        state["players"][id]["speed"] = 0;
        state["players"][id]["boost"] = 0;
        state["players"][id]["isSonic"] = false;

        state["players"][id]["isDead"] = false;
        state["players"][id]["attacker"] = "";

		return;
	}
	//else get car position
	Vector carLocation = car.GetLocation();
	state["players"][id]["x"] = carLocation.X;
	state["players"][id]["y"] = carLocation.Y;
	state["players"][id]["z"] = carLocation.Z;

    if (car.GetbHidden())
    {
        state["players"][id]["isDead"] = true;
        PriWrapper att = car.GetAttackerPRI(); // Attacker is only set on local player???
        if (!att.IsNull())
        {
            std::string attName = att.GetPlayerName().IsNull() ? "" : att.GetPlayerName().ToString();
            state["players"][id]["attacker"] = attName + "_" + std::to_string(att.GetSpectatorShortcut());
        }
        else
        {
            state["players"][id]["attacker"] = "";
        }
    }
    else
    {
        state["players"][id]["isDead"] = false;
        state["players"][id]["attacker"] = "";
    }

    state["players"][id]["hasCar"] = true;
    state["players"][id]["speed"] = static_cast<int>((car.GetVelocity().magnitude() * .036f) + .5f); // Speed in uu/s, 1uu = 1cm, multiply by .036f to get from cm/s to km/h
    float boost = car.GetBoostComponent().IsNull() ? 0 : car.GetBoostComponent().GetPercentBoostFull();
    state["players"][id]["boost"] = static_cast<int>(boost * 100);
    state["players"][id]["isSonic"] = car.GetbSuperSonic() ? true : false;
}
void SOS::GetTeamInfo(json::JSON& state, ServerWrapper server)
{
    //Not enough teams
    if (server.GetTeams().Count() != 2)
    {
        state["game"]["teams"][0]["name"] = "BLUE";
        state["game"]["teams"][0]["score"] = 0;
        state["game"]["teams"][1]["name"] = "ORANGE";
        state["game"]["teams"][1]["score"] = 0;
        return;
    }

    TeamWrapper team0 = server.GetTeams().Get(0);
    if (!team0.IsNull())
    {
        state["game"]["teams"][0]["name"] = team0.GetCustomTeamName().IsNull() ? "BLUE" : team0.GetCustomTeamName().ToString();
        state["game"]["teams"][0]["score"] = team0.GetScore();
    }
    else
    {
        state["game"]["teams"][0]["name"] = "BLUE";
        state["game"]["teams"][0]["score"] = 0;
    }

    TeamWrapper team1 = server.GetTeams().Get(1);
    if (!team1.IsNull())
    {
        state["game"]["teams"][1]["name"] = team1.GetCustomTeamName().IsNull() ? "ORANGE" : team1.GetCustomTeamName().ToString();
        state["game"]["teams"][1]["score"] = team1.GetScore();
    }
    else
    {
        state["game"]["teams"][1]["name"] = "ORANGE";
        state["game"]["teams"][1]["score"] = 0;
    }
}
void SOS::GetGameTimeInfo(json::JSON& state, ServerWrapper server)
{
    //Get the time difference between now and the last time the clock was updated (in UpdateClock() hook)
    //Use a static float so that the diff remains the same value if the clock is paused
    if (!isClockPaused)
    {
        diff = duration_cast<duration<float>>(steady_clock::now() - timeSnapshot).count();
    }

    //Add or subtract the time difference to/from the current time on the clock. +/- depends on overtime status
    float floatTime = 0;
    if (!server.GetbOverTime())
        floatTime = (float)server.GetSecondsRemaining() - diff;
    else
        floatTime = (float)server.GetSecondsRemaining() + diff;

    //Zero out time after game ends because UpdateClock() isn't called again
    if (floatTime < 0 || waitingForOvertimeToStart) floatTime = 0;

    //Store time in json
    state["game"]["time"] = !firstCountdownHit ? 300.f : (bool)server.GetbOverTime() ? server.GetSecondsRemaining() : floatTime;
    state["game"]["isOT"] = (bool)server.GetbOverTime();


    //EXTRA: help logging. Turn off by defining SHOULDLOG as 0
    LOGC(to_string((float)server.GetSecondsRemaining() - diff));
}
void SOS::GetBallInfo(json::JSON& state, ServerWrapper server)
{
    BallWrapper ball = server.GetBall();

    //Ball is null
    if (ball.IsNull())
    {
        state["game"]["ballSpeed"] = 0;
        state["game"]["ballTeam"] = 255;
        state["game"]["isReplay"] = false;
        return;
    }

	//Get ball info
	state["game"]["ballSpeed"] = static_cast<int>((ball.GetVelocity().magnitude() * .036f) + .5f); // Speed in uu/s, 1uu = 1cm, multiply by .036f to get from cm/s to km/h
	state["game"]["ballTeam"] = ball.GetHitTeamNum();
	state["game"]["isReplay"] = ball.GetbReplayActor() ? true : false;

	//Get Ball Location
	Vector ballLocation = ball.GetLocation();
	state["game"]["ballX"] = ballLocation.X;
	state["game"]["ballY"] = ballLocation.Y;
	state["game"]["ballZ"] = ballLocation.Z;
}
void SOS::GetWinnerInfo(json::JSON& state, ServerWrapper server)
{
    TeamWrapper winner = server.GetGameWinner();

    //Winning team is null
    if (winner.IsNull())
    {
        state["game"]["hasWinner"] = false;
        state["game"]["winner"] = "";
        return;
    }

    //Get the winning team
    state["game"]["hasWinner"] = true;
    state["game"]["winner"] = winner.GetCustomTeamName().IsNull() ? "" : winner.GetCustomTeamName().ToString();
}
void SOS::GetCameraInfo(json::JSON& state)
{
    CameraWrapper cam = gameWrapper->GetCamera();

    //Camera is null
    if (cam.IsNull())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    PriWrapper specPri = PriWrapper(reinterpret_cast<std::uintptr_t>(cam.GetViewTarget().PRI));

    //Target PRI is null
    if (specPri.IsNull())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    //Target PRI is the local player
    if (specPri.IsLocalPlayerPRI())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    //Get the target's name
    std::string name = specPri.GetPlayerName().IsNull() ? "" : specPri.GetPlayerName().ToString();
    state["game"]["hasTarget"] = true;
    state["game"]["target"] = name + "_" + std::to_string(specPri.GetSpectatorShortcut());
}

#ifdef USE_NAMEPLATES
void SOS::GetNameplateInfo(CanvasWrapper canvas)
{
    //This function is a "drawable", meaning it runs with every GameViewportClient.Tick()
    //It is registered as a drawable so things like the canvas.ProjectF method are accessible

    if(!gameWrapper->IsInOnlineGame()) { return; }
    if(!gameWrapper->GetLocalCar().IsNull()) { return; }
    ServerWrapper server = gameWrapper->GetOnlineGame();
    if(server.IsNull()) { return; }
    if(server.GetPlaylist().memory_address == NULL) { return; }
    if(server.GetPlaylist().GetPlaylistId() != 6 && server.GetPlaylist().GetPlaylistId() != 24) { return; }

    //Clamp to around 90fps to avoid sending too many updates, but keep smooth motion
    static steady_clock::time_point lastCallTime = steady_clock::now();
    float timeSinceLastCall = duration_cast<duration<float>>(steady_clock::now() - lastCallTime).count();
    if ((timeSinceLastCall * 1000) < 11.11f) {
        //cvarManager->log("Too early to send update");
        return;
    }
    lastCallTime = steady_clock::now();

    //Create nameplates JSON object
    json::JSON nameplatesState;
    nameplatesState["nameplates"] = json::Object();

    CameraWrapper camera = gameWrapper->GetCamera();
    if(camera.IsNull()) { return; }

    RT::Frustum frustum = RT::Frustum(canvas, camera);
    
    //Get information about each car
    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for(int i = 0; i != cars.Count(); ++i)
    {
        CarWrapper car = cars.Get(i);
        if(car.IsNull()) { continue; }
        PriWrapper pri = car.GetPRI();
        if(pri.IsNull()) { continue; }

        GetIndividualNameplate(canvas, frustum, nameplatesState, car);
    }

    //Get information about the ball
    BallWrapper ball = server.GetBall();
    if(!ball.IsNull())
    {
        bool isVisible = frustum.IsInFrustum(ball.GetLocation(), ball.GetRadius());

        Vector2F ballPosition = canvas.ProjectF(ball.GetLocation());
        Vector2 canvasSize = canvas.GetSize();

        nameplatesState["nameplates"]["ball"]["isvisible"] = isVisible;
        nameplatesState["nameplates"]["ball"]["position"]["x"] = isVisible ? (ballPosition.X / canvasSize.X) : 0;
        nameplatesState["nameplates"]["ball"]["position"]["y"] = isVisible ? (ballPosition.Y / canvasSize.Y) : 0;
        nameplatesState["nameplates"]["ball"]["position"]["depth"] = isVisible ? (ball.GetLocation() - camera.GetLocation()).magnitude() : 0;
        nameplatesState["nameplates"]["ball"]["radius"] = GetBallVerticalRadius(canvas, ball, camera, frustum);
    }
    else
    {
        nameplatesState["nameplates"]["ball"]["isvisible"] = false;
        nameplatesState["nameplates"]["ball"]["position"]["x"] = 0;
        nameplatesState["nameplates"]["ball"]["position"]["y"] = 0;
        nameplatesState["nameplates"]["ball"]["position"]["depth"] = 10000;
        nameplatesState["nameplates"]["ball"]["radius"] = 1;
    }

    SendEvent("game:nameplate_tick", nameplatesState);
}
void SOS::GetIndividualNameplate(CanvasWrapper canvas, RT::Frustum &frustum, json::JSON& nameplatesState, CarWrapper car)
{
    CameraWrapper camera = gameWrapper->GetCamera();
    PriWrapper pri = car.GetPRI();

    int key = pri.GetSpectatorShortcut();
    std::string name = pri.GetPlayerName().IsNull() ? "" : pri.GetPlayerName().ToString();
    std::string id = name + "_" + std::to_string(key);
    
    //Location of nameplate in 3D space
    Vector nameplateLoc = car.GetLocation() + Vector{0,0,60};

    bool isVisible = frustum.IsInFrustum(nameplateLoc, 100);
    nameplatesState["nameplates"]["players"][id]["isvisible"] = isVisible;

    //Return empty values if the player is off screen
    if(!isVisible)
    {
        nameplatesState["nameplates"]["players"][id]["position"]["x"] = 0;
        nameplatesState["nameplates"]["players"][id]["position"]["y"] = 0;
        nameplatesState["nameplates"]["players"][id]["position"]["depth"] = 10000;
        nameplatesState["nameplates"]["players"][id]["scale"] = 1;
        return;
    }

    // GET NAMEPLATE POSITION, SCALE, AND DEPTH //

    //Get FOV interpolation amount
    float inFOV = camera.GetFOV();
    if(inFOV > 90)
	{
		inFOV = 90;
	}
	float FOVPerc = inFOV / 90;
	float FOVInterp = (FOVMin * (1 - FOVPerc)) + (FOVMax * FOVPerc);

    //Get distance interpolation amount
	float distMag = (nameplateLoc - camera.GetLocation()).magnitude();
    if(distMag > 10000)
	{
		distMag = 10000;
	}
	float distPerc = distMag / 10000;
	float distInterp = (distMin * (1 - distPerc)) + (distMax * distPerc);

    //Get the base visual scale
	float visScale = RT::GetVisualDistance(canvas, frustum, camera, nameplateLoc);

    //Get the final scale value
	float finalInterp = visScale * (FOVInterp * distInterp);


    Vector2F nameplatePosition = canvas.ProjectF(nameplateLoc);
    Vector2 canvasSize = canvas.GetSize();

    nameplatesState["nameplates"]["players"][id]["position"]["x"] = nameplatePosition.X / canvasSize.X;
    nameplatesState["nameplates"]["players"][id]["position"]["y"] = nameplatePosition.Y / canvasSize.Y;
    nameplatesState["nameplates"]["players"][id]["position"]["depth"] = (nameplateLoc - camera.GetLocation()).magnitude();
    nameplatesState["nameplates"]["players"][id]["scale"] = finalInterp;
}
float SOS::GetBallVerticalRadius(CanvasWrapper canvas, BallWrapper ball, CameraWrapper camera, RT::Frustum &frustum)
{
    //Perspective view will stretch ball horizontally, so just get the vertical radius

    // Get the ball's center location
    //   - If that is outside the frustum (with a buffer radius of ball's size), return 1
    // From that location, find the point camera.Up * ball.GetRadius
    //   - If that calculated location is outside the frustum, use the line that goes along -camera.Up
    // Return 2D magnitude of difference between both Vector2Fs (center and camera.Up)

    Vector ballLoc = ball.GetLocation();
    float ballRadius = ball.GetRadius();

    if(!frustum.IsInFrustum(ballLoc, ballRadius))
    {
        // Ball is off screen
        // Don't return 0 in case someone uses vertical radius in a division calculation
        // 1 pixel will be sufficiently small
        return 1;
    }

    RT::Matrix3 cameraMat(camera.GetRotation());
    Vector verticalPoint = cameraMat.up * ballRadius + ballLoc;
    if(!frustum.IsInFrustum(verticalPoint, 0))
    {
        verticalPoint = cameraMat.up * -ballRadius + ballLoc;
    }

    Vector2F center = canvas.ProjectF(ballLoc);
    Vector2F edge = canvas.ProjectF(verticalPoint);
    Vector2F diff = edge - center;

    return sqrtf(diff.X * diff.X + diff.Y * diff.Y);
}
#endif

/* TIME UPDATING CODE */
void SOS::UpdateClock()
{
    if (!(*enabled) && !matchCreated && !isInReplay) { return; } // Cancel function call from hook

    //This function is called once per second when the scoreboard time updates
    ServerWrapper server = gameWrapper->GetOnlineGame();
    if (!gameWrapper->IsInOnlineGame() || server.IsNull())
    {
        return;
    }

    //Reset waiting for overtime flag
    if (waitingForOvertimeToStart)
    {
        //cvarManager->log("Resetting OT clock waiter");
        waitingForOvertimeToStart = false;
        return;
    }

    //Unpause clock
    isClockPaused = false;

    //Register round as active to bypass ball touch clock reset
    //Refer to UnpauseClockOnBallTouch() for more information
    newRoundActive = true;

    //Save the time that the gametime was updated to get float time difference since the last time this function was called
    timeSnapshot = steady_clock::now();
}
void SOS::PauseClockOnGoal()
{
    //cvarManager->log("PauseClockOnGoal " + std::to_string(clock()));
    if (!(*enabled)) { return; } // Cancel function call from hook

    //Pause clock
    isClockPaused = true;

    //Don't register a new round started because the ball touch info will fire during the replay and start the clock again
    newRoundActive = false;
}
void SOS::UnpauseClockOnBallTouch()
{
    /*
    if(!(*enabled) || isInReplay) { return; } // Cancel function call from hook

    //If a new round has started after a goal, this function will unpause the float clock on the first ball touch of a new round
    //The update clock hook doesn't update the clock until the int time on the scoreboard changes, which isn't always in sync with the first touch
    if(!newRoundActive)
    {
        isClockPaused = false;
        newRoundActive = true;
    }
    */
}
void SOS::PauseClockOnOvertimeStarted()
{
    if (!(*enabled)) { return; } // Cancel function call from hook

    //Pause clock
    isClockPaused = true;

    //Register a new round so that the first ball touch will restart the clock
    newRoundActive = true;
    waitingForOvertimeToStart = true;
}

/* IN GAME EVENTS FEED CODE */
void SOS::OnStatEvent(ServerWrapper caller, void* args) {
    auto tArgs = (DummyStatEventContainer*)args;

    auto victim = PriWrapper(tArgs->Victim);
    auto receiver = PriWrapper(tArgs->Receiver);
    std::string victimName = victim.IsNull() ? "" : victim.GetPlayerName().ToString();
    std::string victimId = victim.IsNull() ? "" : victimName + '_' + std::to_string(victim.GetSpectatorShortcut());
    std::string receiverName = receiver.IsNull() ? "" : receiver.GetPlayerName().ToString();
    std::string receiverId = receiver.IsNull() ? "" : receiverName + '_' + std::to_string(receiver.GetSpectatorShortcut());

    std::wstring ws(tArgs->StatEvent->Label);
    std::string eventStr = std::string(ws.begin(), ws.end()); 

    json::JSON statfeed;
    statfeed["type"] = eventStr;
    statfeed["main_target"]["name"] = receiverName;
    statfeed["main_target"]["id"] = receiverId;
    statfeed["secondary_target"]["name"] = victimName;
    statfeed["secondary_target"]["id"] = victimId;

    if(eventStr == "Goal")
    {
        json::JSON goalScoreData;
        goalScoreData["scorer"]["name"] = receiverName;
        goalScoreData["scorer"]["id"] = receiverId;
        SendEvent("game:goal_scored", goalScoreData);
    }

    SendEvent("game:statfeed_event", statfeed);
}

/* WEBSOCKET CODE */
void SOS::RunWsServer()
{
    cvarManager->log("[SOS] Starting WebSocket server");
    ws_server = new PluginServer();
    cvarManager->log("[SOS] Starting asio");
    ws_server->init_asio();
    ws_server->set_open_handler(websocketpp::lib::bind(&SOS::OnWsOpen, this, _1));
    ws_server->set_close_handler(websocketpp::lib::bind(&SOS::OnWsClose, this, _1));
    ws_server->set_message_handler(websocketpp::lib::bind(&SOS::OnWsMsg, this, _1, _2));
    ws_server->set_http_handler(websocketpp::lib::bind(&SOS::OnHttpRequest, this, _1));
    cvarManager->log("[SOS] Starting listen on port 49122");
    ws_server->listen(*port);
    cvarManager->log("[SOS] Starting accepting connections");
    ws_server->start_accept();
    ws_server->run();
}
void SOS::OnWsMsg(connection_hdl hdl, PluginServer::message_ptr msg)
{
    this->SendWebSocketPayload(msg->get_payload());
}
void SOS::OnHttpRequest(websocketpp::connection_hdl hdl)
{
    PluginServer::connection_ptr connection = ws_server->get_con_from_hdl(hdl);
    connection->append_header("Content-Type", "application/json");
    connection->append_header("Server", "SOS/1.4.0");

    if (connection->get_resource() == "/init")
    {
        json::JSON data;
        data["event"] = "init";
        data["data"] = "json here";

        connection->set_body(data.dump());

        connection->set_status(websocketpp::http::status_code::ok);
        return;
    }

    connection->set_body("Not found");
    connection->set_status(websocketpp::http::status_code::not_found);
}
void SOS::SendWebSocketPayload(std::string payload) {
    // broadcast to all connections
    try {
        for (connection_hdl it : *ws_connections) {
            ws_server->send(it, payload, websocketpp::frame::opcode::text);
        }
    }
    catch (std::exception e) {
        cvarManager->log("An error occured sending websocket event");
    }
}
