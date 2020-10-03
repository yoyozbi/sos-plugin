#include "SOS.h"
#include "RenderingTools.h"

float SOS::ToKPH(float RawSpeed)
{
    //Raw speed from game is cm/s
    return (RawSpeed * 0.036f);
}

void SOS::LockBallSpeed()
{
    //Lock ball speed on ball explode or goal scored to get accurate speed
    bLockBallSpeed = true;
    gameWrapper->SetTimeout([this](GameWrapper* gw)
    {
        bLockBallSpeed = false;
    }, 2.f);
}

Vector2F SOS::GetGoalImpactLocation(BallWrapper ball, void* params, std::string funcName)
{
    //Get goal impact location

    std::string printable;

    BallHitGoalParams* newParams = (BallHitGoalParams*)params;
    GoalWrapper goal(newParams->GoalPointer);

    if(goal.memory_address != NULL)
    {
        ActorWrapper GoalOrientation = goal.GetGoalOrientation();
        Vector GoalLocation = GoalOrientation.GetLocation();
        Vector GoalDirection = RT::Matrix3(GoalOrientation.GetRotation()).forward;
        
        //Correct rounding errors. Results should either be 0 or 1
        GoalDirection.X = abs(GoalDirection.X) < .5f ? 0.f : GoalDirection.X / abs(GoalDirection.X);
        GoalDirection.Y = abs(GoalDirection.Y) < .5f ? 0.f : GoalDirection.Y / abs(GoalDirection.Y);
        GoalDirection.Z = abs(GoalDirection.Z) < .5f ? 0.f : GoalDirection.Z / abs(GoalDirection.Z);

        printable += " GoalLocation(" + std::to_string(GoalLocation.X) + ", " + std::to_string(GoalLocation.Y) + ", " + std::to_string(GoalLocation.Z) + ")";
        printable += " GoalDirection(" + std::to_string(GoalDirection.X) + ", " + std::to_string(GoalDirection.Y) + ", " + std::to_string(GoalDirection.Z) + ")";
    }
    printable += " HitLocation(" + std::to_string(newParams->HitLocation.X) + ", " + std::to_string(newParams->HitLocation.Y) + ", " + std::to_string(newParams->HitLocation.Z) + ")";

    cvarManager->log(printable);

    return Vector2F{0,0};
}
