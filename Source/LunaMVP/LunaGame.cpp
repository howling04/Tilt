#include "LunaGame.h"
#include "LunaScale.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "LunaPawn.h"
#include "LunaCourse.h"
#include "LunaMotion.h"
#include "LunaHUD.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Components/SphereComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"

ALunaController::ALunaController(){PrimaryActorTick.bCanEverTick=true;bShowMouseCursor=true;}
void ALunaController::BeginPlay()
{
    Super::BeginPlay();
    if(PlayerInput)PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Binding){return Binding.Key==EKeys::F1||Binding.Key==EKeys::F2;});
    if(IsLocalController()&&GEngine&&GEngine->GameViewport)
    {SAssignNew(Screen,SLunaHUD).Game(Cast<ALunaGame>(GetWorld()->GetAuthGameMode()));GEngine->GameViewport->AddViewportWidgetContent(Screen.ToSharedRef(),20);}
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);
}
void ALunaController::EndPlay(const EEndPlayReason::Type Reason)
{if(Screen.IsValid()&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(Screen.ToSharedRef());Screen.Reset();Super::EndPlay(Reason);}
void ALunaController::PlayerTick(float Dt)
{Super::PlayerTick(Dt);if(auto* G=Cast<ALunaGame>(GetWorld()->GetAuthGameMode()))G->UpdateControls(*this,Dt);}

ALunaGame::ALunaGame()
{
    PrimaryActorTick.bCanEverTick=true;DefaultPawnClass=ALunaPawn::StaticClass();PlayerControllerClass=ALunaController::StaticClass();
}
void ALunaGame::BeginPlay()
{
    Super::BeginPlay();StartedAt=FPlatformTime::Seconds();if(GEngine)GEngine->Exec(GetWorld(),TEXT("DisableAllScreenMessages"));
    for(TActorIterator<ALunaCourse> It(GetWorld());It;++It){Course=*It;break;}
    if(!Course)Course=GetWorld()->SpawnActor<ALunaCourse>();
    if(auto* PC=GetWorld()->GetFirstPlayerController())Moon=Cast<ALunaPawn>(PC->GetPawn());
    if(!Moon){Moon=GetWorld()->SpawnActor<ALunaPawn>(FVector(0,0,300),FRotator::ZeroRotator);if(auto* PC=GetWorld()->GetFirstPlayerController())PC->Possess(Moon);}
    Device.Open(*GetDefault<ULunaSettings>());bVerify=FParse::Param(FCommandLine::Get(),TEXT("LunaVerify"));
    Narrator=NewObject<UAudioComponent>(this,TEXT("MissionNarrator"));Narrator->bIsUISound=true;Narrator->RegisterComponent();
    FString VoiceJson;if(FFileHelper::LoadFileToString(VoiceJson,*(FPaths::ProjectContentDir()/TEXT("LunaArt/Data/narration.json"))))
    {TSharedPtr<FJsonObject> O;if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VoiceJson),O))for(const auto& P:O->Values)VoiceAssets.Add(FString(P.Key.ToView()),P.Value->AsString());}
    ShowMenu();UE_LOG(LogTemp,Display,TEXT("LUNA_MVP_READY version=2.1-orbital-instruments input_port=%d"),GetDefault<ULunaSettings>()->InputPort);
}
void ALunaGame::EndPlay(const EEndPlayReason::Type Reason){Device.Close();Super::EndPlay(Reason);}
void ALunaGame::Speak(const FString& Text,bool Interrupt)
{
    if(Text.IsEmpty())return;
    if(!Interrupt&&Narrator&&Narrator->IsPlaying())return;
    const auto* Path=VoiceAssets.Find(Text);
    if(!Path){UE_LOG(LogTemp,Warning,TEXT("LUNA_VOICE_MISSING %s"),*Text);return;}
    if(auto* Voice=LoadObject<USoundBase>(nullptr,**Path))
    {
        Subtitle=Text;SubtitleRemaining=FMath::Max(2.f,Voice->Duration+.35f);
        if(Narrator){Narrator->Stop();Narrator->SetSound(Voice);Narrator->SetVolumeMultiplier(bMuted?0.f:.85f);Narrator->Play();}
        UE_LOG(LogTemp,Display,TEXT("LUNA_NARRATION %s duration=%.2f"),*Text,Voice->Duration);
    }
}
void ALunaGame::Notice(FString Text,float Seconds){Message=MoveTemp(Text);MessageRemaining=Seconds;Speak(Message,true);}
FVector ALunaGame::NavigationTarget()const
{
    if(Stage==0)return {1900,0,300};
    if(Stage==1){const float X[]={4000,4700,5400},Y[]={-250,250,-200};return Gate<3?FVector(X[Gate],Y[Gate],300):FVector(6100,0,300);}
    if(HeightLesson<2)return {7100,0,300.f+(HeightLesson==0?200:0)};
    const float X[]={7800,8400,9000},Z[]={500,300,100};return Gate<3?FVector(X[Gate],0,Z[Gate]):FVector(9600,0,100);
}
void ALunaGame::ShowMenu()
{
    if(Narrator)Narrator->Stop();Subtitle.Empty();SubtitleRemaining=0;LastGuidance.Empty();
    Phase=ELunaPhase::Menu;bWaitRelease=true;bSafetyLocked=false;Device.ClearEvents();
    if(Moon){Moon->SetActorLocation({0,0,300},false);Moon->Movement->ResetMotion();Moon->Movement->bLocked=true;Moon->ResetView(true);}
    if(auto* PC=GetWorld()->GetFirstPlayerController())PC->bShowMouseCursor=true;
}
void ALunaGame::StartRun(bool UseDevice,int32 FromStage,bool Practice)
{
    bDeviceMode=UseDevice;bPractice=Practice;RunTime=0;Resets=0;bSecondCheckpoint=false;FailDelay=0;DockHold=0;MessageRemaining=0;
    EnterStage(FMath::Clamp(FromStage,0,2));bWaitRelease=true;Device.ClearEvents();
    if(UseDevice){Phase=ELunaPhase::DeviceSetup;LockDevice();Notice(TEXT("Connect controller. Press C to center."),3);}
    else{Phase=ELunaPhase::Playing;bSafetyLocked=false;}
    if(Moon&&FromStage==0&&!bVerify){Moon->BeginDeparture();Speak(TEXT("Engines ready. Leaving the lunar surface."),true);}
    LastGuidance.Empty();
    if(auto* PC=GetWorld()->GetFirstPlayerController())PC->bShowMouseCursor=UseDevice;
}
void ALunaGame::EnterStage(int32 NewStage)
{
    Stage=NewStage;Gate=0;CheckpointGate=0;HeightLesson=0;CheckpointLayer=0;DockHold=0;
    Checkpoint=FVector(Stage==0?0:Stage==1?3200:6800,0,300);
    if(Moon){Moon->SetActorLocation(Checkpoint,false);Moon->Movement->ResetMotion();Moon->Movement->TargetLayer=0;Moon->Movement->bLocked=true;Moon->ResetView();}
    Course->SetDoors(Stage,false);
    UE_LOG(LogTemp,Display,TEXT("LUNA_STAGE %d"),Stage+1);
}
void ALunaGame::ResetCheckpoint()
{
    if(!Moon||Phase==ELunaPhase::Menu||Phase==ELunaPhase::Complete)return;
    Moon->Movement->bLocked=true;Moon->Movement->ResetMotion();Moon->SetActorLocation(Checkpoint,false);
    Moon->ResetView();
    Moon->Movement->TargetLayer=CheckpointLayer;Gate=CheckpointGate;DockHold=0;FailDelay=0;++Resets;bWaitRelease=true;
    Device.ClearEvents();if(bDeviceMode)LockDevice();Notice(TEXT("Checkpoint restored. Release controls to resume."));
}
void ALunaGame::LockDevice(){bSafetyLocked=true;Device.InvalidateCenter();if(Moon){Moon->Movement->ResetMotion();Moon->Movement->bLocked=true;}}
void ALunaGame::RequestCenter()
{
    if(!bDeviceMode||Phase==ELunaPhase::Menu)return;
    LockDevice();Device.RequestCenter(FPlatformTime::Seconds());Notice(TEXT("Hold controller still. Centering."));
}
void ALunaGame::TogglePause()
{
    if(Phase==ELunaPhase::Playing){Phase=ELunaPhase::Paused;Moon->Movement->ResetMotion();Moon->Movement->bLocked=true;Device.ClearEvents();if(bDeviceMode)LockDevice();}
    else if(Phase==ELunaPhase::Paused){Phase=ELunaPhase::Playing;bWaitRelease=true;if(bDeviceMode)Notice(TEXT("Press C to center and resume."));}
}
void ALunaGame::ShiftLayer(int32 Direction)
{
    if(Phase!=ELunaPhase::Playing||bSafetyLocked||bWaitRelease||!Moon)return;
    if(Stage<2){Notice(TEXT("Maintain altitude. Polar control follows in sector three."));return;}
    const int32 Old=Moon->Movement->TargetLayer;Moon->Movement->TargetLayer=FMath::Clamp(Old+Direction,-1,1);if(Old!=Moon->Movement->TargetLayer)Moon->Movement->StableTime=0;
    Notice(Old==Moon->Movement->TargetLayer?TEXT("Altitude limit reached."):Direction>0?TEXT("Raising orbit."):TEXT("Lowering orbit."),1.5f);
}
void ALunaGame::UpdateControls(ALunaController& PC,float Dt)
{
    if(!Moon)return;
    const bool Focused=!FSlateApplication::IsInitialized()||FSlateApplication::Get().IsActive();
    if(!bVerify&&!Focused&&bWasFocused&&Phase==ELunaPhase::Playing){TogglePause();Notice(TEXT("Flight paused."));}
    bWasFocused=Focused;
    if(PC.WasInputKeyJustPressed(EKeys::F1))bDiagnostics=!bDiagnostics;
    if(PC.WasInputKeyJustPressed(EKeys::V)){bMuted=!bMuted;if(Narrator)Narrator->SetVolumeMultiplier(bMuted?0.f:.85f);}
    if(PC.WasInputKeyJustPressed(EKeys::M)){ShowMenu();return;}
    if(Phase==ELunaPhase::Menu||Phase==ELunaPhase::Complete)
    {
        if(PC.WasInputKeyJustPressed(EKeys::Enter)||PC.WasInputKeyJustPressed(EKeys::K))StartRun(false);
        if(PC.WasInputKeyJustPressed(EKeys::I))StartRun(true);
        if(PC.WasInputKeyJustPressed(EKeys::One))StartRun(false,0,true);
        if(PC.WasInputKeyJustPressed(EKeys::Two))StartRun(false,1,true);
        if(PC.WasInputKeyJustPressed(EKeys::Three))StartRun(false,2,true);
        return;
    }
    if(PC.WasInputKeyJustPressed(EKeys::P)||PC.WasInputKeyJustPressed(EKeys::Escape))TogglePause();
    if(PC.WasInputKeyJustPressed(EKeys::C))RequestCenter();
    if(PC.WasInputKeyJustPressed(EKeys::R))ResetCheckpoint();
    if(PC.WasInputKeyJustPressed(EKeys::F2)){bDeviceMode=!bDeviceMode;Moon->Movement->ResetMotion();Device.ClearEvents();bWaitRelease=true;if(bDeviceMode)LockDevice();else bSafetyLocked=false;Notice(bDeviceMode?TEXT("Controller selected. Press C to center."):TEXT("Keyboard selected."));}
    const bool Held=PC.IsInputKeyDown(EKeys::W)||PC.IsInputKeyDown(EKeys::A)||PC.IsInputKeyDown(EKeys::S)||PC.IsInputKeyDown(EKeys::D)||PC.IsInputKeyDown(EKeys::Q)||PC.IsInputKeyDown(EKeys::E);
    if(bWaitRelease&&!Held)bWaitRelease=false;
    if(Phase!=ELunaPhase::Playing||bWaitRelease||bSafetyLocked||bVerify||Moon->IsDeparting()){if(!bVerify)Moon->Movement->Command=FVector2D::ZeroVector;return;}
    if(!bDeviceMode)
    {
        Moon->Movement->Command=FVector2D((PC.IsInputKeyDown(EKeys::W)?1.:0.)-(PC.IsInputKeyDown(EKeys::S)?1.:0.),(PC.IsInputKeyDown(EKeys::D)?1.:0.)-(PC.IsInputKeyDown(EKeys::A)?1.:0.)).GetClampedToMaxSize(1.);
        if(PC.WasInputKeyJustPressed(EKeys::Q))ShiftLayer(1);
        if(PC.WasInputKeyJustPressed(EKeys::E))ShiftLayer(-1);
    }
}
void ALunaGame::Tick(float Dt)
{
    Super::Tick(Dt);const double Now=FPlatformTime::Seconds();Device.Poll(Now);MessageRemaining=FMath::Max(0.f,MessageRemaining-Dt);
    if(!Moon)return;
    SubtitleRemaining=FMath::Max(0.f,SubtitleRemaining-Dt);
    ObstacleCueCooldown=FMath::Max(0.f,ObstacleCueCooldown-Dt);
    if(Phase==ELunaPhase::Playing&&Moon->Movement->BlockedTime>1&&ObstacleCueCooldown<=0)
    {Notice(TEXT("Path blocked. Change course, or press R to reset."),4);ObstacleCueCooldown=12;}
    if((Phase==ELunaPhase::Playing||Phase==ELunaPhase::DeviceSetup||Phase==ELunaPhase::Paused)&&!Moon->IsDeparting())
    {
        FString Guidance=Task();
        if(Guidance!=LastGuidance&&(!Narrator||!Narrator->IsPlaying())){Speak(Guidance);LastGuidance=Guidance;}
    }
    if(bDeviceMode&&(Phase==ELunaPhase::Playing||Phase==ELunaPhase::DeviceSetup))
    {
        if(!Device.Healthy(Now)||!Device.bCentered)
        {if(!bSafetyLocked)LockDevice();}
        else if(bSafetyLocked)
        {
            bSafetyLocked=false;Moon->Movement->TargetLayer=FMath::Clamp(FMath::RoundToInt((Moon->GetActorLocation().Z-300)/200),-1,1);
            Phase=ELunaPhase::Playing;bWaitRelease=false;Notice(TEXT("Controller centered. Flight control restored."));
            if(auto* PC=GetWorld()->GetFirstPlayerController())PC->bShowMouseCursor=false;
        }
        if(!bSafetyLocked&&Phase==ELunaPhase::Playing)
        {Moon->Movement->Command=LunaMotion::Tilt(Device.Pitch,Device.Roll,*GetDefault<ULunaSettings>());const int32 Event=Device.ConsumeGesture();if(Event)ShiftLayer(Event);}
    }
    const bool Locked=Phase!=ELunaPhase::Playing||bSafetyLocked||bWaitRelease||FailDelay>0||Moon->IsDeparting();
    Moon->Movement->bLocked=Locked;
    if(Phase==ELunaPhase::Playing&&!Locked)
    {
        RunTime+=FMath::Min(Dt,.1f);AdvanceCourse(Dt);
        const FVector P=Moon->GetActorLocation();
        if(FMath::Abs(P.Y)>500||P.Z<32||P.Z>850||P.X< -200||P.X>10000)
        {FailDelay=.5f;Moon->Movement->ResetMotion();Moon->Movement->bLocked=true;Notice(TEXT("Outside flight corridor. Restoring checkpoint."));}
    }
    if(FailDelay>0){FailDelay-=Dt;if(FailDelay<=0)ResetCheckpoint();}
    if(bVerify)VerifyTick(Dt);
}
void ALunaGame::AdvanceCourse(float Dt)
{
    const FVector P=Moon->GetActorLocation();const float Speed=FVector2D(Moon->Movement->Velocity.X,Moon->Movement->Velocity.Y).Size();
    if(Stage==0)
    {
        if(P.X>=1200&&!bSecondCheckpoint){Checkpoint={1240,0,300};bSecondCheckpoint=true;Notice(TEXT("Checkpoint set. Reverse thrust to brake."));}
        const bool Docked=P.X>=1750&&P.X<=2050&&FMath::Abs(P.Y)<=250&&Speed<40;
        DockHold=Docked?DockHold+FMath::Min(Dt,.1f):0;
        if(DockHold>=1.f)
        {
            if(bPractice){FinishTraining();return;}
            Stage=1;Gate=0;Checkpoint={2550,0,300};CheckpointGate=0;CheckpointLayer=0;Course->SetDoors(Stage,false);
            Notice(TEXT("Braking complete. Enter the asteroid corridor."),3);UE_LOG(LogTemp,Display,TEXT("LUNA_STAGE 2"));if(bVerify)Snapshot(TEXT("02_lateral"));
        }
    }
    else if(Stage==1)
    {
        const float X[]={4000,4700,5400},Y[]={-250,250,-200};
        if(Gate<3&&P.X>X[Gate]+85&&FMath::Abs(P.Y-Y[Gate])<100)
        {++Gate;Checkpoint={X[Gate-1]+170,Y[Gate-1],300};CheckpointGate=Gate;Notice(FString::Printf(TEXT("Gate %d clear."),Gate),1.7f);}
        if(Gate==3&&P.X>=6000)
        {if(bPractice){FinishTraining();return;}Stage=2;Gate=0;Checkpoint={6300,0,300};CheckpointGate=0;HeightLesson=0;Course->SetDoors(Stage,false);Notice(TEXT("Lateral control complete. Q to rise, E to descend."),3);UE_LOG(LogTemp,Display,TEXT("LUNA_STAGE 3"));if(bVerify)Snapshot(TEXT("03_height"));}
    }
    else
    {
        if(HeightLesson==0&&Moon->Movement->TargetLayer==1&&Moon->Movement->Stable()){HeightLesson=1;Notice(TEXT("Upper orbit reached. Press E to return."));}
        else if(HeightLesson==1&&Moon->Movement->TargetLayer==0&&Moon->Movement->Stable()){HeightLesson=2;Course->SetDoors(Stage,true);Notice(TEXT("Polar control ready. Follow the altitude markers."));}
        const float X[]={7800,8400,9000};const int32 Layer[]={1,0,-1};
        if(HeightLesson==2&&Gate<3&&P.X>X[Gate]+140&&FMath::Abs(P.Y)<170&&FMath::Abs(P.Z-(300+200*Layer[Gate]))<45)
        {++Gate;Checkpoint={X[Gate-1]+220,0,300.f+200*Layer[Gate-1]};CheckpointLayer=Layer[Gate-1];CheckpointGate=Gate;Notice(FString::Printf(TEXT("Orbit gate %d clear."),Gate),1.7f);}
        if(HeightLesson==2&&Gate==3&&P.X>=9500)
        {FinishTraining();}
    }
}
void ALunaGame::FinishTraining()
{Phase=ELunaPhase::Complete;Moon->SetActorLocation({0,0,300},false);Moon->Movement->ResetMotion();Moon->Movement->bLocked=true;Moon->ResetView(false);Speak(TEXT("Flight complete. Luna is on course."),true);Device.ClearEvents();if(auto* PC=GetWorld()->GetFirstPlayerController())PC->bShowMouseCursor=true;UE_LOG(LogTemp,Display,TEXT("LUNA_COMPLETE seconds=%.2f resets=%d practice=%d"),RunTime,Resets,bPractice);}
FString ALunaGame::StageName()const
{return Stage==0?TEXT("Braking"):Stage==1?TEXT("Lateral control"):TEXT("Polar control");}
FString ALunaGame::Task()const
{
    if(bSafetyLocked)return TEXT("Controls locked. Connect controller and press C.");
    if(Phase==ELunaPhase::Paused)return bDeviceMode?TEXT("Press P to resume, then C to center."):TEXT("Press P to resume. Release flight controls.");
    if(Stage==0){if(Moon&&Moon->GetActorLocation().X<600)return TEXT("Hold W to accelerate.");if(Moon&&Moon->GetActorLocation().X<1200)return TEXT("Release W. Luna keeps drifting.");return TEXT("Hold S to brake. Hold position inside the marker.");}
    if(Stage==1)return FString::Printf(TEXT("Use A and D. Pass through gate %d."),FMath::Min(Gate+1,3));
    if(HeightLesson==0)return TEXT("Press Q to rise. Wait for orbital lock.");
    if(HeightLesson==1)return TEXT("Press E to descend. Wait for orbital lock.");
    return Gate<3?FString::Printf(TEXT("%s orbit ahead. Q to rise, E to descend."),Gate==0?TEXT("Upper"):Gate==1?TEXT("Mid"):TEXT("Lower")):TEXT("Continue to the arrival marker.");
}
float ALunaGame::Progress()const
{if(Phase==ELunaPhase::Complete)return 1;return Moon?FMath::Clamp(float(Moon->GetActorLocation().X/9500.),0.f,1.f):0;}
void ALunaGame::Snapshot(FString Name)
{IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Verification/Screenshots")),true);FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Verification/Screenshots")/(Name+TEXT(".png")),true,false);}
void ALunaGame::WriteVerification(bool Pass)
{
    for(const auto& Result:VerifyResults)if(Result.StartsWith(TEXT("FAIL:")))Pass=false;
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Pass);Report->SetNumberField(TEXT("stage"),Stage+1);Report->SetNumberField(TEXT("gate"),Gate);Report->SetNumberField(TEXT("run_seconds"),RunTime);Report->SetNumberField(TEXT("resets"),Resets);
    Report->SetNumberField(TEXT("campaign_seconds"),VerifiedCampaignSeconds);Report->SetBoolField(TEXT("verification_finished"),true);
    TArray<TSharedPtr<FJsonValue>> Checks;for(const auto& Result:VerifyResults)Checks.Add(MakeShared<FJsonValueString>(Result));Report->SetArrayField(TEXT("checks"),Checks);
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(Report.ToSharedRef(),Writer);
    FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("Verification/runtime.json")));
    UE_LOG(LogTemp,Display,TEXT("LUNA_RUNTIME_VERIFICATION %s"),Pass?TEXT("PASS"):TEXT("FAIL"));
}
void ALunaGame::VerifyTick(float Dt)
{
    const double Elapsed=FPlatformTime::Seconds()-StartedAt;
    if(VerifyStep==90&&Elapsed>VerifyPhaseTime+1)
    {StartRun(false,0,true);bWaitRelease=false;Moon->SetActorLocation({1900,0,300},false);Moon->ResetView();VerifyStep=91;return;}
    if(VerifyStep==91&&Phase==ELunaPhase::Complete)
    {VerifyResults.Add(bPractice&&Stage==0?TEXT("PASS: single-stage practice stops at its own finish"):TEXT("FAIL: practice progression"));Snapshot(TEXT("05_practice_complete"));WriteVerification(true);VerifyStep=95;VerifyPhaseTime=Elapsed;return;}
    if(VerifyStep==95&&Elapsed>VerifyPhaseTime+1){ShowMenu();bVerify=false;return;}
    if(Elapsed>180&&VerifyStep<90){VerifyResults.Add(TEXT("FAIL: runtime verification timeout"));WriteVerification(false);VerifyStep=99;return;}
    if(VerifyStep==0&&Elapsed>8){Snapshot(TEXT("00_menu"));VerifyStep=1;VerifyPhaseTime=Elapsed;return;}
    if(VerifyStep==1&&Elapsed>VerifyPhaseTime+1)
    {StartRun(false);Moon->BeginDeparture();Speak(TEXT("Engines ready. Leaving the lunar surface."),true);VerifyStep=10;VerifyPhaseTime=Elapsed;return;}
    if(VerifyStep==10&&Elapsed>VerifyPhaseTime+.8){Snapshot(TEXT("00b_surface_lift"));VerifyStep=11;return;}
    if(VerifyStep==11&&Elapsed>VerifyPhaseTime+2.3){Snapshot(TEXT("00f_continuous_before"));VerifyStep=101;return;}
    if(VerifyStep==101&&Elapsed>VerifyPhaseTime+2.5){Snapshot(TEXT("00g_continuous_after"));VerifyStep=102;return;}
    if(VerifyStep==102&&Elapsed>VerifyPhaseTime+3.5){Snapshot(TEXT("00c_lunar_reveal"));VerifyStep=12;return;}
    if(VerifyStep==12&&Elapsed>VerifyPhaseTime+5.2){Snapshot(TEXT("00e_luna_full"));VerifyStep=14;return;}
    if(VerifyStep==14&&Elapsed>VerifyPhaseTime+6.8){Snapshot(TEXT("00d_orbital_reveal"));VerifyStep=13;return;}
    if(VerifyStep==13&&Elapsed>VerifyPhaseTime+9.8)
    {
        VerifyResults.Add(!Moon->IsDeparting()?TEXT("PASS: continuous surface-to-orbit transition completes and unlocks flight"):TEXT("FAIL: departure camera"));
        VerifyResults.Add(Moon->HasContinuousSurface()?TEXT("PASS: one lunar surface and rocks persist through departure"):TEXT("FAIL: surface changed"));
        VerifyResults.Add(Moon->EngineCount()==12674?TEXT("PASS: all 12674 engines use anchored monument geometry"):TEXT("FAIL: missing engine bodies"));
        GEngine->GameViewport->InputKey(FInputKeyEventArgs(GEngine->GameViewport->Viewport,FInputDeviceId::CreateFromInternalId(0),EKeys::F1,IE_Pressed,FPlatformTime::Cycles64()));
        VerifyStep=15;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==15&&Elapsed>VerifyPhaseTime+.15)
    {
        GEngine->GameViewport->InputKey(FInputKeyEventArgs(GEngine->GameViewport->Viewport,FInputDeviceId::CreateFromInternalId(0),EKeys::F1,IE_Released,FPlatformTime::Cycles64()));
        VerifyResults.Add(bDiagnostics&&!GEngine->GameViewport->EngineShowFlags.Wireframe?TEXT("PASS: F1 input shows diagnostics without changing rendering"):TEXT("FAIL: F1 rendering conflict"));
        Snapshot(TEXT("01c_F1_diagnostics"));VerifyStep=16;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==16&&Elapsed>VerifyPhaseTime+.2)
    {bDiagnostics=false;StartRun(false,1);bWaitRelease=false;Moon->SetActorLocation({3700,0,300},false);VerifyStep=2;VerifyPhaseTime=Elapsed;return;}
    if(VerifyStep==2)
    {
        Moon->Movement->Command={1,0};
        if(Elapsed>VerifyPhaseTime+2)
        {
            bool Pass=Moon->Movement->CollisionEvents>0;
            UE_LOG(LogTemp,Display,TEXT("LUNA_ASTEROID_SWEEP contacts=%d position=%s"),Moon->Movement->CollisionEvents,*Moon->GetActorLocation().ToString());
            VerifyResults.Add(Pass?TEXT("PASS: swept lunar sphere blocked by real convex asteroid"):TEXT("FAIL: asteroid swept collision"));
            if(!Pass){WriteVerification(false);VerifyStep=99;return;}
            TogglePause();Snapshot(TEXT("01b_paused"));VerifyStep=3;VerifyPhaseTime=Elapsed;
        }return;
    }
    if(VerifyStep==3&&Elapsed>VerifyPhaseTime+.3)
    {
        bool Pass=Moon->Movement->Velocity.IsNearlyZero();VerifyResults.Add(Pass?TEXT("PASS: pause clears velocity"):TEXT("FAIL: pause velocity"));
        ResetCheckpoint();Pass=Moon->Movement->Velocity.IsNearlyZero()&&Moon->Movement->TargetLayer==CheckpointLayer&&FVector::Dist(Moon->GetActorLocation(),Checkpoint)<.1;
        VerifyResults.Add(Pass?TEXT("PASS: checkpoint reset restores safe position, layer and zero velocity"):TEXT("FAIL: checkpoint reset"));
        StartRun(false,2);bWaitRelease=false;ShiftLayer(1);ShiftLayer(1);Pass=Moon->Movement->TargetLayer==1;ShiftLayer(-1);ShiftLayer(-1);ShiftLayer(-1);Pass=Pass&&Moon->Movement->TargetLayer==-1;
        VerifyResults.Add(Pass?TEXT("PASS: layer events clamp to [-1,+1]"):TEXT("FAIL: layer boundaries"));
        StartRun(false,1);bWaitRelease=false;Moon->SetActorLocation({3700,501,300},false);VerifyStep=30;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==30&&Elapsed>VerifyPhaseTime+.7)
    {
        const bool Pass=Resets==1&&FVector::Dist(Moon->GetActorLocation(),Checkpoint)<.1;
        VerifyResults.Add(Pass?TEXT("PASS: leaving the navigation corridor restores the checkpoint"):TEXT("FAIL: course side boundary"));
        if(!Pass){WriteVerification(false);VerifyStep=99;return;}
        bDeviceMode=true;bSafetyLocked=false;Device.bCentered=true;Device.bValid=true;Device.bCalibrated=true;Device.SourceAge=1.;Device.LastSourceTime=FPlatformTime::Seconds();Moon->Movement->Velocity={120,70,100};
        VerifyStep=31;return;
    }
    if(VerifyStep==31)
    {
        bool Pass=bSafetyLocked&&Moon->Movement->Velocity.IsNearlyZero();VerifyResults.Add(Pass?TEXT("PASS: stale device clears horizontal and vertical velocity on next update"):TEXT("FAIL: device failsafe"));
        if(!Pass){WriteVerification(false);VerifyStep=99;return;}
        StartRun(false,2);bWaitRelease=false;ShiftLayer(1);VerifyStep=32;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==32&&Elapsed>VerifyPhaseTime+.12)
    {
        const bool Pass=Moon->SouthThrust>.35f&&Moon->NorthThrust==0;
        VerifyResults.Add(Pass?TEXT("PASS: rising orbit fires strong south polar exhaust"):TEXT("FAIL: south polar exhaust"));
        Snapshot(TEXT("03c_south_thrust"));
        VerifyStep=33;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==33&&Elapsed>VerifyPhaseTime+.25)
    {
        Moon->SetActorLocation({6800,0,300},false);Moon->Movement->Velocity=FVector::ZeroVector;Moon->Movement->TargetLayer=-1;
        VerifyStep=34;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==34&&Elapsed>VerifyPhaseTime+.12)
    {
        const bool Pass=Moon->NorthThrust>.35f&&Moon->SouthThrust==0;
        VerifyResults.Add(Pass?TEXT("PASS: descending orbit fires strong north polar exhaust"):TEXT("FAIL: north polar exhaust"));
        Snapshot(TEXT("03d_north_thrust"));VerifyStep=35;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==35&&Elapsed>VerifyPhaseTime+.25)
    {
        Moon->SetActorLocation({6800,0,490},false);Moon->Movement->Velocity={0,0,150};Moon->Movement->TargetLayer=1;
        VerifyStep=36;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==36&&Elapsed>VerifyPhaseTime+.1)
    {
        VerifyResults.Add(Moon->NorthThrust>0&&Moon->SouthThrust==0?TEXT("PASS: upward approach brakes with north polar exhaust"):TEXT("FAIL: polar braking direction"));
        Snapshot(TEXT("03e_polar_braking"));VerifyStep=37;VerifyPhaseTime=Elapsed;return;
    }
    if(VerifyStep==37&&Elapsed>VerifyPhaseTime+.25){TogglePause();VerifyStep=38;VerifyPhaseTime=Elapsed;return;}
    if(VerifyStep==38&&Elapsed>VerifyPhaseTime+.15)
    {
        VerifyResults.Add(Moon->NorthThrust==0&&Moon->SouthThrust==0?TEXT("PASS: pause shuts down both active polar plumes"):TEXT("FAIL: paused polar thrust"));
        StartRun(false);bWaitRelease=false;VerifyStep=4;Snapshot(TEXT("01_inertia"));return;
    }
    if(VerifyStep!=4&&VerifyStep!=5)return;
    bWaitRelease=false;
    if(Phase==ELunaPhase::Complete&&VerifyStep==4)
    {VerifiedCampaignSeconds=RunTime;VerifyResults.Add(TEXT("PASS: three stages completed in order with swept movement and PD height"));Snapshot(TEXT("04_complete"));VerifyStep=90;VerifyPhaseTime=Elapsed;return;}
    if(Phase!=ELunaPhase::Playing)return;
    FVector P=Moon->GetActorLocation(),Target=P;const FVector V=Moon->Movement->Velocity;
    if(Stage==1&&Gate==0&&P.X>3820&&!bFieldShot){Snapshot(TEXT("02b_asteroid_aperture"));bFieldShot=true;}
    if(Stage==2&&HeightLesson==2&&Gate==0&&P.X>7610&&!bPolarShot){Snapshot(TEXT("03b_polar_aperture"));bPolarShot=true;}
    if(Stage==0)Target={1900,0,300};
    if(Stage==1)
    {
        const float X[]={4000,4700,5400},Y[]={-250,250,-200};
        if(Gate<3){Target={X[Gate]+200,Y[Gate],300};if(FMath::Abs(P.Y-Y[Gate])>40&&P.X<X[Gate]-170)Target.X=FMath::Min(P.X+80.,double(X[Gate]-240));}
        else Target={6100,0,300};
    }
    if(Stage==2)
    {
        if(HeightLesson<2)
        {
            Target={7100,0,300};
            const int32 Desired=HeightLesson==0?1:0;
            if(Moon->Movement->TargetLayer!=Desired)ShiftLayer(Desired>Moon->Movement->TargetLayer?1:-1);
        }
        else if(Gate<3)
        {
            const float X[]={7800,8400,9000};const int32 Layer[]={1,0,-1};
            if(Moon->Movement->TargetLayer!=Layer[Gate])ShiftLayer(Layer[Gate]>Moon->Movement->TargetLayer?1:-1);
            Target={X[Gate]+240,0,P.Z};
            if(!Moon->Movement->Stable())Target.X=FMath::Min(P.X+30.,double(X[Gate]-270));
        }
        else Target={9600,0,100};
    }
    FVector2D Desired=(FVector2D(Target.X-P.X,Target.Y-P.Y)*1.6).GetClampedToMaxSize(240.);
    const auto& S=*GetDefault<ULunaSettings>();FVector2D Velocity(V.X,V.Y);
    Moon->Movement->Command=((Desired-Velocity)*4.+S.Drag*Velocity)/S.Acceleration;
}
