#include "SOS.h"

/*
    This could do with a lot of cleanup. Might be able to remove `diff` and `timeSnapshot` from SOS.h
    Refer to GameTimeInfo() in GameState.cpp for usage of these variables
*/

void SOS::UpdateClock()
{
    //This function is called once per second when the scoreboard time updates

    if (!*cvarEnabled && !matchCreated && !isInReplay) { return; }

    ServerWrapper server = gameWrapper->GetOnlineGame();
    if (!gameWrapper->IsInOnlineGame() || server.IsNull()) { return; }

    //Reset waiting for overtime flag
    if (waitingForOvertimeToStart)
    {
        LOGC("Resetting OT clock waiter");
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
    if (!*cvarEnabled) { return; }

    LOGC("PauseClockOnGoal " + std::to_string(clock()));

    //Pause clock
    isClockPaused = true;

    //Don't register a new round started because the ball touch info will fire during the replay and start the clock again
    newRoundActive = false;
}
void SOS::UnpauseClockOnBallTouch()
{
    /*
    if (!(*enabled) || isInReplay) { return; } // Cancel function call from hook

    //If a new round has started after a goal, this function will unpause the float clock on the first ball touch of a new round
    //The update clock hook doesn't update the clock until the int time on the scoreboard changes, which isn't always in sync with the first touch
    if (!newRoundActive)
    {
        isClockPaused = false;
        newRoundActive = true;
    }
    */
}
void SOS::PauseClockOnOvertimeStarted()
{
    if (!*cvarEnabled) { return; } // Cancel function call from hook

    //Pause clock
    isClockPaused = true;

    //Register a new round so that the first ball touch will restart the clock
    newRoundActive = true;
    waitingForOvertimeToStart = true;
}
