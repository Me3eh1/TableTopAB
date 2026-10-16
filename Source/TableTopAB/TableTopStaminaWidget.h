// Chernobog: HUD выносливости — полоса + счётчик, красный когда не хватает на уклонение

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TableTopStaminaWidget.generated.h"

class UProgressBar;
class UTextBlock;
class ATableTopCharacter;

UCLASS()
class TABLETOPAB_API UTableTopStaminaWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Подписка на персонажа; вызывается из Construct и повторяется в Tick, если павн ещё не готов. */
	void TryBind();

	UFUNCTION()
	void HandleStaminaChanged(float NewStamina, float Delta);

	void Refresh(float NewStamina);

	// Имена не должны совпадать с именами виджетов в WBP (is_variable генерирует
	// одноимённые BP-переменные и компилятор блюпринта падает на коллизии).
	// Поиск в NativeConstruct идёт по имени виджета: "StaminaBar" / "StaminaText".
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

	TWeakObjectPtr<ATableTopCharacter> OwnerCharacter;
};
