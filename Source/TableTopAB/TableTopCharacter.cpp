#include "TableTopCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "TableTopCharacterMovement.h"
#include "TableTopStaminaComponent.h"
#include "TableTopStaminaWidget.h"
#include "TableTopTuningMenuWidget.h"

ATableTopCharacter::ATableTopCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTableTopCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// От первого лица: капсула поворачивается по контроллеру, камера — на высоте глаз
	bUseControllerRotationYaw = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 68.f));
	FirstPersonCamera->SetRelativeRotation(FRotator::ZeroRotator);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 90.f;

	StaminaComponent = CreateDefaultSubobject<UTableTopStaminaComponent>(TEXT("StaminaComponent"));
}

void ATableTopCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateWalkSpeed();

	if (IsLocallyControlled() && StaminaHUDClass)
	{
		StaminaHUD = CreateWidget<UTableTopStaminaWidget>(GetWorld(), StaminaHUDClass);
		if (StaminaHUD)
		{
			StaminaHUD->AddToViewport();
		}
	}
}

void ATableTopCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshVerticalStateFromMovement();
	RefreshGuardState();
	RefreshLocomotionState();
	DrawDebugHud();
}

void ATableTopCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Контекст ввода добавляем локальному игроку
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ATableTopCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ATableTopCharacter::Input_MoveForward);
		Input->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ATableTopCharacter::Input_MoveRight);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATableTopCharacter::Input_Look);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ATableTopCharacter::Input_JumpStarted);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATableTopCharacter::Input_JumpCompleted);
		Input->BindAction(DodgeAction, ETriggerEvent::Started, this, &ATableTopCharacter::Input_Dodge);
		Input->BindAction(GuardAction, ETriggerEvent::Started, this, &ATableTopCharacter::Input_GuardStarted);
		Input->BindAction(GuardAction, ETriggerEvent::Completed, this, &ATableTopCharacter::Input_GuardCompleted);
		Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ATableTopCharacter::Input_Attack);
#if !UE_BUILD_SHIPPING
		Input->BindAction(TuningMenuAction, ETriggerEvent::Started, this, &ATableTopCharacter::Input_ToggleTuningMenu);
#endif
	}
}

// ===== Обработка ввода =====

void ATableTopCharacter::Input_MoveForward(const FInputActionValue& Value)
{
	if (IsStunned())
	{
		return;
	}

	const float Axis = Value.Get<float>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis);
}

void ATableTopCharacter::Input_MoveRight(const FInputActionValue& Value)
{
	if (IsStunned())
	{
		return;
	}

	const float Axis = Value.Get<float>();
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis);
}

void ATableTopCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ATableTopCharacter::Input_JumpStarted()
{
	// Прыжок доступен только с опоры (дока §5, приоритет 4); оглушение блокирует ввод
	if (IsStunned())
	{
		return;
	}
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		Jump();
	}
}

void ATableTopCharacter::Input_JumpCompleted()
{
	StopJumping();
}

void ATableTopCharacter::Input_Dodge()
{
	TryStartDodge();
}

void ATableTopCharacter::Input_GuardStarted()
{
	bGuardHeld = true;
	RefreshGuardState();
}

void ATableTopCharacter::Input_GuardCompleted()
{
	bGuardHeld = false;
	RefreshGuardState();
}

void ATableTopCharacter::Input_Attack()
{
	if (IsStunned())
	{
		return;
	}

	if (LocomotionState == ELocomotionState::Guarded)
	{
		// Атака из защитной стойки — особая атака riposte (дока §3.2)
		UE_LOG(LogTemp, Log, TEXT("[Chernobog] Riposte triggered"));
		OnRiposteTriggered.Broadcast();
	}
	else
	{
		// Заглушка: боевая система подключит позже (дока §8)
		UE_LOG(LogTemp, Log, TEXT("[Chernobog] Attack stub (combat system TBD)"));
	}
}

void ATableTopCharacter::Input_ToggleTuningMenu()
{
#if !UE_BUILD_SHIPPING
	if (!TuningMenu)
	{
		TuningMenu = CreateWidget<UTableTopTuningMenuWidget>(GetWorld(), UTableTopTuningMenuWidget::StaticClass());
	}
	if (TuningMenu)
	{
		TuningMenu->Toggle();
	}
#endif
}

// ===== Отладочные триггеры =====

void ATableTopCharacter::DebugMoveInput(float Forward, float Right)
{
	Input_MoveForward(FInputActionValue(Forward));
	Input_MoveRight(FInputActionValue(Right));
}

// ===== Уклонение =====

void ATableTopCharacter::TryStartDodge()
{
	// Приоритет №1: жёсткий контроль блокирует всё (дока §5)
	if (IsStunned())
	{
		return;
	}

	UTableTopCharacterMovement* Movement = GetTableTopMovement();
	if (!Movement || Movement->IsDodgeActive() || Movement->IsDodgeCooldownActive())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bGrounded = Movement->IsMovingOnGround();

	if (bGrounded)
	{
		// Лимит последовательных уклонений; окно истекло — счётчик сбрасывается (§3.3)
		if (ConsecutiveDodgeCount >= MaxConsecutiveDodges)
		{
			if (Now - LastDodgeTime < ConsecutiveDodgeResetTime)
			{
				return;
			}
			ConsecutiveDodgeCount = 0;
		}
	}
	else
	{
		// Уклонение в падении — одно до приземления (§5, приоритет 2)
		if (!bAirDodgeAvailable || VerticalState != EVerticalMoveState::Falling)
		{
			return;
		}
	}

	// Недостаточно выносливости — команда игнорируется (§5: без некорректных состояний)
	if (!StaminaComponent || !StaminaComponent->TryConsume(DodgeStaminaCost))
	{
		return;
	}

	// Направление: живой ввод движения в момент активации (то, что потребил CMC в прошлом тике),
	// иначе направление взгляда (§3.3). Кэш по ETriggerEvent не годится: на отпускании клавиши
	// приходит только Completed, и закэшированное значение зависало бы навсегда.
	FVector Direction;
	const FVector InputVector = GetCharacterMovement()->GetLastInputVector().GetSafeNormal2D();
	Direction = !InputVector.IsNearlyZero() ? InputVector : GetActorForwardVector();

	if (Movement->StartDodge(Direction, ComputeDodgeDistanceCoefficient(Direction)))
	{
		ConsecutiveDodgeCount++;
		LastDodgeTime = Now;
		if (!bGrounded)
		{
			bAirDodgeAvailable = false;
		}
	}
}

float ATableTopCharacter::ComputeDodgeDistanceCoefficient(const FVector& WorldDirection) const
{
	// Коэффициенты дистанции по направлениям относительно взгляда (дока §4)
	const float Dot = FVector::DotProduct(WorldDirection.GetSafeNormal2D(), GetActorForwardVector().GetSafeNormal2D());
	const float Forwardness = FMath::Clamp((Dot + 1.f) * 0.5f, 0.f, 1.f);
	const float Base = FMath::Lerp(DodgeBackwardDistanceCoef, DodgeForwardDistanceCoef, Forwardness);
	const float Sideways = 1.f - FMath::Abs(Dot);
	return FMath::Lerp(Base, DodgeSideDistanceCoef, Sideways * 0.5f);
}

// ===== Защитная стойка =====

void ATableTopCharacter::RefreshGuardState()
{
	// Стойка активна, пока удерживается кнопка и действие совместимо:
	// уклонение и оглушение прекращают стойку (дока §3.2); при нуле выносливости не активируется
	const bool bShouldGuard = bGuardHeld
		&& !IsStunned()
		&& !(GetTableTopMovement() && GetTableTopMovement()->IsDodgeActive())
		&& StaminaComponent
		&& StaminaComponent->GetStamina() > 0.f;

	if (bShouldGuard != bGuardActive)
	{
		bGuardActive = bShouldGuard;
		UpdateWalkSpeed();
	}
}

void ATableTopCharacter::UpdateWalkSpeed()
{
	if (UTableTopCharacterMovement* Movement = GetTableTopMovement())
	{
		Movement->MaxWalkSpeed = bGuardActive ? BaseWalkSpeed * GuardSpeedFactor : BaseWalkSpeed;
	}
}

// ===== Состояния =====

void ATableTopCharacter::RefreshVerticalStateFromMovement()
{
	const UTableTopCharacterMovement* Movement = GetTableTopMovement();
	if (!Movement)
	{
		return;
	}

	// На земле: обычное движение или наземное уклонение; иначе Jumping/Falling (§3.4)
	const bool bAirborne = !Movement->IsMovingOnGround() && !Movement->IsDodgeGrounded();
	if (bAirborne)
	{
		SetVerticalState(Movement->Velocity.Z > KINDA_SMALL_NUMBER
			? EVerticalMoveState::Jumping
			: EVerticalMoveState::Falling);
		return;
	}

	// Переход из воздуха на опору — приземление (§5: Landing)
	if (VerticalState == EVerticalMoveState::Jumping || VerticalState == EVerticalMoveState::Falling)
	{
		HandleGroundedEntered();
	}
}

void ATableTopCharacter::HandleGroundedEntered()
{
	// Сброс правил уклонения после полёта
	ConsecutiveDodgeCount = 0;
	bAirDodgeAvailable = true;

	SetVerticalState(EVerticalMoveState::Landing);
	GetWorldTimerManager().ClearTimer(LandingTimerHandle);
	GetWorldTimerManager().SetTimer(LandingTimerHandle, this, &ATableTopCharacter::OnLandingFinished, LandingDuration, false);
}

void ATableTopCharacter::OnLandingFinished()
{
	if (VerticalState == EVerticalMoveState::Landing)
	{
		SetVerticalState(EVerticalMoveState::Grounded);
	}
}

void ATableTopCharacter::RefreshLocomotionState()
{
	// Слой локомоции — производное: уклонение перекрывает стойку (дока §3.2, §3.3)
	ELocomotionState NewState = ELocomotionState::Normal;
	if (const UTableTopCharacterMovement* Movement = GetTableTopMovement())
	{
		if (Movement->IsDodgeActive())
		{
			NewState = ELocomotionState::Dashing;
		}
		else if (bGuardActive)
		{
			NewState = ELocomotionState::Guarded;
		}
	}
	SetLocomotionState(NewState);
}

void ATableTopCharacter::SetVerticalState(EVerticalMoveState NewState)
{
	if (VerticalState == NewState)
	{
		return;
	}
	VerticalState = NewState;
	OnStateChanged.Broadcast(VerticalState, LocomotionState, CombatState);
}

void ATableTopCharacter::SetLocomotionState(ELocomotionState NewState)
{
	if (LocomotionState == NewState)
	{
		return;
	}
	LocomotionState = NewState;
	OnStateChanged.Broadcast(VerticalState, LocomotionState, CombatState);
}

void ATableTopCharacter::SetCombatState(ECombatState NewState)
{
	if (CombatState == NewState)
	{
		return;
	}
	CombatState = NewState;
	OnStateChanged.Broadcast(VerticalState, LocomotionState, CombatState);
}

// ===== Жёсткий контроль =====

void ATableTopCharacter::SetStunned(float Duration)
{
	SetCombatState(ECombatState::Stunned);

	// Оглушение прерывает стойку и уклонение (дока §5, приоритет 1)
	bGuardHeld = false;
	if (UTableTopCharacterMovement* Movement = GetTableTopMovement())
	{
		Movement->StopDodge();
	}
	GetCharacterMovement()->StopMovementImmediately();

	GetWorldTimerManager().ClearTimer(StunTimerHandle);
	GetWorldTimerManager().SetTimer(StunTimerHandle, this, &ATableTopCharacter::OnStunExpired, FMath::Max(Duration, 0.1f), false);
}

void ATableTopCharacter::OnStunExpired()
{
	SetCombatState(ECombatState::None);
}

// ===== Отладка =====

UTableTopCharacterMovement* ATableTopCharacter::GetTableTopMovement() const
{
	return Cast<UTableTopCharacterMovement>(GetCharacterMovement());
}

void ATableTopCharacter::DrawDebugHud()
{
	if (!bShowDebugHud)
	{
		return;
	}

	const UTableTopCharacterMovement* Movement = GetTableTopMovement();
	if (!Movement)
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	const bool bMoving = Movement->IsMovingOnGround() && Speed > 30.f;
	const float CooldownLeft = Movement->GetDodgeCooldownRemaining();

	const FString Text = FString::Printf(
		TEXT("Vertical: %s%s\nLocomotion: %s\nCombat: %s\nSpeed: %.0f cm/s | Stamina: %.0f\nDodge CD: %.2f | Chain: %d/%d | AirDodge: %s"),
		*UEnum::GetValueAsString(VerticalState), bMoving ? TEXT(" (Moving)") : TEXT(""),
		*UEnum::GetValueAsString(LocomotionState),
		*UEnum::GetValueAsString(CombatState),
		Speed, StaminaComponent ? StaminaComponent->GetStamina() : -1.f,
		CooldownLeft, ConsecutiveDodgeCount, MaxConsecutiveDodges,
		bAirDodgeAvailable ? TEXT("yes") : TEXT("no"));

	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 140.f), Text, nullptr, FColor::White, 0.f, true);
}
