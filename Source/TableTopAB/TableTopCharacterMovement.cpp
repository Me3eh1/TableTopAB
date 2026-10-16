#include "TableTopCharacterMovement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UTableTopCharacterMovement::UTableTopCharacterMovement()
{
	// Базовые параметры (дока §4, значения первого прототипа, настраиваются в BP)
	MaxWalkSpeed = 700.f;                   // 7 м/с
	MaxAcceleration = 8000.f;               // быстрый набор скорости при смене направления
	BrakingDecelerationWalking = 20000.f;   // практически мгновенная остановка (§3.1)
	GroundFriction = 8.f;
	JumpZVelocity = 650.f;                  // стартово 7 м/с по доке §4; снижено по итогам плейтеста
	AirControl = 0.35f;                     // управление в воздухе (настраиваемое)
	BrakingDecelerationFalling = 0.f;
	GravityScale = 1.55f;                   // высота прыжка ~1.4 м, собранный воздух
	bNotifyApex = false;
}

bool UTableTopCharacterMovement::StartDodge(const FVector& Direction, float DistanceCoefficient)
{
	if (IsDodgeActive() || IsDodgeCooldownActive())
	{
		return false;
	}

	FVector FlatDirection = FVector(Direction.X, Direction.Y, 0.f);
	if (!FlatDirection.Normalize())
	{
		return false;
	}

	DodgeDirection = FlatDirection;
	DodgeStartSpeed = DodgeImpulse * FMath::Clamp(DistanceCoefficient, 0.1f, 2.f);
	DodgeElapsedTime = 0.f;
	// В воздушном уклонении продолжаем текущую вертикальную скорость (§5: уклонение в падении)
	DodgeVerticalVelocity = IsMovingOnGround() ? 0.f : Velocity.Z;
	bDodgeAirborne = !IsMovingOnGround();

	// Проходим сквозь коллизию противников во время уклонения (§3.3, §7)
	SetDodgePawnCollision(true);

	Velocity = FlatDirection * DodgeStartSpeed;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ETableTopCustomMovementMode::Dodge));
	return true;
}

void UTableTopCharacterMovement::StopDodge()
{
	if (IsDodgeActive())
	{
		EndDodge(true);
	}
}

void UTableTopCharacterMovement::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (GetTableTopCustomMode())
	{
	case ETableTopCustomMovementMode::Dodge:
		PhysDodge(DeltaTime);
		break;
	default:
		// Неизвестный кастомный режим — возвращаемся в стандартную физику
		SetMovementMode(MOVE_Falling);
		break;
	}
}

void UTableTopCharacterMovement::PhysDodge(float DeltaTime)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// Обычный ввод не влияет на траекторию уклонения (§3.3)
	Acceleration = FVector::ZeroVector;

	// Опора под капсулой: наземное уклонение прижимается к полу, воздушное — падает по гравитации
	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false, nullptr);
	const bool bHasFloor = FloorResult.IsWalkableFloor();
	if (bHasFloor)
	{
		bDodgeAirborne = false;
		DodgeVerticalVelocity = 0.f;
	}
	else
	{
		bDodgeAirborne = true;
		DodgeVerticalVelocity += GetGravityZ() * DeltaTime;
	}

	DodgeElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(DodgeElapsedTime / FMath::Max(DodgeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float HorizontalSpeed = FMath::Lerp(DodgeStartSpeed, DodgeEndSpeed, Alpha);

	FVector MoveDelta = DodgeDirection * (HorizontalSpeed * DeltaTime);
	MoveDelta.Z = DodgeVerticalVelocity * DeltaTime;
	Velocity = MoveDelta / DeltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		const bool bWalkableFloorHit = IsWalkable(Hit);
		if (!bWalkableFloorHit)
		{
			const FVector FlatNormal = FVector(Hit.Normal.X, Hit.Normal.Y, 0.f).GetSafeNormal();
			const float Opposition = FlatNormal.IsNearlyZero() ? 0.f : FVector::DotProduct(DodgeDirection, -FlatNormal);
			if (Opposition >= DodgeWallStopDot)
			{
				// Встречная стена останавливает уклонение (§3.3, §7)
				EndDodge(true);
				return;
			}
		}
		// Скользящее касание — проскальзываем вдоль поверхности (§7: скольжение вдоль стен)
		SlideAlongSurface(MoveDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	// Прижимаемся к опоре, чтобы наземное уклонение не отрывалось от пола/пандусов
	if (!bDodgeAirborne && FloorResult.IsWalkableFloor() && FloorResult.FloorDist > KINDA_SMALL_NUMBER)
	{
		FHitResult SnapHit(1.f);
		SafeMoveUpdatedComponent(FVector(0.f, 0.f, -FloorResult.FloorDist), UpdatedComponent->GetComponentQuat(), true, SnapHit);
	}

	if (DodgeElapsedTime >= DodgeDuration)
	{
		EndDodge(false);
	}
}

void UTableTopCharacterMovement::EndDodge(bool bInterruptedByWall)
{
	if (!IsDodgeActive())
	{
		return;
	}

	LastDodgeEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	DodgeVerticalVelocity = 0.f;

	// Восстановление коллизии с противниками (§7)
	SetDodgePawnCollision(false);

	if (bInterruptedByWall)
	{
		Velocity = FVector::ZeroVector;
	}

	// Возврат в обычный режим по наличию опоры
	FFindFloorResult FloorResult;
	FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false, nullptr);
	SetMovementMode(FloorResult.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);
}

void UTableTopCharacterMovement::SetDodgePawnCollision(bool bIgnorePawns)
{
	if (const ACharacter* Char = CharacterOwner)
	{
		if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, bIgnorePawns ? ECR_Ignore : ECR_Block);
		}
	}
}

void UTableTopCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// При любом внешнем выходе из режима уклонения (не через EndDodge) восстанавливаем коллизию
	if (PreviousMovementMode == MOVE_Custom &&
		static_cast<ETableTopCustomMovementMode>(PreviousCustomMode) == ETableTopCustomMovementMode::Dodge &&
		!IsDodgeActive())
	{
		SetDodgePawnCollision(false);
		LastDodgeEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	}
}
