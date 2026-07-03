#include "Framework.h"

namespace SDK
{

uintptr_t InSDKUtils::GetImageBase()
{
    return *(uintptr_t*)(__readgsqword(0x60) + 0x10);
}

static uint64 GetCastFlags(EClassCastFlags Flags)
{
    return static_cast<uint64>(Flags);
}

class UObject* UObject::FindObjectImpl(const char* FullName, EClassCastFlags RequiredType)
{
    for (int32 i = 0; i < GObjects->Num(); ++i)
    {
        class UObject* Obj = GObjects->GetByIndex(i);
        if (!Obj) continue;
        if (Obj->GetFullName().find(FullName) != UEAllocatedString::npos)
        {
            if (RequiredType == EClassCastFlags::None || Obj->HasTypeFlag(RequiredType))
                return Obj;
        }
    }
    return nullptr;
}

class UObject* UObject::FindObjectFastImpl(const char* Name, EClassCastFlags RequiredType)
{
    for (int32 i = 0; i < GObjects->Num(); ++i)
    {
        class UObject* Obj = GObjects->GetByIndex(i);
        if (!Obj) continue;
        if (Obj->GetName() == Name)
        {
            if (RequiredType == EClassCastFlags::None || Obj->HasTypeFlag(RequiredType))
                return Obj;
        }
    }
    return nullptr;
}

UEAllocatedString UObject::GetFullName() const
{
    UEAllocatedString Result;
    if (Class)
    {
        Result += Class->GetName();
        Result += " ";
    }
    UEAllocatedString OuterName;
    if (Outer)
        OuterName = Outer->GetName();
    else
        OuterName = "None";
    Result += OuterName;
    Result += ".";
    Result += GetName();
    return Result;
}

UEAllocatedString UObject::GetName() const
{
    return Name.ToString();
}

bool UObject::HasTypeFlag(EClassCastFlags TypeFlags) const
{
    if (!Class) return false;
    return (Class->CastFlags & static_cast<uint64>(TypeFlags)) == static_cast<uint64>(TypeFlags);
}

bool UObject::IsA(EClassCastFlags TypeFlags) const
{
    return HasTypeFlag(TypeFlags);
}

bool UObject::IsA(class UClass* TypeClass) const
{
    for (auto* Super = Class; Super; Super = static_cast<UClass*>(Super->Super))
    {
        if (Super == TypeClass)
            return true;
    }
    return false;
}

bool UObject::IsDefaultObject() const
{
    return (static_cast<uint32_t>(Flags) & static_cast<uint32_t>(EObjectFlags::ClassDefaultObject)) != 0;
}

void UObject::ExecuteUbergraph(int32 EntryPoint)
{
    using FuncType = void (*)(const UObject*, int32, void*);
    InSDKUtils::CallGameFunction(InSDKUtils::GetVirtualFunction<FuncType>(this, Offsets::ProcessEventIdx), this, EntryPoint, nullptr);
}

bool UStruct::IsSubclassOf(const UStruct* Base) const
{
    for (auto* Super = this; Super; Super = static_cast<UStruct*>(Super->Super))
    {
        if (Super == Base)
            return true;
    }
    return false;
}

class UFunction* UClass::FindFunction(const char* FuncName) const
{
    for (auto* SuperClass = this; SuperClass; SuperClass = static_cast<UClass*>(SuperClass->Super))
    {
        for (auto* Field = SuperClass->Children; Field; Field = Field->Next)
        {
            if (Field->GetName() == FuncName)
                return static_cast<class UFunction*>(Field);
        }
    }
    return nullptr;
}

class UFunction* UClass::GetFunction(const char* ClassName, const char* FuncName) const
{
    for (auto* SuperClass = this; SuperClass; SuperClass = static_cast<UClass*>(SuperClass->Super))
    {
        if (SuperClass->GetName() != ClassName)
            continue;
        for (auto* Field = SuperClass->Children; Field; Field = Field->Next)
        {
            if (Field->GetName() == FuncName)
                return static_cast<class UFunction*>(Field);
        }
    }
    return nullptr;
}

FName UKismetStringLibrary::Conv_StringToName(const class FString& InString)
{
    static auto* KismetClass = UObject::FindClass("Class Engine.KismetStringLibrary");
    static auto* ConvFunc = KismetClass ? KismetClass->FindFunction("Conv_StringToName") : nullptr;

    if (!KismetClass || !ConvFunc) return FName(0, 0);
    if (!KismetClass->DefaultObject) return FName(0, 0);

    struct { FString InString; FName ReturnValue; } Params{};
    Params.InString = InString;

    KismetClass->DefaultObject->ProcessEvent(ConvFunc, &Params);
    return Params.ReturnValue;
}

}
