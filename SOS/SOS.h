#pragma once
#pragma comment( lib, "pluginsdk.lib" )

#include <set>
#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include "json.hpp"

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "RenderingTools.h"

using websocketpp::connection_hdl;

class SOS : public BakkesMod::Plugin::BakkesModPlugin
{
    typedef websocketpp::server<websocketpp::config::asio> PluginServer;
    typedef std::set<connection_hdl, std::owner_less<connection_hdl>> ConnectionSet;

public:
    void onLoad() override;
    void onUnload() override;

private:
    /* CVARS */
    std::shared_ptr<bool> enabled;
    std::shared_ptr<bool> useBase64;
    std::shared_ptr<int> port;

    /* FLOAT TIME ASSETS */
    float diff = 0;
    std::chrono::steady_clock::time_point timeSnapshot;
    bool isClockPaused = false;
    bool newRoundActive = false;
    bool waitingForOvertimeToStart = false;
    void EnabledChanged();

    /* NAMEPLATE SCALE VALUES */
    //FOV: Anything above 90 degrees remains at 1.0
	const float FOVMin = 0.1f; //0 degrees
	const float FOVMax = 1.0f; //90 degrees
	//DISTANCE: Anything above 10,000 cm remains at 9.0
	const float distMin = 0.5f; //0 centimeters
	const float distMax = 9.0f; //10,000 centimeters

    /* ORIGINAL SOS VARIABLES */
    std::shared_ptr<float> update_cvar;
    bool firstCountdownHit = false;
    bool matchCreated = false;
    bool isCurrentlySpectating = false;
    bool isInReplay = false;

    float ballCurrentSpeed = 0;
    float goalSpeed = 0;

    struct LastTouchInfo
    {
        std::string playerID;
        float speed = 0;
    };
    LastTouchInfo lastTouch;

    /* ORIGINAL SOS HOOKS */
    void HookBallExplode();
    void HookOnHitGoal();
    void HookMatchCreated();
    void HookMatchEnded();
    void HookCountdownInit();
    void HookPodiumStart();
    void HookGoalScored();
    void HookReplayStart();
    void HookReplayEnd();
    void HookReplayWillEnd();
    void HookCarBallHit(CarWrapper car, void * params, std::string funcName);
    void GetLastTouchInfo(CarWrapper car);

    /* SEND EVENTS */
    void SendEvent(std::string eventName, json::JSON jsawn);
    //void SendEvent(string eventName, ApiGame game);
    //void SendEvent(string eventName, ApiPlayer player);

    /* MAIN FUNCTION */
    void UpdateGameState();

    /* INDIVIDUAL FUNCTIONS */
    void GetBallCurrentSpeed();
    void GetPlayerInfo(json::JSON& state, PriWrapper pri);
    void GetTeamInfo(json::JSON& state, ServerWrapper server);
    void GetGameTimeInfo(json::JSON& state, ServerWrapper server);
    void GetBallInfo(json::JSON& state, ServerWrapper server);
    void GetWinnerInfo(json::JSON& state, ServerWrapper server);
    void GetCameraInfo(json::JSON& state);
    
    #ifdef USE_NAMEPLATES
    void GetNameplateInfo(CanvasWrapper canvas);
    void GetIndividualNameplate(CanvasWrapper canvas, RT::Frustum &frustum, json::JSON& nameplatesState, CarWrapper car);
    float GetBallVerticalRadius(CanvasWrapper canvas, BallWrapper ball, CameraWrapper camera, RT::Frustum &frustum);
    #endif

    /* TIME UPDATING CODE */
    void UpdateClock();
    void PauseClockOnGoal();
    void UnpauseClockOnBallTouch();
    void PauseClockOnOvertimeStarted();

    void OnStatEvent(ServerWrapper caller, void* args);
    struct DummyStatEvent {
        char pad[144];
        wchar_t* Label;
    };

    struct DummyStatEventContainer
    {
        uintptr_t Receiver;
        uintptr_t Victim;
        DummyStatEvent* StatEvent;
    };

    /* WEBSOCKET CODE */
    PluginServer* ws_server;
    ConnectionSet* ws_connections;
    void RunWsServer();
    void OnHttpRequest(connection_hdl hdl);
    //void SendWsPayload(string payload);
    void SendWebSocketPayload(std::string payload);
    void OnWsMsg(connection_hdl hdl, PluginServer::message_ptr msg);
    void OnWsOpen(connection_hdl hdl) { this->ws_connections->insert(hdl); }
    void OnWsClose(connection_hdl hdl) { this->ws_connections->erase(hdl); }


    // Helpers because Simple is bad at coding and organizing
    std::string& replace(std::string& s, const std::string& from, const std::string& to)
    {
        if (!from.empty())
            for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
                s.replace(pos, from.size(), to);
        return s;
    }
};