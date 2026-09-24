#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HuntingHUD.generated.h"

/**
 * 最小限の HUD。リアリティ重視のため、構えていないときは照準を出さない。
 * 本格的な UI は UMG で置き換える想定。
 */
UCLASS()
class HUNTINGGAME_API AHuntingHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
	void DrawWind(float CenterX, float CenterY, float CameraYaw);
};
