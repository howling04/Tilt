#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "LunaDevice.h"
#include "LunaGame.generated.h"
class ALunaPawn;
class ALunaCourse;
class SLunaHUD;

UENUM(BlueprintType)
enum class ELunaPhase:uint8 { Menu, DeviceSetup, Playing, Paused, Complete };

UCLASS()
class LUNAMVP_API ALunaController:public APlayerController
{
    GENERATED_BODY()
public:
    ALunaController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PlayerTick(float Dt) override;
private:
    TSharedPtr<SLunaHUD> Screen;
};

UCLASS(Blueprintable)
class LUNAMVP_API ALunaGame:public AGameModeBase
{
    GENERATED_BODY()
public:
    ALunaGame();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartRun(bool DeviceMode,int32 FromStage=0,bool Practice=false);
    void ShowMenu();
    void TogglePause();
    void ResetCheckpoint();
    void RequestCenter();
    void ShiftLayer(int32 Direction);
    void UpdateControls(ALunaController& PC,float Dt);
    void Notice(FString Text,float Seconds=2.5f);
    FString Task()const;
    FString StageName()const;
    float Progress()const;
    void Snapshot(FString Name);
    void Speak(const FString& Text,bool Interrupt=false);
    FVector NavigationTarget()const;
    FString Subtitle;
    float SubtitleRemaining=0;
    bool bMuted=false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) ELunaPhase Phase=ELunaPhase::Menu;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Stage=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Gate=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HeightLesson=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDeviceMode=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bSafetyLocked=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ALunaPawn> Moon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ALunaCourse> Course;
    FLunaDevice Device;
    bool bDiagnostics=false,bWaitRelease=false,bVerify=false,bPractice=false;
    FString Message;
    float MessageRemaining=0,RunTime=0,DockHold=0;
    int32 Resets=0;
private:
    FVector Checkpoint=FVector(0,0,300);
    int32 CheckpointLayer=0,CheckpointGate=0;
    bool bSecondCheckpoint=false,bWasFocused=true;
    float FailDelay=0;
    double StartedAt=0,VerifyPhaseTime=0;
    float VerifiedCampaignSeconds=0;
    int32 VerifyStep=0;
    TArray<FString> VerifyResults;
    UPROPERTY() TObjectPtr<class UAudioComponent> Narrator;
    TMap<FString,FString> VoiceAssets;
    FString LastGuidance;
    float ObstacleCueCooldown=0;
    bool bFieldShot=false,bPolarShot=false;
    void EnterStage(int32 NewStage);
    void FinishTraining();
    void LockDevice();
    void AdvanceCourse(float Dt);
    void VerifyTick(float Dt);
    void WriteVerification(bool Pass);
};
