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
