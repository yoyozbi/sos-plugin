#include "SOS.h"

/*
    This is a tweaked version of DanMB's GameStateApi: https://github.com/DanMB/GameStateApi
    A lot of features merged in from the original SOS plugin: https://gitlab.com/bakkesplugins/sos/sos-plugin
    
    - Tweaked by CinderBlock
    - Thanks to Martinn for the Stat Feed code (and inadvertently, demolitions)
*/


BAKKESMOD_PLUGIN(SOS, "Simple Overlay System", "1.5.0", PLUGINTYPE_THREADED)

void SOS::onLoad()
{
    //Cvars
    cvarEnabled = std::make_shared<bool>(false);
    cvarUseBase64 = std::make_shared<bool>(false);
    cvarPort = std::make_shared<int>(49122);
    cvarUpdateRate = std::make_shared<float>(100.0f);

    CVarWrapper registeredEnabledCvar = cvarManager->registerCvar("SOS_Enabled", "1", "Enable SOS plugin", true, true, 0, true, 1);
    registeredEnabledCvar.bindTo(cvarEnabled);
    registeredEnabledCvar.addOnValueChanged(std::bind(&SOS::OnEnabledChanged, this));
    
    cvarManager->registerCvar("SOS_use_base64", "0", "Use base64 encoding to send websocket info (useful for non ASCII characters)", true, true, 0, true, 1).bindTo(cvarUseBase64);
    cvarManager->registerCvar("SOS_Port", "49122", "Websocket port for SOS overlay plugin", true).bindTo(cvarPort);
    cvarManager->registerCvar("SOS_state_flush_rate", "100", "Rate at which to send events to websocket (milliseconds)", true, true, 5.0f, true, 2000.0f).bindTo(cvarUpdateRate);
    //TAKE SPECIAL NOTE OF THIS  ^^^  The "SOS" here used to be lowercase specifically for (SOS_state_flush_rate), but I wanted it to be consistent. May break things
    //If it breaks, the change also needs to be reverted in sos.set

    //Notifiers
    cvarManager->registerNotifier("SOS_c_reset_internal_state", [this](std::vector<std::string> params) { HookMatchEnded(); }, "Reset internal state", PERMISSION_ALL);

    //Handle all the event hooking (EventHooks.cpp)
    HookAllEvents();

    //Check if there is a game currently active
    gameWrapper->SetTimeout([this](GameWrapper* gw)
    {
        if(ShouldRun())
        {
            HookMatchCreated();
        }
    }, 1.f);

    //Register drawable for nameplates (Nameplates.cpp)
    #ifdef USE_NAMEPLATES
    gameWrapper->RegisterDrawable(std::bind(&SOS::GetNameplateInfo, this, _1));
    #endif

    //Run websocket server (Websocket.cpp)
    RunWsServer();
}

void SOS::onUnload()
{
    StopWsServer();
}

void SOS::OnEnabledChanged()
{
    //If mod has been disabled, stop any potentially remaining clock calculations
    if (!*cvarEnabled)
    {
        isClockPaused = true;
        newRoundActive = false;
    }
}

bool SOS::ShouldRun()
{
    //Check if player is spectating
    if (!gameWrapper->GetLocalCar().IsNull())
    {
        LOGC("GetLocalCar().IsNull(): (need true) false");
        return false;
    }

    //Check if server exists
    ServerWrapper server = GetCurrentGameState();
    if (server.IsNull())
    {
        LOGC("server.IsNull(): (need false) true");
        return false;
    }

    //Check if server playlist exists
    if (server.GetPlaylist().memory_address == NULL)
    {
        LOGC("server.GetPlaylist().memory_address == NULL: (need false) true");
        return false;
    }

    //Check if server playlist is valid
    int playlistID = server.GetPlaylist().GetPlaylistId();
    if (playlistID != 6 && playlistID != 24)
    {
        LOGC("server.GetPlaylist().GetPlaylistId(): (need 6 or 24) " + std::to_string(playlistID));
        return false;
    }

    return true;
}
