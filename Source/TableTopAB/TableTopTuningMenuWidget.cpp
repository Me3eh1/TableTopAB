#include "TableTopTuningMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

#include "TableTopCharacter.h"
#include "TableTopCharacterMovement.h"
#include "TableTopStaminaComponent.h"

namespace TuningMenu
{
	static FSlateFontInfo HeaderFont() { return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14); }
	static FSlateFontInfo BodyFont() { return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11); }
}

// ===== Строка-слайдер =====

void UTuningSliderRow::InitRow(int32 InParamIndex, UTableTopTuningMenuWidget* InMenu)
{
	ParamIndex = InParamIndex;
	Menu = InMenu;
}

void UTuningSliderRow::NativeConstruct()
{
	Super::NativeConstruct();

	const TArray<UTableTopTuningMenuWidget::FTuningParam>& Params = UTableTopTuningMenuWidget::GetParams();
	if (!Params.IsValidIndex(ParamIndex))
	{
		return;
	}
	const auto& P = Params[ParamIndex];

	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = RowBox;

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(FText::FromString(P.DisplayName));
	Label->SetFont(TuningMenu::BodyFont());
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(230.f);
	LabelBox->AddChild(Label);
	if (UHorizontalBoxSlot* SlotObj = RowBox->AddChildToHorizontalBox(LabelBox))
	{
		SlotObj->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
		SlotObj->SetVerticalAlignment(VAlign_Center);
	}

	USlider* NewSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	NewSlider->SetMinValue(P.Min);
	NewSlider->SetMaxValue(P.Max);
	NewSlider->OnValueChanged.AddDynamic(this, &UTuningSliderRow::HandleSliderChanged);
	if (UHorizontalBoxSlot* SlotObj = RowBox->AddChildToHorizontalBox(NewSlider))
	{
		SlotObj->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotObj->SetPadding(FMargin(0.f, 4.f, 8.f, 4.f));
		SlotObj->SetVerticalAlignment(VAlign_Center);
	}
	Slider = NewSlider;

	UTextBlock* NewValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NewValue->SetFont(TuningMenu::BodyFont());
	NewValue->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 1.f, 0.6f, 1.f)));
	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(64.f);
	ValueBox->AddChild(NewValue);
	if (UHorizontalBoxSlot* SlotObj = RowBox->AddChildToHorizontalBox(ValueBox))
	{
		SlotObj->SetVerticalAlignment(VAlign_Center);
	}
	ValueText = NewValue;

	SyncFromParam();
}

void UTuningSliderRow::SyncFromParam()
{
	const TArray<UTableTopTuningMenuWidget::FTuningParam>& Params = UTableTopTuningMenuWidget::GetParams();
	ATableTopCharacter* Pawn = Menu.IsValid() ? Menu->ResolvePawn() : nullptr;
	if (!Params.IsValidIndex(ParamIndex) || !Pawn || !Slider.IsValid() || !ValueText.IsValid())
	{
		return;
	}

	bSynching = true;
	const float Current = Params[ParamIndex].Get(Pawn);
	Slider->SetValue(Current);
	ValueText->SetText(FText::FromString(FString::Printf(TEXT("%g"), Current)));
	bSynching = false;
}

void UTuningSliderRow::HandleSliderChanged(float Value)
{
	if (bSynching || !Menu.IsValid())
	{
		return;
	}
	Menu->ApplyParam(ParamIndex, Value);
	if (ValueText.IsValid())
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%g"), Value)));
	}
}

// ===== Меню =====

void UTableTopTuningMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsFocusable = true;
	BuildPanel();
}

const TArray<UTableTopTuningMenuWidget::FTuningParam>& UTableTopTuningMenuWidget::GetParams()
{
	static TArray<FTuningParam> Params;
	if (!Params.Num())
	{
		const auto CMC = [](const ATableTopCharacter* C) -> UTableTopCharacterMovement*
		{
			return Cast<UTableTopCharacterMovement>(C->GetCharacterMovement());
		};
		Params = {
			// --- Движение ---
			{TEXT("Движение"), TEXT("Скорость ходьбы"), 300.f, 1200.f, false,
				[](const ATableTopCharacter* C) { return C->BaseWalkSpeed; },
				[](ATableTopCharacter* C, float V) { C->BaseWalkSpeed = V; C->RefreshTunedMovement(); }},
			{TEXT("Движение"), TEXT("Множитель стойки"), 0.1f, 1.f, false,
				[](const ATableTopCharacter* C) { return C->GuardSpeedFactor; },
				[](ATableTopCharacter* C, float V) { C->GuardSpeedFactor = V; C->RefreshTunedMovement(); }},
			{TEXT("Движение"), TEXT("Ускорение"), 1000.f, 20000.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->MaxAcceleration : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->MaxAcceleration = V; }},
			{TEXT("Движение"), TEXT("Торможение (стоп)"), 500.f, 40000.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->BrakingDecelerationWalking : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->BrakingDecelerationWalking = V; }},
			{TEXT("Движение"), TEXT("Управление в воздухе"), 0.f, 1.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->AirControl : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->AirControl = V; }},
			{TEXT("Движение"), TEXT("Гравитация"), 0.5f, 3.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->GravityScale : 1.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->GravityScale = V; }},
			{TEXT("Движение"), TEXT("Импульс прыжка"), 300.f, 1000.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->JumpZVelocity : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->JumpZVelocity = V; }},
			// --- Уклонение ---
			{TEXT("Уклонение"), TEXT("Импульс"), 600.f, 2500.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->DodgeImpulse : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->DodgeImpulse = V; }},
			{TEXT("Уклонение"), TEXT("Длительность"), 0.15f, 0.8f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->DodgeDuration : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->DodgeDuration = V; }},
			{TEXT("Уклонение"), TEXT("Конечная скорость"), 0.f, 600.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->DodgeEndSpeed : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->DodgeEndSpeed = V; }},
			{TEXT("Уклонение"), TEXT("Кулдаун"), 0.f, 2.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->DodgeCooldown : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->DodgeCooldown = V; }},
			{TEXT("Уклонение"), TEXT("Порог стены"), 0.f, 1.f, false,
				[CMC](const ATableTopCharacter* C) { return CMC(C) ? CMC(C)->DodgeWallStopDot : 0.f; },
				[CMC](ATableTopCharacter* C, float V) { if (CMC(C)) CMC(C)->DodgeWallStopDot = V; }},
			{TEXT("Уклонение"), TEXT("Коэф. вперёд"), 0.5f, 2.f, false,
				[](const ATableTopCharacter* C) { return C->DodgeForwardDistanceCoef; },
				[](ATableTopCharacter* C, float V) { C->DodgeForwardDistanceCoef = V; }},
			{TEXT("Уклонение"), TEXT("Коэф. в сторону"), 0.5f, 2.f, false,
				[](const ATableTopCharacter* C) { return C->DodgeSideDistanceCoef; },
				[](ATableTopCharacter* C, float V) { C->DodgeSideDistanceCoef = V; }},
			{TEXT("Уклонение"), TEXT("Коэф. назад"), 0.5f, 2.f, false,
				[](const ATableTopCharacter* C) { return C->DodgeBackwardDistanceCoef; },
				[](ATableTopCharacter* C, float V) { C->DodgeBackwardDistanceCoef = V; }},
			{TEXT("Уклонение"), TEXT("Лимит серии"), 1.f, 5.f, true,
				[](const ATableTopCharacter* C) { return static_cast<float>(C->MaxConsecutiveDodges); },
				[](ATableTopCharacter* C, float V) { C->MaxConsecutiveDodges = FMath::RoundToInt(V); }},
			{TEXT("Уклонение"), TEXT("Окно сброса серии"), 0.2f, 3.f, false,
				[](const ATableTopCharacter* C) { return C->ConsecutiveDodgeResetTime; },
				[](ATableTopCharacter* C, float V) { C->ConsecutiveDodgeResetTime = V; }},
			{TEXT("Уклонение"), TEXT("Стоимость (стамина)"), 5.f, 50.f, false,
				[](const ATableTopCharacter* C) { return C->DodgeStaminaCost; },
				[](ATableTopCharacter* C, float V) { C->DodgeStaminaCost = V; }},
			// --- Выносливость ---
			{TEXT("Выносливость"), TEXT("Максимум"), 20.f, 300.f, false,
				[](const ATableTopCharacter* C) { return C->StaminaComponent ? C->StaminaComponent->GetMaxStamina() : 0.f; },
				[](ATableTopCharacter* C, float V) { if (C->StaminaComponent) C->StaminaComponent->SetMaxStamina(V); }},
			{TEXT("Выносливость"), TEXT("Реген / с"), 0.f, 100.f, false,
				[](const ATableTopCharacter* C) { return C->StaminaComponent ? C->StaminaComponent->RegenPerSecond : 0.f; },
				[](ATableTopCharacter* C, float V) { if (C->StaminaComponent) C->StaminaComponent->RegenPerSecond = V; }},
			{TEXT("Выносливость"), TEXT("Задержка регена"), 0.f, 5.f, false,
				[](const ATableTopCharacter* C) { return C->StaminaComponent ? C->StaminaComponent->RegenDelay : 0.f; },
				[](ATableTopCharacter* C, float V) { if (C->StaminaComponent) C->StaminaComponent->RegenDelay = V; }},
			// --- Камера ---
			{TEXT("Камера"), TEXT("FOV"), 70.f, 110.f, false,
				[](const ATableTopCharacter* C) { return C->FirstPersonCamera ? C->FirstPersonCamera->FieldOfView : 90.f; },
				[](ATableTopCharacter* C, float V) { if (C->FirstPersonCamera) C->FirstPersonCamera->SetFieldOfView(V); }},
		};
	}
	return Params;
}

ATableTopCharacter* UTableTopTuningMenuWidget::ResolvePawn() const
{
	return Cast<ATableTopCharacter>(GetOwningPlayerPawn());
}

void UTableTopTuningMenuWidget::BuildPanel()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Background->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	if (UCanvasPanelSlot* BgSlot = Canvas->AddChildToCanvas(Background))
	{
		BgSlot->SetAnchors(FAnchors(0.70f, 0.02f, 0.995f, 0.98f));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UCanvasPanelSlot* ScrollSlot = Canvas->AddChildToCanvas(Scroll))
	{
		ScrollSlot->SetAnchors(FAnchors(0.70f, 0.02f, 0.995f, 0.98f));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Content);

	// Заголовок
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("CHERNOBOG — TUNING (H)")));
	Title->SetFont(TuningMenu::HeaderFont());
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f, 1.f)));
	if (UVerticalBoxSlot* SlotObj = Content->AddChildToVerticalBox(Title))
	{
		SlotObj->SetPadding(FMargin(4.f, 6.f, 0.f, 6.f));
	}

	// Кнопки: Reset / Дебаг-HUD / Закрыть
	auto MakeButton = [this](UVerticalBox* Parent, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ButtonText->SetText(FText::FromString(Label));
		ButtonText->SetFont(TuningMenu::BodyFont());
		Button->AddChild(ButtonText);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(26.f);
		Box->AddChild(Button);
		if (UVerticalBoxSlot* SlotObj = Parent->AddChildToVerticalBox(Box))
		{
			SlotObj->SetPadding(FMargin(0.f, 2.f));
			SlotObj->SetHorizontalAlignment(HAlign_Left);
		}
		return Button;
	};

	UVerticalBox* Buttons = AddSection(Content, TEXT("Управление"));
	{ UButton* B = MakeButton(Buttons, TEXT("Reset к дефолтам BP"));
	if (B) { B->OnClicked.AddDynamic(this, &UTableTopTuningMenuWidget::HandleResetClicked); } }
	UButton* DebugHudButton = MakeButton(Buttons, TEXT("Дебаг-HUD: выкл"));
	if (DebugHudButton) { DebugHudButton->OnClicked.AddDynamic(this, &UTableTopTuningMenuWidget::HandleDebugHudClicked); }
	if (UTextBlock* BtnText = Cast<UTextBlock>(DebugHudButton->GetChildAt(0)))
	{
		DebugHudButtonText = BtnText;
	}
	{ UButton* B = MakeButton(Buttons, TEXT("Закрыть (H)"));
	if (B) { B->OnClicked.AddDynamic(this, &UTableTopTuningMenuWidget::HandleCloseClicked); } }

	// Строки параметров по секциям
	FString CurrentGroup;
	UVerticalBox* SectionBox = nullptr;
	const TArray<FTuningParam>& Params = GetParams();
	for (int32 i = 0; i < Params.Num(); ++i)
	{
		if (Params[i].Group != CurrentGroup)
		{
			CurrentGroup = Params[i].Group;
			SectionBox = AddSection(Content, CurrentGroup);
		}
		AddSliderRow(SectionBox, i);
	}
}

UVerticalBox* UTableTopTuningMenuWidget::AddSection(UVerticalBox* ParentBox, const FString& Title)
{
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Header->SetText(FText::FromString(Title));
	Header->SetFont(TuningMenu::HeaderFont());
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f, 1.f)));
	if (UVerticalBoxSlot* SlotObj = ParentBox->AddChildToVerticalBox(Header))
	{
		SlotObj->SetPadding(FMargin(4.f, 10.f, 0.f, 4.f));
	}

	UVerticalBox* SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ParentBox->AddChildToVerticalBox(SectionBox);
	return SectionBox;
}

void UTableTopTuningMenuWidget::AddSliderRow(UVerticalBox* SectionBox, int32 ParamIndex)
{
	UTuningSliderRow* Row = WidgetTree->ConstructWidget<UTuningSliderRow>(UTuningSliderRow::StaticClass());
	Row->InitRow(ParamIndex, this);
	if (UVerticalBoxSlot* SlotObj = SectionBox->AddChildToVerticalBox(Row))
	{
		SlotObj->SetPadding(FMargin(4.f, 0.f, 6.f, 0.f));
	}
	Rows.Add(Row);
}

void UTableTopTuningMenuWidget::ApplyParam(int32 ParamIndex, float NewValue)
{
	const TArray<FTuningParam>& Params = GetParams();
	ATableTopCharacter* Pawn = ResolvePawn();
	if (!Params.IsValidIndex(ParamIndex) || !Pawn)
	{
		return;
	}
	const FTuningParam& P = Params[ParamIndex];
	P.Set(Pawn, P.bInteger ? FMath::RoundToFloat(NewValue) : NewValue);
}

void UTableTopTuningMenuWidget::SyncAllRows()
{
	for (const TWeakObjectPtr<UTuningSliderRow>& Row : Rows)
	{
		if (Row.IsValid())
		{
			Row->SyncFromParam();
		}
	}
}

void UTableTopTuningMenuWidget::HandleResetClicked()
{
	ATableTopCharacter* Pawn = ResolvePawn();
	if (!Pawn)
	{
		return;
	}

	// Дефолты берём из CDO класса (значения из BP)
	const ATableTopCharacter* Defaults = Pawn->GetClass()->GetDefaultObject<ATableTopCharacter>();
	const TArray<FTuningParam>& Params = GetParams();
	for (const FTuningParam& P : Params)
	{
		P.Set(Pawn, P.Get(Defaults));
	}
	Pawn->RefreshTunedMovement();
	SyncAllRows();
}

void UTableTopTuningMenuWidget::HandleCloseClicked()
{
	Toggle();
}

void UTableTopTuningMenuWidget::HandleDebugHudClicked()
{
	ATableTopCharacter* Pawn = ResolvePawn();
	if (!Pawn)
	{
		return;
	}
	Pawn->bShowDebugHud = !Pawn->bShowDebugHud;
	if (DebugHudButtonText.IsValid())
	{
		DebugHudButtonText->SetText(FText::FromString(Pawn->bShowDebugHud ? TEXT("Дебаг-HUD: вкл") : TEXT("Дебаг-HUD: выкл")));
	}
}

void UTableTopTuningMenuWidget::Toggle()
{
	if (IsInViewport())
	{
		RemoveFromParent();
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
		// Возврат проектных режимов захвата/блокировки мыши (DefaultInput.ini)
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->SetMouseCaptureMode(SavedCaptureMode);
			Viewport->SetMouseLockMode(SavedLockMode);
		}
		return;
	}

	AddToViewport(10);
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;

		// Проектный дефолт CapturePermanently_IncludingInitialMouseDown отдаёт клики
		// вьюпорту игры — без явного NoCapture UI не нажимается мышью
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			SavedCaptureMode = Viewport->GetMouseCaptureMode();
			SavedLockMode = Viewport->GetMouseLockMode();
			Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
			Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
		}
	}
	SyncAllRows();
}
