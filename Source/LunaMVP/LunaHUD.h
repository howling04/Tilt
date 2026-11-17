#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class ALunaGame;

class SLunaHUD:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLunaHUD){} SLATE_ARGUMENT(ALunaGame*,Game) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool)const override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&)override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&)override;
    virtual FVector2D ComputeDesiredSize(float)const override {return {1280,720};}
private:
    TWeakObjectPtr<ALunaGame> Game;
    struct FAction{FSlateRect Rect;int32 Id;};
    mutable TArray<FAction> Actions;
    FVector2D Pointer=FVector2D(-100,-100);
};
