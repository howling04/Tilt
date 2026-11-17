#include "LunaHUD.h"
#include "LunaGame.h"
#include "LunaPawn.h"
#include "LunaScale.h"
#include "LunaSettings.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "InputCoreTypes.h"
// UI craft workflow: unslop by Damir Sokolovsky, CC BY 4.0, https://heydamir.design/unslop.

namespace
{
FLinearColor C(const TCHAR* S){return FLinearColor::FromSRGBColor(FColor::FromHex(S));}
const FLinearColor White=C(TEXT("EDF3F6")),Blue=C(TEXT("AFD8EA")),Muted=C(TEXT("95A3AD")),Warning=C(TEXT("F3CBA5"));
FLinearColor Alpha(FLinearColor Color,float A){Color.A=A;return Color;}
const FLinearColor Rule=Alpha(White,.25f);
struct FDraw
{
    const FGeometry& G;FSlateWindowElementList& O;int32 L;float S;FVector2f Offset;
    FDraw(const FGeometry& Geo,FSlateWindowElementList& Out,int32 Layer):G(Geo),O(Out),L(Layer)
    {S=FMath::Min(G.GetLocalSize().X/1280.,G.GetLocalSize().Y/720.);Offset=FVector2f((G.GetLocalSize()-FVector2D(1280*S,720*S))*.5);}
    FPaintGeometry PG(float X,float Y,float W=1,float H=1)const{return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(Offset+FVector2f(X*S,Y*S)));}
    void Rect(float X,float Y,float W,float H,FLinearColor Color){FSlateDrawElement::MakeBox(O,L++,PG(X,Y,W,H),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,Color);}
    void Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Width=1)
    {TArray<FVector2D> P={{X*S+Offset.X,Y*S+Offset.Y},{X2*S+Offset.X,Y2*S+Offset.Y}};FSlateDrawElement::MakeLines(O,L++,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Width*S);}
    FSlateFontInfo Font(int32 Size,bool Title=false)const
    {
        const FString Path=FPaths::ProjectContentDir()/(Title?TEXT("LunaArt/Fonts/Rajdhani-Medium.ttf"):TEXT("LunaArt/Fonts/NotoSansCJKsc-Regular.otf"));
        static TMap<FString,TSharedPtr<const FCompositeFont>> Fonts;
        if(!Fonts.Contains(Path))Fonts.Add(Path,MakeShared<FCompositeFont>(TEXT("Regular"),Path,EFontHinting::Default,EFontLoadingPolicy::LazyLoad));
        return FSlateFontInfo(Fonts[Path],FMath::Max(6.f,Size*S*.75f),TEXT("Regular"));
    }
    void Text(const FString& T,float X,float Y,int32 Size,FLinearColor Color,bool Title=false)
    {FSlateDrawElement::MakeText(O,L++,PG(X,Y),T,Font(Size,Title),ESlateDrawEffect::None,Color);}
    float Width(const FString& T,int32 Size,bool Title=false)const
    {return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T,Font(Size,Title)).X/S;}
    void Center(const FString& T,float X,float Y,int32 Size,FLinearColor Color,bool Title=false)
    {Text(T,X-Width(T,Size,Title)*.5,Y,Size,Color,Title);}
    void Diamond(float X,float Y,float R,FLinearColor Color)
    {Line(X-R,Y,X,Y-R,Color);Line(X,Y-R,X+R,Y,Color);Line(X+R,Y,X,Y+R,Color);Line(X,Y+R,X-R,Y,Color);}
};
}
void SLunaHUD::Construct(const FArguments& Args){Game=Args._Game;SetVisibility(EVisibility::Visible);ForceVolatile(true);}
FReply SLunaHUD::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const float S=FMath::Min(G.GetLocalSize().X/1280.,G.GetLocalSize().Y/720.);const auto Off=(G.GetLocalSize()-FVector2D(1280*S,720*S))*.5;
    Pointer=(G.AbsoluteToLocal(E.GetScreenSpacePosition())-Off)/S;return FReply::Unhandled();
}
FReply SLunaHUD::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    OnMouseMove(G,E);if(E.GetEffectingButton()!=EKeys::LeftMouseButton||!Game.IsValid())return FReply::Unhandled();
    for(const auto& A:Actions)if(A.Rect.ContainsPoint(Pointer))
    {
        switch(A.Id){case 1:Game->StartRun(false);break;case 2:Game->StartRun(true);break;case 3:Game->RequestCenter();break;case 4:Game->TogglePause();break;case 5:Game->ShowMenu();break;case 6:Game->StartRun(false,0,true);break;case 7:Game->StartRun(false,1,true);break;case 8:Game->StartRun(false,2,true);break;}
        return FReply::Handled();
    }return FReply::Unhandled();
}
int32 SLunaHUD::OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const
{
    auto* G=Game.Get();if(!G||!G->Moon)return Layer;
    auto* Moon=G->Moon.Get();auto* Motion=Moon->Movement.Get();FDraw D(Geometry,Out,Layer);Actions.Reset();
    auto Button=[&](float X,float Y,float W,const FString& Label,int32 Id,bool Primary=false)
    {
        const FSlateRect R(X,Y,X+W,Y+40);const bool Hover=R.ContainsPoint(Pointer);
        if(Primary)D.Rect(X,Y,W,40,Alpha(White,Hover?1.f:.91f));
        else{if(Hover)D.Rect(X,Y,W,40,Alpha(White,.07f));D.Line(X,Y+40,X+W,Y+40,Hover?White:Rule);}
        D.Text(Label,X+16,Y+10,15,Primary?C(TEXT("162027")):Hover?White:Muted);Actions.Add({R,Id});
    };
    const bool Menu=G->Phase==ELunaPhase::Menu,Complete=G->Phase==ELunaPhase::Complete;
    auto Caption=[&]()
    {
        if(G->SubtitleRemaining<=0||G->Subtitle.IsEmpty())return;
        const float A=FMath::Min(1.f,G->SubtitleRemaining/.4f),W=D.Width(G->Subtitle,15)+40;
        D.Rect(640-W*.5,548,W,36,FLinearColor(.005f,.007f,.009f,.7f*A));
        D.Center(G->Subtitle,640,556,15,Alpha(White,A));
    };
    if(Menu)
    {
        D.Center(TEXT("The Wandering Luna"),640,60,58,White,true);
        D.Center(TEXT("Lunar flight command"),640,128,12,Muted);
        Button(56,360,244,TEXT("Begin flight     /     Enter"),1,true);
        Button(56,412,200,TEXT("Controller     /     I"),2);
        D.Text(TEXT("Training"),56,484,11,Muted);
        Button(40,508,140,TEXT("1   Brake"),6);Button(188,508,140,TEXT("2   Lateral"),7);Button(336,508,140,TEXT("3   Polar"),8);
        return D.L;
    }
    if(Moon->IsDeparting()&&G->Phase!=ELunaPhase::Paused)
    {
        D.Text(TEXT("LUNA"),48,36,18,White,true);
        D.Center(TEXT("Orbital transfer"),640,48,17,White);
        D.Line(568,80,712,80,Rule);D.Line(568,80,568+144*Moon->DepartureProgress(),80,Blue);
        Caption();return D.L;
    }
    D.Text(TEXT("LUNA"),48,32,22,White,true);
    D.Line(104,37,104,57,Rule);
    D.Text(G->StageName(),120,37,14,White);
    for(int32 I=0;I<3;++I)
    {
        const float X=608+I*32;D.Text(FString::Printf(TEXT("%02d"),I+1),X,38,15,I==G->Stage?White:Alpha(Muted,.5f),true);
        if(I==G->Stage)D.Line(X,64,X+16,64,Blue);
    }
    if(G->bSafetyLocked){D.Rect(1072,39,4,4,Warning);D.Text(TEXT("Control hold"),1088,32,12,Warning);}
    Button(1184,24,48,TEXT("II"),4);
    const FVector2D Applied=Motion->bLocked?FVector2D::ZeroVector:Motion->Command.GetClampedToMaxSize(1.);
    const float Thrust=FMath::Max(float(Applied.Size()),FMath::Max(Moon->NorthThrust,Moon->SouthThrust));
    const double Speed=LunaScale::SpeedKmS(Motion->Velocity.Size());
    D.Text(FString::Printf(TEXT("%.2f"),Speed),48,592,48,White,true);
    D.Text(TEXT("km/s"),48+D.Width(FString::Printf(TEXT("%.2f"),Speed),48,true)+12,624,13,Muted,true);
    D.Line(48,656,208,656,Rule);D.Line(48,656,48+160*Thrust,656,Blue,2);
    D.Text(FString::Printf(TEXT("Thrust  %.0f%%"),100*Thrust),48,668,12,Muted);
    D.Text(FString::Printf(TEXT("%.1f h"),G->RunTime),1120,652,22,White,true);
    D.Text(TEXT("Mission time"),1120,680,11,Muted);
    if(G->Stage==2)
    {
        constexpr float X=1208,Y=260,H=176;
        D.Line(X,Y,X,Y+H,Rule);
        for(int32 I=-1;I<=1;++I)
        {
            const float YY=Y+H*(1-I)*.5;const bool Current=Motion->TargetLayer==I;
            D.Line(X-(Current?14:7),YY,X+7,YY,Current?Blue:Rule);
            D.Text(FString::Printf(TEXT("%+d"),I),X+14,YY-9,14,Current?White:Muted,true);
        }
        const float Actual=FMath::Clamp(float((Moon->GetActorLocation().Z-100)/400.),0.f,1.f);
        D.Diamond(X,Y+H*(1-Actual),4,White);
        const FString Alt=FString::Printf(TEXT("%+.0f km"),LunaScale::DistanceKm(Moon->GetActorLocation().Z-300));
        D.Text(Alt,1240-D.Width(Alt,15,true),468,15,White,true);
        D.Text(Motion->Stable()?TEXT("Orbit hold"):TEXT("Orbit shift"),1140,496,11,Muted);
        const float Polar=FMath::Max(Moon->NorthThrust,Moon->SouthThrust);
        if(Polar>.02f)D.Text(TEXT("Polar thrust"),1140,520,11,Blue);
    }
    if(auto* PC=G->GetWorld()->GetFirstPlayerController())
    {
        auto Project=[&](FVector P)
        {
            FVector2D Screen;int32 W=1,H=1;PC->GetViewportSize(W,H);
            if(!PC->ProjectWorldLocationToScreen(P,Screen))return FVector2D(640,144);
            const FVector2D Local(Screen.X/FMath::Max(1,W)*Geometry.GetLocalSize().X,Screen.Y/FMath::Max(1,H)*Geometry.GetLocalSize().Y);
            return (Local-FVector2D(D.Offset))/D.S;
        };
        const FVector2D Raw=Project(G->NavigationTarget());
        const FVector2D Way(FMath::Clamp(Raw.X,288.,1040.),FMath::Clamp(Raw.Y,144.,508.));
        D.Diamond(Way.X,Way.Y,7,Blue);
        if(!Way.Equals(Raw,1.))
        {const auto V=(Raw-Way).GetSafeNormal(),Side=FVector2D(-V.Y,V.X);const auto Tip=Way+V*18;D.Line(Tip.X,Tip.Y,Tip.X-V.X*5+Side.X*4,Tip.Y-V.Y*5+Side.Y*4,Blue);D.Line(Tip.X,Tip.Y,Tip.X-V.X*5-Side.X*4,Tip.Y-V.Y*5-Side.Y*4,Blue);}
        const double Km=LunaScale::DistanceKm(FVector::Dist(Moon->GetActorLocation(),G->NavigationTarget()));
        D.Center(FString::Printf(TEXT("%.0f km"),Km),Way.X,Way.Y+16,13,White,true);
        auto Origin=Project(Moon->GetActorLocation());
        D.Line(Origin.X-48,Origin.Y-16,Origin.X-48,Origin.Y-28,Alpha(White,.45f));D.Line(Origin.X-48,Origin.Y-28,Origin.X-36,Origin.Y-28,Alpha(White,.45f));
        D.Line(Origin.X+48,Origin.Y+16,Origin.X+48,Origin.Y+28,Alpha(White,.45f));D.Line(Origin.X+48,Origin.Y+28,Origin.X+36,Origin.Y+28,Alpha(White,.45f));
        if(Motion->Velocity.Size()>2)
        {
            auto End=Project(Moon->GetActorLocation()+Motion->Velocity.GetSafeNormal()*70);
            D.Line(Origin.X,Origin.Y,End.X,End.Y,Alpha(White,.3f));D.Diamond(End.X,End.Y,3,White);
        }
    }
    D.Line(48,80,208,80,Rule);D.Line(48,80,48+160*G->Progress(),80,Blue);
    Caption();
    if(G->Stage==0&&G->DockHold>0){D.Center(TEXT("Hold"),640,464,13,White);D.Line(608,488,608+64*FMath::Min(G->DockHold,1.f),488,Blue,2);}
    if(G->bDiagnostics)
    {
        D.Rect(48,104,400,108,FLinearColor(.008f,.01f,.012f,.9f));
        D.Text(G->Device.SourceSeq<0?FString::Printf(TEXT("UDP %s   No samples"),G->Device.bBound?TEXT("bound"):TEXT("off")):FString::Printf(TEXT("UDP %s   seq %lld   age %.0f ms"),G->Device.bBound?TEXT("bound"):TEXT("off"),G->Device.SourceSeq,G->Device.Age(FPlatformTime::Seconds())*1000),64,116,12,Muted,true);
        D.Text(G->Device.Status,64,140,12,Muted);
        D.Text(TEXT("WASD  Thrust    Q / E  Altitude    R  Reset"),64,168,12,White);
        D.Text(TEXT("F2  Controller    C  Center    V  Voice    M  Menu"),64,188,11,Muted);
    }
    if(G->Phase==ELunaPhase::Paused||G->Phase==ELunaPhase::DeviceSetup||Complete)
    {
        const bool Setup=G->Phase==ELunaPhase::DeviceSetup;
        D.Rect(0,0,1280,720,FLinearColor(.004f,.006f,.008f,.86f));
        D.Text(Complete?TEXT("Flight complete"):Setup?TEXT("Controller"):TEXT("Flight paused"),440,228,40,White,true);
        D.Line(440,292,840,292,Rule);
        D.Text(Complete?FString::Printf(TEXT("%.1f h   /   %d resets"),G->RunTime,G->Resets):Setup?G->Device.Status:TEXT("Release flight controls before resuming"),440,312,14,Muted);
        Button(440,376,300,Complete?TEXT("Fly again     /     Enter"):Setup?TEXT("Center     /     C"):TEXT("Resume     /     Esc"),Complete?1:Setup?3:4,true);
        Button(440,432,200,TEXT("Main menu     /     M"),5);
        D.Text(G->bMuted?TEXT("V   Voice off"):TEXT("V   Voice on"),440,520,12,Muted);
        if(!Setup&&!Complete)D.Text(TEXT("F1   Flight controls"),440,548,12,Muted);
    }
    return D.L;
}
