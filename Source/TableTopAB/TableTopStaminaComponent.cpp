#include "TableTopStaminaComponent.h"

UTableTopStaminaComponent::UTableTopStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTableTopStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	Stamina = MaxStamina;
	TimeSinceConsumption = 0.f;
}

void UTableTopStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceConsumption += DeltaTime;
	if (Stamina < MaxStamina && TimeSinceConsumption >= RegenDelay)
	{
		ChangeStamina(RegenPerSecond * DeltaTime);
	}
}

bool UTableTopStaminaComponent::TryConsume(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	if (Stamina < Amount)
	{
		return false;
	}
	ChangeStamina(-Amount);
	return true;
}

float UTableTopStaminaComponent::AbsorbGuardDamage(float DamageAmount)
{
	const float Absorbed = FMath::Min(Stamina, DamageAmount);
	if (Absorbed > 0.f)
	{
		ChangeStamina(-Absorbed);
	}
	return DamageAmount - Absorbed;
}

void UTableTopStaminaComponent::ChangeStamina(float Delta)
{
	const float OldStamina = Stamina;
	Stamina = FMath::Clamp(Stamina + Delta, 0.f, MaxStamina);

	if (!FMath::IsNearlyEqual(OldStamina, Stamina))
	{
		if (Delta < 0.f)
		{
			TimeSinceConsumption = 0.f;
		}
		OnStaminaChanged.Broadcast(Stamina, Stamina - OldStamina);

		if (Stamina <= 0.f && !bDepletedNotified)
		{
			bDepletedNotified = true;
			OnStaminaDepleted.Broadcast();
		}
		else if (Stamina > 0.f)
		{
			bDepletedNotified = false;
		}
	}
}
