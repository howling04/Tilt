#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "LunaPawn.generated.h"
class USphereComponent;
class UCameraComponent;
class USpringArmComponent;
class UProceduralMeshComponent;

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class LUNAMVP_API ULunaMovement : public UPawnMovementComponent
{
    GENERATED_BODY()
public:
    ULunaMovement();
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* ThisTick) override;
    void ResetMotion();
    FVector2D Command=FVector2D::ZeroVector;
    int32 TargetLayer=0;
    bool bLocked=true;
    float StableTime=0,BlockedTime=0,HitFeedback=0;
    int32 CollisionEvents=0;
    bool Stable() const {return StableTime>=.3f;}
private:
    double Accumulator=0;
};

UCLASS(Blueprintable)
class LUNAMVP_API ALunaPawn : public APawn
{
    GENERATED_BODY()
public:
    ALunaPawn();
    virtual UPawnMovementComponent* GetMovementComponent() const override;
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult) override;
    void ResetView(bool Menu=false);
    void BeginDeparture();
    bool IsDeparting()const{return ViewTransition>=0&&ViewTransition<9.f;}
    float DepartureProgress()const{return FMath::Clamp(ViewTransition/9.f,0.f,1.f);}
    bool HasContinuousSurface()const;
    int32 EngineCount()const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ULunaMovement> Movement;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Replaceable Appearance") TObjectPtr<UStaticMesh> ReplacementMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Propulsion") float NorthThrust=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Propulsion") float SouthThrust=0;
private:
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> LowPolyMoon;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> NorthJet;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SouthJet;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Surface;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeroThruster;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> NearTerrain;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> GlobalPlumes;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> LocalEngines;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> FarEngines;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> PolarPlumes;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> PolarCores;
    FVector HeroBase=FVector(-22,0,17),HeroNormal=FVector(-.78,0,.625);
    FVector SurfaceEye,SurfaceLook;
    bool bSurfaceView=true;
    float ViewTransition=-1,VisualScale=12000;
    TArray<FVector> JetBases,JetNormals;
    FVector PolarBases[2],PolarNormals[2];
    FVector2D LastPlumeCommand=FVector2D(100,100);
    void UpdatePlumes();
    void BuildVisual();
};
