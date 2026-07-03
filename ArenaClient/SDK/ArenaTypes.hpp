#pragma once

namespace SDK
{

class FStructBaseChain
{
protected:
	FStructBaseChain()
		: StructBaseChainArray(nullptr)
		, NumStructBasesInChainMinusOne(-1)
	{
	}
	~FStructBaseChain()
	{
		delete[] StructBaseChainArray;
	}

	FStructBaseChain(const FStructBaseChain&) = delete;
	FStructBaseChain& operator=(const FStructBaseChain&) = delete;

	__forceinline bool IsChildOfUsingStructArray(const FStructBaseChain* Parent) const
	{
		int32 NumParentStructBasesInChainMinusOne = Parent->NumStructBasesInChainMinusOne;
		return NumParentStructBasesInChainMinusOne <= NumStructBasesInChainMinusOne && StructBaseChainArray[NumParentStructBasesInChainMinusOne] == Parent;
	}

private:
	FStructBaseChain** StructBaseChainArray;
	int32 NumStructBasesInChainMinusOne;

	friend class UStruct;
};

// Class CoreUObject.Object
class alignas(0x08) UObject
{
public:
	static inline class TUObjectArrayWrapper      GObjects;

	void**                                        VTable;
	EObjectFlags                                  Flags;
	int32                                         Index;
	class UClass*                                 Class;
	class FName                                   Name;
	class UObject*                                Outer;

public:
	static class UObject* FindObjectFastImpl(const char* Name, EClassCastFlags RequiredType = EClassCastFlags::None);
	static class UObject* FindObjectImpl(const char* FullName, EClassCastFlags RequiredType = EClassCastFlags::None);

	UEAllocatedString GetFullName() const;
	UEAllocatedString GetName() const;
	bool HasTypeFlag(EClassCastFlags TypeFlags) const;
	bool IsA(EClassCastFlags TypeFlags) const;
	bool IsA(class UClass* TypeClass) const;

	template <typename _Ut>
	bool IsA() const
	{
		return IsA(_Ut::StaticClass());
	}

	template <typename _Nt>
	_Nt* Cast(UClass* _Cl) const
	{
		return IsA(_Cl) ? (_Nt*)this : nullptr;
	}

	template <typename _Nt>
	_Nt* Cast() const
	{
		return Cast<_Nt>(_Nt::StaticClass());
	}
	bool IsDefaultObject() const;

	void ExecuteUbergraph(int32 EntryPoint);

public:
	static class UClass* FindClass(const char* ClassFullName)
	{
		return FindObject<class UClass>(ClassFullName, EClassCastFlags::Class);
	}
	static class UClass* FindClassFast(const char* ClassName)
	{
		return FindObjectFast<class UClass>(ClassName, EClassCastFlags::Class);
	}

	template<typename UEType = UObject>
	static UEType* FindObject(const char* Name, EClassCastFlags RequiredType = EClassCastFlags::None)
	{
		return static_cast<UEType*>(FindObjectImpl(Name, RequiredType));
	}
	template<typename UEType = UObject>
	static UEType* FindObjectFast(const char* Name, EClassCastFlags RequiredType = EClassCastFlags::None)
	{
		return static_cast<UEType*>(FindObjectFastImpl(Name, RequiredType));
	}

	void ProcessEvent(class UFunction* Function, void* Parms) const
	{
		InSDKUtils::CallGameFunction(InSDKUtils::GetVirtualFunction<void (*)(const UObject*, class UFunction*, void*)>(this, Offsets::ProcessEventIdx), this, Function, Parms);
	}

	void AddToRoot() const
	{
		auto Item = GObjects->GetItemByIndex(Index);
		if (Item)
			Item->Flags |= 1 << 30;
	}
};

// Class CoreUObject.Field
class UField : public UObject
{
public:
	class UField*                                 Next;
};

// Class CoreUObject.Struct
class UStruct : public UField, FStructBaseChain
{
public:
	class UStruct*                                Super;
	class UField*                                 Children;
	class FField*                                 ChildProperties;
	int32                                         Size;
	int32                                         MinAlignemnt;

public:
	bool IsSubclassOf(const UStruct* Base) const;
};

// Class CoreUObject.Class
class UClass : public UStruct
{
public:
	uint64                                        CastFlags;
	class UObject*                                DefaultObject;

public:
	class UFunction* FindFunction(const char* FuncName) const;
	class UFunction* GetFunction(const char* ClassName, const char* FuncName) const;
};

// Class CoreUObject.Function
class UFunction : public UStruct
{
public:
	using FNativeFuncPtr = void (*)(void* Context, void* TheStack, void* Result);

	uint32                                        FunctionFlags;
	FNativeFuncPtr                                ExecFunction;
};

// Enum FortniteGame.EUIExtensionSlot
enum class EUIExtensionSlot : uint8
{
	Primary                                  = 0,
	Secondary                                = 1,
	TopRightCorner                           = 2,
	GameInfoBox                              = 3,
	Quickbar                                 = 4,
	QuickbarUnderlay                         = 5,
	UpperCenter                              = 6,
	CrosshairRight                           = 7,
	UnderSquadInfo                           = 8,
	FullScreenMap                            = 9,
	BelowRespawnWidget                       = 10,
	BelowCompass                             = 11,
	UnderTeammateStatus                      = 12,
	ControllerBindingCallout                 = 13,
	AboveStormMessageSlot                    = 14,
	CustomMinigameCallouts                   = 15,
	UnderLocalPlayerInfo                     = 16,
	PlayerHealthbarOverlay                   = 17,
	VehicleActionsSpacer                     = 18,
	InventoryScreenReplacement               = 19,
	Reticle                                  = 20,
	KillfeedSlot                             = 21,
	PrioritizedContextualSlot                = 22,
	RightOfTeammateStatus                    = 23,
	TeammateStatusPortraitOverlay            = 24,
	TeammateStatusPortraitOverlay_IGM        = 25,
};

// ScriptStruct FortniteGame.FUIExtension
struct FUIExtension final
{
public:
	EUIExtensionSlot                              Slot;                                              // 0x00(0x01)
	uint8                                         Pad_1[0x7];                                        // 0x01(0x07)
	TSoftClassPtr<class UClass>                   WidgetClass;                                       // 0x08(0x28)
};

// Class FortniteGame.FortPlaylistAthena
class UFortPlaylistAthena : public UObject
{
public:
	// Only defining the members we need
	// UIExtensions is at offset 0x0838
	// We'll access it manually through a helper
};

struct FFortPlaylistAthena_Helper
{
	uint8 Pad_0x0838[0x0838];
	TArray<struct FUIExtension> UIExtensions;
};

// Class Engine.KismetStringLibrary
class UKismetStringLibrary
{
public:
	static FName Conv_StringToName(const class FString& InString);
};

}
