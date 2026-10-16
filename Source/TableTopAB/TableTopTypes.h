// Chernobog: система передвижения — общие типы (Docs/Прототип Chernobog.md, §5)

#pragma once

#include "CoreMinimal.h"
#include "TableTopTypes.generated.h"

/** Вертикальный слой состояния: положение персонажа относительно земли. */
UENUM(BlueprintType)
enum class EVerticalMoveState : uint8
{
	Grounded	UMETA(DisplayName = "Grounded"),
	Jumping		UMETA(DisplayName = "Jumping"),
	Falling		UMETA(DisplayName = "Falling"),
	Landing		UMETA(DisplayName = "Landing"),
};

/** Слой локомоции: текущий режим перемещения. */
UENUM(BlueprintType)
enum class ELocomotionState : uint8
{
	Normal		UMETA(DisplayName = "Normal"),
	Guarded		UMETA(DisplayName = "Guarded"),
	Dashing		UMETA(DisplayName = "Dashing"),
};

/** Боевой слой состояния (заглушка под боевую систему). */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	None		UMETA(DisplayName = "None"),
	Attacking	UMETA(DisplayName = "Attacking"),
	Stunned		UMETA(DisplayName = "Stunned"),
};

/** Кастомные режимы UCharacterMovementComponent. */
UENUM(BlueprintType)
enum class ETableTopCustomMovementMode : uint8
{
	None	= 0 UMETA(Hidden),
	Dodge	= 1 UMETA(DisplayName = "Dodge"),
};
