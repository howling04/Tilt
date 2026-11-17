#include "LunaPawn.h"
#include "LunaGame.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "LunaMotion.h"
#include "LunaPropulsion.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

ULunaMovement::ULunaMovement(){PrimaryComponentTick.bCanEverTick=true;}
void ULunaMovement::ResetMotion(){Velocity=FVector::ZeroVector;Command=FVector2D::ZeroVector;Accumulator=0;StableTime=0;BlockedTime=0;HitFeedback=0;CollisionEvents=0;}
void ULunaMovement::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(Dt,Type,ThisTick);
    if(!UpdatedComponent||ShouldSkipUpdate(Dt))return;
    HitFeedback=FMath::Max(0.f,HitFeedback-Dt);
    if(bLocked){Velocity=FVector::ZeroVector;Command=FVector2D::ZeroVector;Accumulator=0;UpdateComponentVelocity();return;}
    const ULunaSettings& S=*GetDefault<ULunaSettings>();
    Accumulator+=FMath::Clamp(Dt,0.f,.1f);
    constexpr float H=1.f/120.f;
    bool bBlocked=false;
    while(Accumulator+1e-8>=H)
    {
        Accumulator-=H;
        FVector Delta=LunaMotion::Step(Velocity,Command,UpdatedComponent->GetComponentLocation().Z,TargetLayer,H,S);
        FHitResult Hit;
        SafeMoveUpdatedComponent(Delta,UpdatedComponent->GetComponentQuat(),true,Hit);
        if(Hit.IsValidBlockingHit())
        {
            bBlocked=true;HitFeedback=.22f;++CollisionEvents;
            const float Inward=FVector::DotProduct(Velocity,Hit.Normal);
            if(Inward<0)Velocity-=Hit.Normal*Inward;
            SlideAlongSurface(Delta,1.f-Hit.Time,Hit.Normal,Hit,true);
        }
        StableTime=LunaMotion::HeightStable(UpdatedComponent->GetComponentLocation().Z,Velocity.Z,TargetLayer)?StableTime+H:0.f;
    }
    BlockedTime=bBlocked?BlockedTime+Dt:0.f;
    UpdateComponentVelocity();
}
ALunaPawn::ALunaPawn()
{
    PrimaryActorTick.bCanEverTick=true;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));SetRootComponent(Collision);
    Collision->InitSphereRadius(30);Collision->SetCollisionProfileName(TEXT("Pawn"));Collision->SetCanEverAffectNavigation(false);
    VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("LunaAssembly"));VisualRoot->SetupAttachment(Collision);
    Surface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LunarSurface"));Surface->SetupAttachment(VisualRoot);Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeroThruster=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurfaceHeroThruster"));HeroThruster->SetupAttachment(VisualRoot);HeroThruster->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NearTerrain=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LunarForefield"));NearTerrain->SetupAttachment(VisualRoot);NearTerrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NorthJet=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthPolarEngine"));NorthJet->SetupAttachment(VisualRoot);NorthJet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SouthJet=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthPolarEngine"));SouthJet->SetupAttachment(VisualRoot);SouthJet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GlobalPlumes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GlobalPlasma"));GlobalPlumes->SetupAttachment(VisualRoot);GlobalPlumes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LocalEngines=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LocalEngineArray"));LocalEngines->SetupAttachment(VisualRoot);LocalEngines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FarEngines=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DistantEngineArray"));FarEngines->SetupAttachment(VisualRoot);FarEngines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PolarPlumes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PolarPlasma"));PolarPlumes->SetupAttachment(VisualRoot);PolarPlumes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PolarCores=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PolarCore"));PolarCores->SetupAttachment(VisualRoot);PolarCores->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement=CreateDefaultSubobject<ULunaMovement>(TEXT("InertialMovement"));Movement->SetUpdatedComponent(Collision);
    CameraArm=CreateDefaultSubobject<USpringArmComponent>(TEXT("FollowCameraArm"));CameraArm->SetupAttachment(Collision);CameraArm->bDoCollisionTest=false;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));Camera->SetupAttachment(CameraArm);
    AutoPossessPlayer=EAutoReceiveInput::Player0;
}
UPawnMovementComponent* ALunaPawn::GetMovementComponent()const{return Movement;}
void ALunaPawn::ResetView(bool Menu)
{
    bSurfaceView=Menu;ViewTransition=-1;VisualScale=Menu?12000.f:1.f;
    if(VisualRoot)VisualRoot->SetRelativeScale3D(FVector(VisualScale));
    // The same surface, rocks and anchored engines remain visible throughout the departure.
    LastPlumeCommand=FVector2D(100,100);UpdatePlumes();
}
void ALunaPawn::BeginDeparture(){bSurfaceView=false;ViewTransition=0;VisualScale=12000;LastPlumeCommand=FVector2D(100,100);UpdatePlumes();}
void ALunaPawn::BeginPlay(){Super::BeginPlay();BuildVisual();ResetView(true);}
void ALunaPawn::BuildVisual()
{
    auto* Mesh=ReplacementMesh?ReplacementMesh.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Game/LunaArt/Meshes/SM_MoonUnified.SM_MoonUnified"));
    Surface->SetStaticMesh(Mesh);Surface->SetRelativeScale3D(FVector(.28));Surface->SetRelativeRotation(FRotator(0,0,90));
    auto* Jet=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/LunaArt/Meshes/SM_EngineMonument.SM_EngineMonument"));HeroThruster->SetStaticMesh(Jet);
    LocalEngines->SetStaticMesh(Jet);
    FarEngines->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/LunaArt/Meshes/SM_EngineMonument_Far.SM_EngineMonument_Far")));
    NearTerrain->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/LunaArt/Meshes/SM_SurfaceRocks.SM_SurfaceRocks")));NearTerrain->SetRelativeRotation(FRotator(0,0,90));NearTerrain->SetRelativeScale3D(FVector(28./173740.));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/LunaArt/Meshes/SM_PlasmaColumn.SM_PlasmaColumn"));
    auto* Plasma=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_Plasma.M_Plasma"));
    for(auto* C:{GlobalPlumes.Get(),PolarPlumes.Get()}){C->SetStaticMesh(Cylinder);if(Plasma)C->SetMaterial(0,Plasma);C->SetCastShadow(false);}
    PolarPlumes->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_PolarPlasma.M_PolarPlasma")));
    PolarCores->SetStaticMesh(Cylinder);PolarCores->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_PolarCore.M_PolarCore")));PolarCores->SetCastShadow(false);
    FString Raw;
    if(FFileHelper::LoadFileToString(Raw,*(FPaths::ProjectContentDir()/TEXT("LunaArt/Data/thrusters.json"))))
    {
        TSharedPtr<FJsonObject> Doc;
        if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Doc))
        {
            auto Vec=[](const TSharedPtr<FJsonObject>& O,const TCHAR* Key){const auto& A=O->GetArrayField(Key);return FVector(A[0]->AsNumber(),-A[1]->AsNumber(),A[2]->AsNumber());};
            const double S=28./1737400.;
            auto Hero=Doc->GetObjectField(TEXT("hero"));HeroBase=Vec(Hero,TEXT("position_m"))*S;HeroNormal=Vec(Hero,TEXT("normal")).GetSafeNormal();
            auto Cine=Doc->GetObjectField(TEXT("cinematic"));HeroBase=Vec(Cine,TEXT("base_m"))*S;SurfaceEye=Vec(Cine,TEXT("eye_m"))*S;SurfaceLook=Vec(Cine,TEXT("look_m"))*S;
            NearTerrain->SetRelativeLocation(HeroBase);
            HeroThruster->SetRelativeLocation(HeroBase);HeroThruster->SetRelativeRotation(FRotationMatrix::MakeFromZ(HeroNormal).ToQuat()*FRotator(0,0,90).Quaternion());
            HeroThruster->SetRelativeScale3D(FVector(S*10.));
            for(auto Value:Doc->GetArrayField(TEXT("instances")))
            {
                auto O=Value->AsObject();FVector P=Vec(O,TEXT("position_m"))*S,N=Vec(O,TEXT("normal")).GetSafeNormal();
                if(O->GetStringField(TEXT("kind"))==TEXT("polar_main"))
                {
                    auto* Body=N.Z>0?NorthJet.Get():SouthJet.Get();
                    Body->SetStaticMesh(Jet);
                    Body->SetRelativeLocation(P);Body->SetRelativeRotation(FRotationMatrix::MakeFromZ(N).ToQuat()*FRotator(0,0,90).Quaternion());Body->SetRelativeScale3D(FVector(S*10.));
                    const int32 Index=N.Z>0?0:1;PolarBases[Index]=P;PolarNormals[Index]=N;
                }
                else
                {
                    JetBases.Add(P);JetNormals.Add(N);
                    if(FVector::DotProduct(N,HeroNormal)<.9999999)
                    {
                        auto* Group=FVector::Dist(P,HeroBase)<4.?LocalEngines.Get():FarEngines.Get();
                        Group->AddInstance(FTransform(FRotationMatrix::MakeFromZ(N).ToQuat()*FRotator(0,0,90).Quaternion(),P,FVector(S*10.*O->GetNumberField(TEXT("height_m"))/8000.)));
                    }
                }
            }
        }
    }
    UpdatePlumes();
}
void ALunaPawn::UpdatePlumes()
{
    const auto& Settings=*GetDefault<ULunaSettings>();
    const float Az=LunaPropulsion::VerticalAcceleration(GetActorLocation().Z,Movement->Velocity.Z,Movement->TargetLayer,Settings);
    const FVector2D PolarPower=LunaPropulsion::PolarPower(Az,Settings.MaxVerticalAcceleration,Movement->bLocked||IsDeparting()||bSurfaceView);
    NorthThrust=PolarPower.X;SouthThrust=PolarPower.Y;
    const double S=28./1737400.;
    PolarPlumes->ClearInstances();PolarCores->ClearInstances();
    for(int32 I=0;I<2;++I)
    {
        const float P=I==0?NorthThrust:SouthThrust;
        if(P<.003f)continue;
        const FVector N=PolarNormals[I],Base=PolarBases[I]+N*(8000*S);
        const float Length=(80000+5420000*P)*S;
        const float Width=(6000+450000*FMath::Sqrt(P))*S;
        const FQuat Q=FRotationMatrix::MakeFromZ(N).ToQuat();
        PolarPlumes->AddInstance(FTransform(Q,Base,FVector(Width/100,Width/100,Length/100)));
        PolarCores->AddInstance(FTransform(Q,Base,FVector(Width/380,Width/380,Length/150)));
    }
    if(LastPlumeCommand.Equals(Movement->Command,.015))return;
    LastPlumeCommand=Movement->Command;
    GlobalPlumes->ClearInstances();
    for(int32 I=0;I<JetBases.Num();++I)
    {
        FVector P=JetBases[I],N=JetNormals[I];
        // Idle: 6 km; active back-facing engines: up to 120 km. Towers keep supplied dimensions.
        const float Power=.005f+.195f*FMath::Max(0.f,-float(FVector::DotProduct(N,FVector(Movement->Command.X,Movement->Command.Y,0))));
        const float Length=600000*S*Power;
        const bool Hero=FVector::DotProduct(N,HeroNormal)>.9999999;
        GlobalPlumes->AddInstance(FTransform(FRotationMatrix::MakeFromZ(N).ToQuat(),(Hero?HeroBase:P)+N*((Hero?8000:1200)*S),FVector(Hero?.0008:.00015,Hero?.0008:.00015,(Hero?Length*8:Length)/100.)));
    }
}
void ALunaPawn::CalcCamera(float Dt,FMinimalViewInfo& Out)
{
    const FVector Center=GetActorLocation();
    const FVector Tangent=FVector::CrossProduct(FVector::UpVector,HeroNormal).GetSafeNormal();
    const FVector OrbitEye(-680,0,480);
    const FVector OrbitTarget(110,0,0);
    FVector Eye,Target;FVector Up=FVector::UpVector;
    if(bSurfaceView||IsDeparting())
    {
        const float T=bSurfaceView?0.f:DepartureProgress();
        const float Ease=T*T*(3-2*T);
        const double Scale=FMath::Pow(12000.,1.-double(Ease));
        // Surface and camera shrink together. Angular image remains continuous while clearance grows.
        const double Clearance=210.*FMath::Pow(double(T),8.);
        double Mix=FMath::Clamp((double(T)-.45)/.55,0.,1.);Mix=Mix*Mix*(3-2*Mix);
        const FVector LiftEye=SurfaceEye+HeroNormal*Clearance;
        Eye=FMath::Lerp(LiftEye,OrbitEye,Mix)*Scale;
        Target=FMath::Lerp(SurfaceLook,OrbitTarget,Mix)*Scale;
        Up=FMath::Lerp(HeroNormal,FVector::UpVector,Mix).GetSafeNormal();
        VisualRoot->SetRelativeScale3D(FVector(Scale));
    }
    else{Eye=OrbitEye;Target=OrbitTarget;VisualRoot->SetRelativeScale3D(FVector::OneVector);}
    Surface->SetCustomPrimitiveDataVector3(0,VisualRoot->GetComponentTransform().TransformPosition(HeroBase));
    Surface->SetCustomPrimitiveDataFloat(4,42000.f*28.f/1737400.f*VisualRoot->GetComponentScale().X);
    Out.Location=Center+Eye;
    Out.Rotation=FRotationMatrix::MakeFromXZ((Target-Eye).GetSafeNormal(),Up).Rotator();
    Out.FOV=bSurfaceView?58.f:IsDeparting()?FMath::Lerp(58.f,60.f,DepartureProgress()):60.f;Out.AspectRatio=16.f/9.f;Out.bConstrainAspectRatio=false;
}
bool ALunaPawn::HasContinuousSurface()const{return Surface&&Surface->IsVisible()&&Surface->GetStaticMesh()&&Surface->GetStaticMesh()->GetName()==TEXT("SM_MoonUnified")&&NearTerrain->IsVisible();}
int32 ALunaPawn::EngineCount()const{return LocalEngines->GetInstanceCount()+FarEngines->GetInstanceCount()+3;}
void ALunaPawn::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Game=Cast<ALunaGame>(GetWorld()->GetAuthGameMode());
    if(IsDeparting()&&(!Game||Game->Phase!=ELunaPhase::Paused))
    {
        ViewTransition=FMath::Min(9.f,ViewTransition+Dt);
        if(ViewTransition>=9)ResetView(false);
    }
    // Polar feedback updates every frame; planar instances only rebuild when their input changes.
    UpdatePlumes();
}
