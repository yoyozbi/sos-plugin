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
    // Goal Real Size: (1920, 752) - Extends past posts and ground
    // Goal Scoreable Zone: (1800, 640)
    // Ball Radius: ~93

    BallHitGoalParams* newParams = (BallHitGoalParams*)params;
    GoalWrapper goal(newParams->GoalPointer);

    if(goal.memory_address == NULL) { return {0,0}; }

    //Get goal direction and correct rounding errors. Results should either be 0 or +-1
    Vector GoalDirection = RT::Matrix3(goal.GetGoalOrientation().GetRotation()).forward;        
    GoalDirection.X = abs(GoalDirection.X) < .5f ? 0.f : GoalDirection.X / abs(GoalDirection.X);
    GoalDirection.Y = abs(GoalDirection.Y) < .5f ? 0.f : GoalDirection.Y / abs(GoalDirection.Y);
    GoalDirection.Z = abs(GoalDirection.Z) < .5f ? 0.f : GoalDirection.Z / abs(GoalDirection.Z);

    static const Vector2F GoalSize = {1800, 640}; // Scoreable zone
    float HitX = ((newParams->HitLocation.X * GoalDirection.Y) + (GoalSize.X / 2)) / GoalSize.X;
    float HitY = (GoalSize.Y - newParams->HitLocation.Z) / GoalSize.Y;

    return Vector2F{ HitX, HitY };
}
