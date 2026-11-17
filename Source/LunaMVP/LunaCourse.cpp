#include "LunaCourse.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"

ALunaCourse::ALunaCourse(){SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SpaceRoot")));}
UProceduralMeshComponent* ALunaCourse::Geometry(FString Name,const TArray<FVector>& V,const TArray<int32>& I,UMaterialInterface* M)
{
    auto* C=NewObject<UProceduralMeshComponent>(this,*Name,RF_Transactional);C->SetupAttachment(RootComponent);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
    TArray<FVector> N;TArray<FVector2D> UV;TArray<FLinearColor> Colors;TArray<FProcMeshTangent> T;
    N.Init(FVector::UpVector,V.Num());UV.Init(FVector2D::ZeroVector,V.Num());
    C->CreateMeshSection_LinearColor(0,V,I,N,UV,Colors,T,false);if(M)C->SetMaterial(0,M);
    AddInstanceComponent(C);C->RegisterComponent();Generated.Add(C);return C;
}
UProceduralMeshComponent* ALunaCourse::Ring(FString Name,FVector Center,float Radius,float Width,UMaterialInterface* M)
{
    TArray<FVector> V;TArray<int32> T;
    for(int32 I=0;I<96;++I)
    {
        if(I%24>19)continue;
        float A=I*2*PI/96,B=(I+1)*2*PI/96;int32 K=V.Num();
        for(auto Pair:{FVector2D(A,Radius),FVector2D(B,Radius),FVector2D(B,Radius+Width),FVector2D(A,Radius+Width)})
            V.Add(Center+FVector(0,FMath::Cos(Pair.X)*Pair.Y,FMath::Sin(Pair.X)*Pair.Y));
        T.Append({K,K+1,K+2,K,K+2,K+3});
    }
    return Geometry(Name,V,T,M);
}
void ALunaCourse::Rock(FVector P,float R,FRandomStream& Random)
{
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FLinearColor> Col;TArray<FProcMeshTangent> Tangents;
    constexpr int32 Rows=8,Cols=12;
    TArray<FVector> Grid;
    for(int32 I=0;I<=Rows;++I)for(int32 J=0;J<=Cols;++J)
    {
        float A=PI*I/Rows,B=2*PI*(J%Cols)/Cols;
        const float Noise=1+.10*FMath::Sin(3*B+1.7*A)+.09*FMath::Cos(5*A-2*B);
        Grid.Add(FVector(FMath::Sin(A)*FMath::Cos(B),FMath::Sin(A)*FMath::Sin(B)*.8,FMath::Cos(A)*.9)*R*Noise);
    }
    const FQuat Q=FRotator(Random.FRandRange(0,180),Random.FRandRange(0,360),Random.FRandRange(0,180)).Quaternion();
    auto Tri=[&](int32 A,int32 B,int32 C){FVector X=Q.RotateVector(Grid[A]),Y=Q.RotateVector(Grid[B]),Z=Q.RotateVector(Grid[C]);auto Normal=FVector::CrossProduct(Y-X,Z-X).GetSafeNormal();if(FVector::DotProduct(Normal,X)<0){Swap(Y,Z);Normal=-Normal;}int32 K=V.Num();V.Append({X+P,Y+P,Z+P});N.Append({Normal,Normal,Normal});T.Append({K,K+1,K+2});UV.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(.5,1)});};
    for(int32 I=0;I<Rows;++I)for(int32 J=0;J<Cols;++J){int32 A=I*(Cols+1)+J,B=A+Cols+1;Tri(A,B,A+1);Tri(A+1,B,B+1);}
    auto* C=Geometry(FString::Printf(TEXT("Asteroid_%04d"),AsteroidCount++),V,T,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_Asteroid.M_Asteroid")));
    C->CreateMeshSection_LinearColor(0,V,T,N,UV,Col,Tangents,false);
    TArray<FVector> Hull;for(auto Point:Grid)Hull.Add(Q.RotateVector(Point)+P);
    C->bUseComplexAsSimpleCollision=false;C->AddCollisionConvexMesh(Hull);C->SetCollisionProfileName(TEXT("BlockAll"));
}
void ALunaCourse::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);for(auto C:Generated)if(IsValid(C))C->DestroyComponent();Generated.Reset();AsteroidCount=0;
    auto* H=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_Hologram.M_Hologram"));
    auto* Star=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LunaArt/Materials/M_Star.M_Star"));
    FRandomStream Random(FieldSeed);TArray<FVector> Stars;TArray<int32> ST;
    for(int32 I=0;I<2800;++I)
    {
        FVector D=Random.VRand();FVector P=FVector(4800,0,300)+D*5500000;
        FVector U=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),V=FVector::CrossProduct(D,U);
        float Size=Random.FRandRange(400,1400);if(I%31==0)Size=2400;
        int32 K=Stars.Num();Stars.Append({P-U*Size-V*Size,P+U*Size-V*Size,P+U*Size+V*Size,P-U*Size+V*Size});ST.Append({K,K+1,K+2,K,K+2,K+3});
    }
    Geometry(TEXT("DistantStarfield"),Stars,ST,Star);
    for(float X:{1750.f,1900.f,2050.f})Ring(FString::Printf(TEXT("DockProjection_%.0f"),X),{X,0,300},250,.7f,H);
    for(int32 I=0;I<3;++I)
    {
        const float X=4000+I*700;const float GapY=I==0?-250:I==1?250:-200;
        Ring(FString::Printf(TEXT("LateralAperture_%d"),I),{X,GapY,300},115,1,H);
        for(int32 J=0;J<int32(150*AsteroidDensity);++J)
        {
            FVector P(X+Random.FRandRange(-75,75),Random.FRandRange(-720,720),Random.FRandRange(-100,900));
            if(FMath::Abs(P.Y-GapY)<165)continue;Rock(P,Random.FRandRange(.8,5.4),Random);
        }
    }
    const float HX[]={7800,8400,9000},HZ[]={500,300,100};
    for(int32 I=0;I<3;++I)
    {
        Ring(FString::Printf(TEXT("PolarAperture_%d"),I),{HX[I],0,HZ[I]},68,1,H);
        for(int32 J=0;J<int32(190*AsteroidDensity);++J)
        {
            FVector P(HX[I]+Random.FRandRange(-90,90),Random.FRandRange(-650,650),Random.FRandRange(-150,950));
            if(FMath::Abs(P.Y)<215&&FMath::Abs(P.Z-HZ[I])<110)continue;Rock(P,Random.FRandRange(.8,5.4),Random);
        }
    }
    CollisionProbe={4000,0,300};Rock(CollisionProbe,5.4f,Random);
    auto Barrier=[&](FString Name,float X){auto* B=NewObject<UBoxComponent>(this,*Name,RF_Transactional);B->SetupAttachment(RootComponent);B->SetRelativeLocation({X,0,350});B->SetBoxExtent({10,500,500});B->SetCollisionProfileName(TEXT("BlockAll"));AddInstanceComponent(B);B->RegisterComponent();Generated.Add(B);return B;};
    StageDoorBlock=Barrier(TEXT("DepartureInterlock"),2460);LessonDoorBlock=Barrier(TEXT("PolarInterlock"),7540);
    StageDoor=Ring(TEXT("DepartureInterlockProjection"),{2460,0,300},440,2,H);
    LessonDoor=Ring(TEXT("PolarInterlockProjection"),{7540,0,300},440,2,H);
    Ring(TEXT("ArrivalProjection"),{9500,0,100},290,1,H);
}
void ALunaCourse::SetDoors(int32 Stage,bool bDone)
{
    // Compact center beacons leave the flight view open; the safety volumes retain their full size.
    if(StageDoor){StageDoor->SetRelativeScale3D(FVector(.32));StageDoor->SetRelativeLocation(FVector(2460,0,300)*.68);}
    if(LessonDoor){LessonDoor->SetRelativeScale3D(FVector(.32));LessonDoor->SetRelativeLocation(FVector(7540,0,300)*.68);}
    if(StageDoorBlock)StageDoorBlock->SetCollisionEnabled(Stage>0?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
    if(LessonDoorBlock)LessonDoorBlock->SetCollisionEnabled(Stage>2||bDone?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
    if(StageDoor)StageDoor->SetVisibility(Stage==0);if(LessonDoor)LessonDoor->SetVisibility(Stage<=2&&!bDone);
}
