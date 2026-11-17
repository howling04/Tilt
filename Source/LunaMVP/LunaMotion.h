#pragma once
#include "CoreMinimal.h"
#include "LunaSettings.h"
#include "LunaPropulsion.h"

namespace LunaMotion
{
    inline float TiltAxis(float Degrees, const ULunaSettings& S)
    {
        const float Range = FMath::Max(.01f, S.FullTiltDegrees - S.DeadZoneDegrees);
        return FMath::Sign(Degrees) * FMath::Clamp((FMath::Abs(Degrees)-S.DeadZoneDegrees)/Range,0.f,1.f);
    }
    inline FVector2D Tilt(float Pitch, float Roll, const ULunaSettings& S)
    {
        return FVector2D(TiltAxis(Pitch,S),TiltAxis(Roll,S)).GetClampedToMaxSize(1.);
    }
    inline FVector Step(FVector& Velocity, FVector2D Input, float Z, int32 Layer, float Dt, const ULunaSettings& S)
    {
        Input = Input.GetClampedToMaxSize(1.);
        FVector2D XY(Velocity.X,Velocity.Y);
        XY += (S.Acceleration*Input-S.Drag*XY)*Dt;
        XY = XY.GetClampedToMaxSize(S.MaxSpeed);
        Velocity.X=XY.X; Velocity.Y=XY.Y;
        const float Az=LunaPropulsion::VerticalAcceleration(Z,Velocity.Z,Layer,S);
        Velocity.Z=FMath::Clamp(Velocity.Z+Az*Dt,-S.MaxVerticalSpeed,S.MaxVerticalSpeed);
        return Velocity*Dt;
    }
    inline bool HeightStable(float Z, float Vz, int32 Layer)
    {
        return FMath::Abs(Z-(300.f+200.f*Layer))<=15.f && FMath::Abs(Vz)<=20.f;
    }
}
