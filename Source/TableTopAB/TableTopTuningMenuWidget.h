// Chernobog: внутриигровая крутилка параметров движения (F1, не-Shipping билды)
// Панель строится кодом; параметры — таблица дескрипторов, новый параметр = строка в GetParams().

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TableTopTuningMenuWidget.generated.h"

class ATableTopCharacter;
class UButton;
class USlider;
class UTextBlock;
class UTableTopTuningMenuWidget;
class UVerticalBox;

/** Строка-слайдер одного параметра; знает свой индекс и владельца-меню. */
UCLASS()
class UTuningSliderRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitRow(int32 InParamIndex, UTableTopTuningMenuWidget* InMenu);
	virtual void NativeConstruct() override;

	/** Синхронизировать слайдер и подпись с текущим значением параметра. */
	void SyncFromParam();

	UFUNCTION()
	void HandleSliderChanged(float Value);

private:
	int32 ParamIndex = INDEX_NONE;
	TWeakObjectPtr<UTableTopTuningMenuWidget> Menu;
	TWeakObjectPtr<USlider> Slider;
	TWeakObjectPtr<UTextBlock> ValueText;
	bool bSynching = false;
};

UCLASS()
class TABLETOPAB_API UTableTopTuningMenuWidget : public UUserWidget
{
	GENERATED_BODY()

	friend class UTuningSliderRow;

public:
	UTableTopTuningMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Показать/скрыть с переключением режима ввода и курсора. */
	void Toggle();

	void ApplyParam(int32 ParamIndex, float NewValue);

private:
	struct FTuningParam
	{
		const TCHAR* Group;
		const TCHAR* DisplayName;
		float Min = 0.f;
		float Max = 1.f;
		bool bInteger = false;
		TFunction<float(const ATableTopCharacter*)> Get;
		TFunction<void(ATableTopCharacter*, float)> Set;
	};

	static const TArray<FTuningParam>& GetParams();

	ATableTopCharacter* ResolvePawn() const;
	void BuildPanel();
	UVerticalBox* AddSection(UVerticalBox* ParentBox, const FString& Title);
	void AddSliderRow(UVerticalBox* SectionBox, int32 ParamIndex);
	void SyncAllRows();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleDebugHudClicked();

	TArray<TWeakObjectPtr<UTuningSliderRow>> Rows;
	TWeakObjectPtr<UTextBlock> DebugHudButtonText;

	/** Проектные режимы захвата/блокировки мыши — выключаются, пока меню открыто. */
	EMouseCaptureMode SavedCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
	EMouseLockMode SavedLockMode = EMouseLockMode::LockOnCapture;
};
