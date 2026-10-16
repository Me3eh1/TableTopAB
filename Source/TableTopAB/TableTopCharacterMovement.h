// Chernobog: компонент движения — обычное движение, прыжок/падение и кастомный режим уклонения
// (дока §3.1, §3.3, §3.4, §7)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TableTopTypes.h"
#include "TableTopCharacterMovement.generated.h"

/**
 * Уклонение реализовано кастомным режимом движения (MOVE_Custom / CMOVE_Dodge):
 *  - скорость задаётся профилем затухания, ввод не влияет на траекторию (§3.3);
 *  - встречная стена останавливает уклонение — дистанция выходит меньше расчётной (§3.3, §7);
 *  - на время уклонения капсула игнорирует канал Pawn — проход сквозь противников (§3.3, §7);
 *  - воздушное уклонение сохраняет гравитацию и заканчивается приземлением (§5, приоритет 2).
 */
UCLASS()
class TABLETOPAB_API UTableTopCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UTableTopCharacterMovement();

	/** Запускает уклонение в горизонтальном направлении (нормализуется внутри). */
	UFUNCTION(BlueprintCallable, Category = "TableTop|Dodge")
	bool StartDodge(const FVector& Direction, float DistanceCoefficient);

	/** Внешнее прерывание уклонения (оглушение и другой жёсткий контроль). */
	void StopDodge();

	UFUNCTION(BlueprintPure, Category = "TableTop|Dodge")
	bool IsDodgeActive() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ETableTopCustomMovementMode::Dodge); }

	/** Уклонение выполняется и персонаж находится на опоре. */
	UFUNCTION(BlueprintPure, Category = "TableTop|Dodge")
	bool IsDodgeGrounded() const { return IsDodgeActive() && !bDodgeAirborne; }

	UFUNCTION(BlueprintPure, Category = "TableTop|Dodge")
	bool IsDodgeCooldownActive() const { return GetDodgeCooldownRemaining() > 0.f; }

	/** Время, оставшееся до доступности следующего уклонения (0 — готово), с. */
	UFUNCTION(BlueprintPure, Category = "TableTop|Dodge")
	float GetDodgeCooldownRemaining() const
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		return FMath::Max(0.f, DodgeCooldown - (Now - LastDodgeEndTime));
	}

	// ===== Параметры уклонения (дока §4; редактируются без изменения логики) =====

	/** Импульс уклонения, см/с (дока: 13 м/с). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0"))
	float DodgeImpulse = 1300.f;

	/** Длительность уклонения, с. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.05"))
	float DodgeDuration = 0.35f;

	/** Скорость в конце уклонения, см/с: линейное затухание от импульса к этому значению. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0"))
	float DodgeEndSpeed = 200.f;

	/** Кулдаун между уклонениями, с (дока §3.3: правила восстановления). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0"))
	float DodgeCooldown = 0.4f;

	/** Насколько нормаль препятствия должна противодействовать направлению уклонения (0..1),
	 *  чтобы столкновение останавливало уклонение; скользящие касания проскальзывают вдоль стены. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DodgeWallStopDot = 0.3f;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void PhysDodge(float DeltaTime);
	void EndDodge(bool bInterruptedByWall);

	/** На время уклонения игнорирует павнов капсулой (дока §7), после — восстанавливает. */
	void SetDodgePawnCollision(bool bIgnorePawns);

	ETableTopCustomMovementMode GetTableTopCustomMode() const { return static_cast<ETableTopCustomMovementMode>(CustomMovementMode); }

	FVector DodgeDirection = FVector::ForwardVector;
	float DodgeStartSpeed = 1300.f;
	float DodgeElapsedTime = 0.f;
	float DodgeVerticalVelocity = 0.f;
	float LastDodgeEndTime = -1000.f;
	bool bDodgeAirborne = false;
};
