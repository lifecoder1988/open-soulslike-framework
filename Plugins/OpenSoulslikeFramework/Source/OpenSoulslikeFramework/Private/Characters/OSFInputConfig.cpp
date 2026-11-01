// Copyright Open Soulslike Framework contributors. MIT License.

#include "Characters/OSFInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	void MapAxis2D(UInputMappingContext* Context, UInputAction* Action, FKey Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}
}

UOSFInputConfig* UOSFInputConfig::BuildDefault(UObject* Outer)
{
	UOSFInputConfig* Config = NewObject<UOSFInputConfig>(Outer, TEXT("OSFDefaultInput"));
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Config, TEXT("IMC_OSFDefault"));
	Config->MappingContext = Context;

	Config->Move = MakeAction(Config, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Config->Look = MakeAction(Config, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Config->DodgeSprint = MakeAction(Config, TEXT("IA_DodgeSprint"), EInputActionValueType::Boolean);
	Config->LightAttack = MakeAction(Config, TEXT("IA_LightAttack"), EInputActionValueType::Boolean);
	Config->HeavyAttack = MakeAction(Config, TEXT("IA_HeavyAttack"), EInputActionValueType::Boolean);
	Config->Guard = MakeAction(Config, TEXT("IA_Guard"), EInputActionValueType::Boolean);
	Config->WeaponSkill = MakeAction(Config, TEXT("IA_WeaponSkill"), EInputActionValueType::Boolean);
	Config->LockOn = MakeAction(Config, TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	Config->UseItem = MakeAction(Config, TEXT("IA_UseItem"), EInputActionValueType::Boolean);
	Config->Interact = MakeAction(Config, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	Config->CycleRight = MakeAction(Config, TEXT("IA_CycleRight"), EInputActionValueType::Boolean);
	Config->CycleLeft = MakeAction(Config, TEXT("IA_CycleLeft"), EInputActionValueType::Boolean);
	Config->CycleQuick = MakeAction(Config, TEXT("IA_CycleQuick"), EInputActionValueType::Boolean);
	Config->TwoHand = MakeAction(Config, TEXT("IA_TwoHand"), EInputActionValueType::Boolean);
	Config->Fire = MakeAction(Config, TEXT("IA_Fire"), EInputActionValueType::Boolean);
	Config->Reload = MakeAction(Config, TEXT("IA_Reload"), EInputActionValueType::Boolean);
	Config->CycleAmmo = MakeAction(Config, TEXT("IA_CycleAmmo"), EInputActionValueType::Boolean);
	Config->Menu = MakeAction(Config, TEXT("IA_Menu"), EInputActionValueType::Boolean);
	Config->MenuUp = MakeAction(Config, TEXT("IA_MenuUp"), EInputActionValueType::Boolean);
	Config->MenuDown = MakeAction(Config, TEXT("IA_MenuDown"), EInputActionValueType::Boolean);
	Config->MenuLeft = MakeAction(Config, TEXT("IA_MenuLeft"), EInputActionValueType::Boolean);
	Config->MenuRight = MakeAction(Config, TEXT("IA_MenuRight"), EInputActionValueType::Boolean);
	Config->MenuConfirm = MakeAction(Config, TEXT("IA_MenuConfirm"), EInputActionValueType::Boolean);
	Config->MenuBack = MakeAction(Config, TEXT("IA_MenuBack"), EInputActionValueType::Boolean);

	// Movement: WASD + left stick.
	MapAxis2D(Context, Config->Move, EKeys::W, true, false);
	MapAxis2D(Context, Config->Move, EKeys::S, true, true);
	MapAxis2D(Context, Config->Move, EKeys::D, false, false);
	MapAxis2D(Context, Config->Move, EKeys::A, false, true);
	Context->MapKey(Config->Move, EKeys::Gamepad_Left2D);

	// Camera: mouse + right stick (mouse Y inverted to "look up = up").
	{
		FEnhancedActionKeyMapping& Mouse = Context->MapKey(Config->Look, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(Context);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mouse.Modifiers.Add(NegateY);
		FEnhancedActionKeyMapping& Stick = Context->MapKey(Config->Look, EKeys::Gamepad_Right2D);
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(Context);
		Scale->Scalar = FVector(2.5f, 2.f, 1.f);
		Stick.Modifiers.Add(Scale);
		UInputModifierNegate* StickNegateY = NewObject<UInputModifierNegate>(Context);
		StickNegateY->bX = false;
		StickNegateY->bY = true;
		StickNegateY->bZ = false;
		Stick.Modifiers.Add(StickNegateY);
	}

	auto Map = [Context](UInputAction* Action, std::initializer_list<FKey> Keys)
	{
		for (const FKey& Key : Keys)
		{
			Context->MapKey(Action, Key);
		}
	};

	Map(Config->Jump, { EKeys::C, EKeys::Gamepad_FaceButton_Top });
	Map(Config->DodgeSprint, { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Right });
	Map(Config->LightAttack, { EKeys::LeftMouseButton, EKeys::Gamepad_RightShoulder });
	Map(Config->HeavyAttack, { EKeys::Q, EKeys::Gamepad_RightTrigger });
	Map(Config->Guard, { EKeys::RightMouseButton, EKeys::Gamepad_LeftShoulder });
	Map(Config->WeaponSkill, { EKeys::F, EKeys::Gamepad_LeftTrigger });
	Map(Config->LockOn, { EKeys::MiddleMouseButton, EKeys::Tab, EKeys::Gamepad_RightThumbstick });
	Map(Config->UseItem, { EKeys::R, EKeys::Gamepad_FaceButton_Left });
	Map(Config->Interact, { EKeys::E, EKeys::Gamepad_FaceButton_Bottom });
	Map(Config->CycleRight, { EKeys::One, EKeys::Gamepad_DPad_Right });
	Map(Config->CycleLeft, { EKeys::Two, EKeys::Gamepad_DPad_Left });
	Map(Config->CycleQuick, { EKeys::Three, EKeys::Gamepad_DPad_Down });
	Map(Config->TwoHand, { EKeys::T, EKeys::Gamepad_DPad_Up });
	Map(Config->Menu, { EKeys::Escape, EKeys::I, EKeys::Gamepad_Special_Right });
	Map(Config->MenuUp, { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up });
	Map(Config->MenuDown, { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down });
	Map(Config->MenuLeft, { EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left });
	Map(Config->MenuRight, { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right });
	Map(Config->MenuConfirm, { EKeys::Enter, EKeys::E, EKeys::Gamepad_FaceButton_Bottom });
	Map(Config->MenuBack, { EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right });
	return Config;
}
