// Chernobog: персонаж — ввод, трёхслойные состояния, защитная стойка, приоритеты действий
// (дока §2, §3.2, §5, §8)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TableTopTypes.h"
#include "TableTopCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class UCameraComponent;
class UTableTopCharacterMovement;
class UTableTopStaminaComponent;

/** Атака из защитной стойки — особая атака riposte (дока §3.2); логику подключит боевая система. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRiposteTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTableTopStateChanged,
	EVerticalMoveState, Vertical, ELocomotionState, Locomotion, ECombatState, Combat);

UCLASS(Blueprintable)
class TABLETOPAB_API ATableTopCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATableTopCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	// ===== Публичный API для боевой системы =====

	/** Жёсткий контроль: полностью блокирует движение и действия (приоритет №1, дока §5). */
	UFUNCTION(BlueprintCallable, Category = "TableTop|Combat")
	void SetStunned(float Duration);

	UFUNCTION(BlueprintPure, Category = "TableTop|Combat")
	bool IsStunned() const { return CombatState == ECombatState::Stunned; }

	UFUNCTION(BlueprintPure, Category = "TableTop|State")
	EVerticalMoveState GetVerticalState() const { return VerticalState; }

	UFUNCTION(BlueprintPure, Category = "TableTop|State")
	ELocomotionState GetLocomotionState() const { return LocomotionState; }

	UFUNCTION(BlueprintPure, Category = "TableTop|State")
	ECombatState GetCombatState() const { return CombatState; }

	UPROPERTY(BlueprintAssignable, Category = "TableTop")
	FOnRiposteTriggered OnRiposteTriggered;

	UPROPERTY(BlueprintAssignable, Category = "TableTop")
	FOnTableTopStateChanged OnStateChanged;

	// ===== Отладочные триггеры (автотесты/QA: дергают те же обработчики, что и ввод) =====

	UFUNCTION(BlueprintCallable, Category = "TableTop|Debug")
	void DebugMoveInput(float Forward, float Right);

	UFUNCTION(BlueprintCallable, Category = "TableTop|Debug")
	void DebugTriggerDodge() { TryStartDodge(); }

	UFUNCTION(BlueprintCallable, Category = "TableTop|Debug")
	void DebugTriggerJump() { Input_JumpStarted(); }

	UFUNCTION(BlueprintCallable, Category = "TableTop|Debug")
	void DebugGuardHeld(bool bHeld) { bHeld ? Input_GuardStarted() : Input_GuardCompleted(); }

	UFUNCTION(BlueprintCallable, Category = "TableTop|Debug")
	void DebugTriggerAttack() { Input_Attack(); }


	// ===== Компоненты =====

	UPROPERTY(VisibleAnywhere, Category = "TableTop")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "TableTop")
	TObjectPtr<UTableTopStaminaComponent> StaminaComponent;

	// ===== Ввод (дока §2) =====

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> MoveForwardAction;		// Axis1D: W/S

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> MoveRightAction;		// Axis1D: D/A

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> LookAction;			// Axis2D: мышь

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> JumpAction;			// Digital: Space

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> DodgeAction;			// Digital: Ctrl

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> GuardAction;			// Digital (удержание): ПКМ

	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Input")
	TObjectPtr<UInputAction> AttackAction;			// Digital: ЛКМ (заглушка под riposte)

	// ===== Параметры (дока §4; настраиваются в BP без изменения логики) =====

	/** Базовая скорость ходьбы, см/с (дока: 700). */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Movement", meta = (ClampMin = "0.0"))
	float BaseWalkSpeed = 700.f;

	/** Множитель скорости в защитной стойке (дока: 0.5). */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardSpeedFactor = 0.5f;

	/** Длительность состояния Landing, с. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Movement", meta = (ClampMin = "0.0"))
	float LandingDuration = 0.15f;

	/** Стоимость уклонения в выносливости. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0"))
	float DodgeStaminaCost = 25.f;

	/** Максимум последовательных уклонений по земле. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "1"))
	int32 MaxConsecutiveDodges = 2;

	/** Окно сброса счётчика последовательных уклонений, с. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.0"))
	float ConsecutiveDodgeResetTime = 0.9f;

	/** Коэффициент дистанции уклонения вперёд. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DodgeForwardDistanceCoef = 1.0f;

	/** Коэффициент дистанции уклонения назад. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DodgeBackwardDistanceCoef = 0.75f;

	/** Коэффициент дистанции уклонения в сторону. */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Dodge", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DodgeSideDistanceCoef = 0.9f;

	/** Отладочный HUD состояний (для проверки требований доки). */
	UPROPERTY(EditDefaultsOnly, Category = "TableTop|Debug")
	bool bShowDebugHud = true;

private:
	// ===== Обработка ввода =====
	void Input_MoveForward(const FInputActionValue& Value);
	void Input_MoveRight(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	void Input_Dodge();
	void Input_GuardStarted();
	void Input_GuardCompleted();
	void Input_Attack();

	// ===== Состояния =====
	void SetVerticalState(EVerticalMoveState NewState);
	void SetLocomotionState(ELocomotionState NewState);
	void SetCombatState(ECombatState NewState);
	void RefreshVerticalStateFromMovement();
	void RefreshGuardState();
	void RefreshLocomotionState();
	void UpdateWalkSpeed();

	void TryStartDodge();
	float ComputeDodgeDistanceCoefficient(const FVector& WorldDirection) const;
	void HandleGroundedEntered();
	void OnLandingFinished();
	void OnStunExpired();

	void DrawDebugHud();

	UTableTopCharacterMovement* GetTableTopMovement() const;

	EVerticalMoveState VerticalState = EVerticalMoveState::Grounded;
	ELocomotionState LocomotionState = ELocomotionState::Normal;
	ECombatState CombatState = ECombatState::None;

	bool bGuardHeld = false;
	bool bGuardActive = false;

	/** Последний ввод движения (X — вправо, Y — вперёд); уклонение берёт направление из него (§3.3). */
	FVector2D LastMoveInput = FVector2D::ZeroVector;

	int32 ConsecutiveDodgeCount = 0;
	float LastDodgeTime = -1000.f;
	bool bAirDodgeAvailable = true;

	FTimerHandle LandingTimerHandle;
	FTimerHandle StunTimerHandle;
};
