// Chernobog: ресурс выносливости — расход на уклонение и блок урона в стойке (дока §3.2, §3.3)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TableTopStaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaDepleted);

UCLASS(ClassGroup = (TableTop), meta = (BlueprintSpawnableComponent))
class TABLETOPAB_API UTableTopStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTableTopStaminaComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Пытается списать выносливость; false — если её недостаточно (команда игнорируется). */
	UFUNCTION(BlueprintCallable, Category = "TableTop|Stamina")
	bool TryConsume(float Amount);

	/** Хук боевой системы: урон в защитной стойке расходует выносливость вместо здоровья.
	 *  Возвращает урон, не поглощённый выносливостью (его должна получить шкала здоровья). */
	UFUNCTION(BlueprintCallable, Category = "TableTop|Stamina")
	float AbsorbGuardDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "TableTop|Stamina")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "TableTop|Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "TableTop|Stamina")
	float GetStaminaPercent() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }

	/** Смена максимума с клампом текущего запаса (крутилка параметров). */
	UFUNCTION(BlueprintCallable, Category = "TableTop|Stamina")
	void SetMaxStamina(float NewMax);

	UPROPERTY(BlueprintAssignable, Category = "TableTop|Stamina")
	FOnStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "TableTop|Stamina")
	FOnStaminaDepleted OnStaminaDepleted;

	/** Максимальный запас выносливости. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Stamina")
	float MaxStamina = 100.f;

	/** Восстановление в секунду. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Stamina")
	float RegenPerSecond = 25.f;

	/** Задержка перед началом восстановления после расхода, с. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Stamina")
	float RegenDelay = 1.f;

private:
	float Stamina = 0.f;
	float TimeSinceConsumption = 0.f;
	bool bDepletedNotified = false;

	void ChangeStamina(float Delta);
};
