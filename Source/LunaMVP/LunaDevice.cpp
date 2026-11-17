#include "LunaDevice.h"
#include "LunaSettings.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Common/UdpSocketBuilder.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool Number(const TSharedPtr<FJsonObject>& J,const TCHAR* Key,double& V)
{ return J->TryGetNumberField(Key,V) && FMath::IsFinite(V); }
bool Counter(const TSharedPtr<FJsonObject>& J,const TCHAR* Key,int64& V)
{
    double N;
    if(!Number(J,Key,N)||N<0||N>9007199254740991.||FMath::FloorToDouble(N)!=N) return false;
    V=static_cast<int64>(N); return true;
}
}
FLunaDevice::~FLunaDevice(){ Close(); }
bool FLunaDevice::Open(const ULunaSettings& S)
{
    Close(); Stale=FMath::Max(.05f,S.StaleSeconds); ControlAddress=S.BridgeAddress; ControlPort=S.ControlPort;
    FIPv4Address Address;
    if(!FIPv4Address::Parse(S.ListenAddress,Address)||S.InputPort<1||S.InputPort>65535){Status=TEXT("Invalid UDP endpoint");return false;}
    Socket=FUdpSocketBuilder(TEXT("LunaMVP_Input")).AsNonBlocking().BoundToEndpoint(FIPv4Endpoint(Address,S.InputPort)).WithReceiveBufferSize(65536);
    bBound=Socket!=nullptr; Status=bBound?TEXT("Waiting for controller"):TEXT("UDP port unavailable");
    return bBound;
}
void FLunaDevice::Close()
{
    if(Socket){Socket->Close();ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);Socket=nullptr;}
    bBound=false; bValid=false; bCalibrated=false; bCentered=false; bAcked=false;
    SourceSeq=-1; GestureSeq=-1; LastSourceTime=-1e9; SourceAge=1e9; PendingRequest.Reset(); BridgeId.Reset(); BootId.Reset(); ClearEvents();
}
void FLunaDevice::Poll(double Now)
{
    if(Socket)
    {
        uint32 Size=0;
        for(int32 I=0;I<64&&Socket->HasPendingData(Size);++I)
        {
            TArray<uint8> Bytes; Bytes.SetNumUninitialized(FMath::Min<uint32>(Size,65507)+1);
            int32 Read=0;
            if(!Socket->Recv(Bytes.GetData(),Bytes.Num()-1,Read)) break;
            if(Read<=0||Read>4096){++Rejected;continue;}
            Bytes[Read]=0;
            Consume(FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData()))),Now);
        }
    }
    if(!PendingRequest.IsEmpty()&&Now-RequestTime>2.){PendingRequest.Reset();bAcked=false;Status=TEXT("Centering timed out. Try again");}
    if(bCentered&&!Healthy(Now)){bCentered=false;bAcked=false;ClearEvents();Status=TEXT("Controller lost. Flight on hold");}
}
double FLunaDevice::Age(double Now) const { return SourceAge+FMath::Max(0.,Now-LastSourceTime); }
bool FLunaDevice::Healthy(double Now) const {return bValid&&bCalibrated&&Age(Now)<=Stale;}
bool FLunaDevice::CenterReady(double Now) const {return bAcked&&Healthy(Now)&&SourceSeq>AckSource;}
void FLunaDevice::ClearEvents(){ PendingGesture=0; }
void FLunaDevice::InvalidateCenter(){bCentered=false;bAcked=false;PendingRequest.Reset();ClearEvents();}
int32 FLunaDevice::ConsumeGesture(){int32 V=PendingGesture;PendingGesture=0;return V;}
bool FLunaDevice::Consume(const FString& Text,double Now)
{
    TSharedPtr<FJsonObject> J; auto Reader=TJsonReaderFactory<>::Create(Text);
    auto Fail=[this](){++Rejected;return false;};
    if(Text.Len()>4096||!FJsonSerializer::Deserialize(Reader,J)||!J.IsValid()) return Fail();
    double Schema; FString Type;
    if(!Number(J,TEXT("schema"),Schema)||Schema!=1||!J->TryGetStringField(TEXT("type"),Type)) return Fail();
    if(Type==TEXT("recenter_ack"))
    {
        FString Request,Code,Bridge,Boot; bool Ok=false; int64 Seq;
        if(!J->TryGetStringField(TEXT("request_id"),Request)||Request!=PendingRequest||Request.IsEmpty()||
           !J->TryGetStringField(TEXT("bridge_id"),Bridge)||Bridge!=BridgeId||!J->TryGetStringField(TEXT("boot_id"),Boot)||Boot!=BootId||
           !J->TryGetBoolField(TEXT("ok"),Ok)||!J->TryGetStringField(TEXT("code"),Code)||!Counter(J,TEXT("source_seq"),Seq))return Fail();
        if(!Ok||Code!=TEXT("CENTERED")){Status=TEXT("Centering incomplete. Hold still and retry");PendingRequest.Reset();return Fail();}
        bAcked=true; AckSource=Seq; Status=TEXT("Waiting for centered input"); return true;
    }
    if(Type!=TEXT("input"))return Fail();
    FString Bridge,Boot,Gesture; double P,R,AgeMs,BoundMs=0; int64 Seq,GSeq; bool Valid,Calibrated;
    if(!J->TryGetStringField(TEXT("bridge_id"),Bridge)||Bridge.IsEmpty()||Bridge.Len()>128||
       !J->TryGetStringField(TEXT("boot_id"),Boot)||Boot.IsEmpty()||Boot.Len()>128||
       !Counter(J,TEXT("source_seq"),Seq)||!Counter(J,TEXT("gesture_seq"),GSeq)||
       !Number(J,TEXT("source_age_ms"),AgeMs)||AgeMs<0||AgeMs>10000||
       !Number(J,TEXT("pitch_deg"),P)||FMath::Abs(P)>45||!Number(J,TEXT("roll_deg"),R)||FMath::Abs(R)>45||
       !J->TryGetBoolField(TEXT("valid"),Valid)||!J->TryGetBoolField(TEXT("calibrated"),Calibrated)||
       !J->TryGetStringField(TEXT("gesture"),Gesture)||(Gesture!=TEXT("NONE")&&Gesture!=TEXT("UP")&&Gesture!=TEXT("DOWN")))return Fail();
    if(J->HasField(TEXT("source_bound_ms"))&&(!Number(J,TEXT("source_bound_ms"),BoundMs)||BoundMs<0||BoundMs>10000))return Fail();
    if(Bridge!=BridgeId||Boot!=BootId)
    {
        BridgeId=Bridge; BootId=Boot; SourceSeq=-1; GestureSeq=-1;
        bCentered=false;bAcked=false;PendingRequest.Reset();ClearEvents();Status=TEXT("Controller reconnected. Recenter required");
    }
    if(Seq<=SourceSeq){++Duplicate;return false;}
    const bool bNewGesture=GSeq>GestureSeq&&GestureSeq>=0;
    SourceSeq=Seq; LastSourceTime=Now; SourceAge=(AgeMs+BoundMs)/1000.;
    bValid=Valid;bCalibrated=Calibrated;Pitch=P;Roll=R;++Accepted;
    if(bNewGesture&&bCentered&&Healthy(Now))PendingGesture=Gesture==TEXT("UP")?1:Gesture==TEXT("DOWN")?-1:0;
    GestureSeq=FMath::Max(GestureSeq,GSeq);
    if(!bCentered&&CenterReady(Now)){bCentered=true;PendingRequest.Reset();ClearEvents();Status=TEXT("Controller centered");}
    return true;
}
void FLunaDevice::RequestCenter(double Now)
{
    bCentered=false;bAcked=false;ClearEvents();
    if(BridgeId.IsEmpty()||BootId.IsEmpty()){Status=TEXT("Connect controller first");return;}
    PendingRequest=FGuid::NewGuid().ToString(EGuidFormats::Digits);RequestTime=Now;
    auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("schema"),1);J->SetStringField(TEXT("type"),TEXT("recenter"));
    J->SetStringField(TEXT("request_id"),PendingRequest);J->SetStringField(TEXT("bridge_id"),BridgeId);J->SetStringField(TEXT("boot_id"),BootId);
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text); FJsonSerializer::Serialize(J,Writer);
    if(Socket)
    {
        auto Dest=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();bool Valid=false;Dest->SetIp(*ControlAddress,Valid);Dest->SetPort(ControlPort);
        FTCHARToUTF8 Bytes(*Text);int32 Sent=0;
        if(!Valid||!Socket->SendTo(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length(),Sent,*Dest)||Sent!=Bytes.Length())
        {PendingRequest.Reset();Status=TEXT("Center request failed");return;}
    }
    Status=TEXT("Hold still. Centering");
}
