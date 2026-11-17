#pragma once
#include "CoreMinimal.h"
#include "LunaSettings.h"

namespace LunaPropulsion
{
    inline float VerticalAcceleration(float Z,float Vz,int32 Layer,const ULunaSettings& S)
    {
        const float Target=300.f+200.f*FMath::Clamp(Layer,-1,1);
        return FMath::Clamp(S.HeightKp*(Target-Z)-S.HeightKd*Vz,-S.MaxVerticalAcceleration,S.MaxVerticalAcceleration);
    }
    // Exhaust points away from the accelerating body. The opposite pole fires during braking.
    inline FVector2D PolarPower(float Az,float MaxAz,bool Locked)
    {
        const float Signed=Locked?0.f:FMath::Clamp(Az/FMath::Max(1.f,MaxAz),-1.f,1.f);
        return {FMath::Max(0.f,-Signed),FMath::Max(0.f,Signed)}; // north, south
    }
}
