#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindSubsystem.generated.h"

/**
 * ワールド全体の風。数分単位でゆっくり風向が振れ、数秒単位で突風が吹く。
 * 動物の嗅覚 (風下にいるハンターの匂い) と弾道 (横風による偏流) に使う。
 */
UCLASS()
class HUNTINGGAME_API UWindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 風が吹いていく方向 (風下方向) の単位ベクトル */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindDirection() const;

	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetWindSpeedMS() const { return CurrentSpeedMS; }

	/** 風速ベクトル (m/s) */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindVelocityMS() const { return GetWindDirection() * CurrentSpeedMS; }

	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetWindHeadingDeg() const { return CurrentHeadingDeg; }

	UFUNCTION(BlueprintCallable, Category = "Wind")
	void SetPrevailingWind(float HeadingDeg, float MeanSpeedMS);

private:
	float PrevailingHeadingDeg = 45.f;
	float MeanSpeedMS = 3.f;
	float CurrentHeadingDeg = 45.f;
	float CurrentSpeedMS = 3.f;
	float Time = 0.f;
	float Seed = 0.f;
};
