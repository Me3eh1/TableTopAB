#include "TableTopStaminaWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "TableTopCharacter.h"
#include "TableTopStaminaComponent.h"

void UTableTopStaminaWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Виджеты ищутся по имени из Widget BP (StaminaBar / StaminaText)
	Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("StaminaBar")));
	ValueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("StaminaText")));

	// Раскладка: полоса внизу по центру, счётчик над ней (прототип; в ассете не задан)
	if (UCanvasPanelSlot* BarSlot = Bar ? Cast<UCanvasPanelSlot>(Bar->Slot) : nullptr)
	{
		BarSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		BarSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BarSlot->SetPosition(FVector2D(0.f, -90.f));
		BarSlot->SetSize(FVector2D(500.f, 24.f));
	}
	if (UCanvasPanelSlot* TextSlot = ValueText ? Cast<UCanvasPanelSlot>(ValueText->Slot) : nullptr)
	{
		TextSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		TextSlot->SetAlignment(FVector2D(0.5f, 1.f));
		TextSlot->SetPosition(FVector2D(0.f, -122.f));
		TextSlot->SetSize(FVector2D(400.f, 26.f));
	}

	TryBind();
}

void UTableTopStaminaWidget::NativeDestruct()
{
	if (OwnerCharacter.IsValid() && OwnerCharacter->StaminaComponent)
	{
		OwnerCharacter->StaminaComponent->OnStaminaChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UTableTopStaminaWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// В BeginPlay павн может быть ещё не заспавнен контроллером — добираемся, когда появился
	if (!OwnerCharacter.IsValid())
	{
		TryBind();
	}
}

void UTableTopStaminaWidget::TryBind()
{
	OwnerCharacter = Cast<ATableTopCharacter>(GetOwningPlayerPawn());
	if (!OwnerCharacter.IsValid() || !OwnerCharacter->StaminaComponent)
	{
		return;
	}

	OwnerCharacter->StaminaComponent->OnStaminaChanged.AddUniqueDynamic(this, &UTableTopStaminaWidget::HandleStaminaChanged);
	Refresh(OwnerCharacter->StaminaComponent->GetStamina());
}

void UTableTopStaminaWidget::HandleStaminaChanged(float NewStamina, float Delta)
{
	Refresh(NewStamina);
}

void UTableTopStaminaWidget::Refresh(float NewStamina)
{
	if (!OwnerCharacter.IsValid() || !OwnerCharacter->StaminaComponent)
	{
		return;
	}

	const float MaxStamina = OwnerCharacter->StaminaComponent->GetMaxStamina();
	const bool bEnoughForDodge = NewStamina >= OwnerCharacter->GetDodgeStaminaCost();

	if (Bar)
	{
		Bar->SetPercent(MaxStamina > 0.f ? NewStamina / MaxStamina : 0.f);
		// Красный — следующее уклонение недоступно
		Bar->SetFillColorAndOpacity(bEnoughForDodge
			? FLinearColor(0.15f, 0.8f, 0.35f, 1.f)
			: FLinearColor(0.9f, 0.2f, 0.15f, 1.f));
	}

	if (ValueText)
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), NewStamina, MaxStamina)));
		ValueText->SetColorAndOpacity(FSlateColor(bEnoughForDodge
			? FLinearColor(1.f, 1.f, 1.f, 1.f)
			: FLinearColor(0.9f, 0.2f, 0.15f, 1.f)));
	}
}
