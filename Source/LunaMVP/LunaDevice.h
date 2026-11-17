#pragma once
#include "CoreMinimal.h"
class FSocket;
class ULunaSettings;

// Owned and polled only on the game thread. No UObject is touched by a receiver thread.
class FLunaDevice
{
public:
    ~FLunaDevice();
    bool Open(const ULunaSettings& Settings);
    void Close();
    void Poll(double Now);
    bool Consume(const FString& Json, double Now);
    bool Healthy(double Now) const;
    double Age(double Now) const;
    bool CenterReady(double Now) const;
    void RequestCenter(double Now);
    int32 ConsumeGesture();
    void ClearEvents();
    void InvalidateCenter();

    bool bBound=false, bValid=false, bCalibrated=false, bCentered=false;
    FString Status=TEXT("Waiting for controller"), BridgeId, BootId, PendingRequest;
    float Pitch=0, Roll=0;
    int64 SourceSeq=-1, GestureSeq=-1;
    int32 Accepted=0, Rejected=0, Duplicate=0;
    double LastSourceTime=-1e9, SourceAge=1e9, Stale=.25, RequestTime=0;
private:
    FSocket* Socket=nullptr;
    FString ControlAddress=TEXT("127.0.0.1");
    int32 ControlPort=7002, PendingGesture=0;
    int64 AckSource=-1;
    bool bAcked=false;
};
