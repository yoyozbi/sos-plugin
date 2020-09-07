#ifdef USE_NAMEPLATES

#include "SOS.h"
#include "json.hpp"
#include "RenderingTools.h"


void SOS::GetNameplateInfo(CanvasWrapper canvas)
{
    //This function is a "drawable", meaning it runs with every GameViewportClient.Tick
    //It is registered as a drawable so things like the canvas.ProjectF method are accessible
    //Otherwise it would be part of the HookViewportClientTick event

    if (!gameWrapper->IsInOnlineGame()) { return; }
    if (!gameWrapper->GetLocalCar().IsNull()) { return; }
    ServerWrapper server = gameWrapper->GetOnlineGame();
    if (server.IsNull()) { return; }
    if (server.GetPlaylist().memory_address == NULL) { return; }
    if (server.GetPlaylist().GetPlaylistId() != 6 && server.GetPlaylist().GetPlaylistId() != 24) { return; }

    //Clamp to around 90fps to avoid sending too many updates, but keep smooth motion
    static steady_clock::time_point lastCallTime = steady_clock::now();
    float timeSinceLastCall = duration_cast<duration<float>>(steady_clock::now() - lastCallTime).count();
    if ((timeSinceLastCall * 1000) < 11.11f) {
        LOGC("Too early to send nameplates update");
        return;
    }
    lastCallTime = steady_clock::now();

    //Create nameplates JSON object
    json::JSON nameplatesState;
    nameplatesState["nameplates"] = json::Object();

    CameraWrapper camera = gameWrapper->GetCamera();
    if (camera.IsNull()) { return; }

    RT::Frustum frustum = RT::Frustum(canvas, camera);
    
    //Get information about each car
    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for(int i = 0; i != cars.Count(); ++i)
    {
        CarWrapper car = cars.Get(i);
        if (car.IsNull()) { continue; }
        PriWrapper pri = car.GetPRI();
        if (pri.IsNull()) { continue; }

        GetIndividualNameplate(canvas, frustum, nameplatesState, car);
    }

    //Get information about the ball
    BallWrapper ball = server.GetBall();
    if (!ball.IsNull())
    {
        bool isVisible = frustum.IsInFrustum(ball.GetLocation(), ball.GetRadius());

        Vector2F ballPosition = canvas.ProjectF(ball.GetLocation());
        Vector2 canvasSize = canvas.GetSize();

        nameplatesState["nameplates"]["ball"]["isvisible"] = isVisible;
        nameplatesState["nameplates"]["ball"]["position"]["x"] = isVisible ? (ballPosition.X / canvasSize.X) : 0;
        nameplatesState["nameplates"]["ball"]["position"]["y"] = isVisible ? (ballPosition.Y / canvasSize.Y) : 0;
        nameplatesState["nameplates"]["ball"]["position"]["depth"] = isVisible ? (ball.GetLocation() - camera.GetLocation()).magnitude() : 10000;
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

    std::string name, id;
    GetNameAndID(car.GetPRI(), name, id);
    
    //Location of nameplate in 3D space
    Vector nameplateLoc = car.GetLocation() + Vector{0,0,60};

    bool isVisible = frustum.IsInFrustum(nameplateLoc, 100);
    nameplatesState["nameplates"]["players"][id]["isvisible"] = isVisible;

    //Return empty values if the player is off screen
    if (!isVisible)
    {
        nameplatesState["nameplates"]["players"][id]["position"]["x"] = 0;
        nameplatesState["nameplates"]["players"][id]["position"]["y"] = 0;
        nameplatesState["nameplates"]["players"][id]["position"]["depth"] = 10000;
        nameplatesState["nameplates"]["players"][id]["scale"] = 1;
        return;
    }

    //Get FOV interpolation amount
    float inFOV = camera.GetFOV();
    if (inFOV > 90)
	{
		inFOV = 90;
	}
	float FOVPerc = inFOV / 90;
	float FOVInterp = (FOVMin * (1 - FOVPerc)) + (FOVMax * FOVPerc);

    //Get distance interpolation amount
	float distMag = (nameplateLoc - camera.GetLocation()).magnitude();
    if (distMag > 10000)
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

    if (!frustum.IsInFrustum(ballLoc, ballRadius))
    {
        // Ball is off screen
        // Don't return 0 in case someone uses vertical radius in a division calculation
        // 1 pixel will be sufficiently small
        return 1;
    }

    RT::Matrix3 cameraMat(camera.GetRotation());
    Vector verticalPoint = cameraMat.up * ballRadius + ballLoc;
    if (!frustum.IsInFrustum(verticalPoint, 0))
    {
        verticalPoint = cameraMat.up * -ballRadius + ballLoc;
    }

    Vector2F center = canvas.ProjectF(ballLoc);
    Vector2F edge = canvas.ProjectF(verticalPoint);
    Vector2F diff = edge - center;

    return sqrtf(diff.X * diff.X + diff.Y * diff.Y);
}
#endif
