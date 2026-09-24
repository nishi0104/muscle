#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/HuntingTypes.h"
#include "AnimalSpeciesData.generated.h"

/**
 * 動物種ごとのパラメータ。デフォルト値はニホンジカ相当。
 * エディタで DataAsset を複製してイノシシ・クマなどを作る。
 * 距離は cm、速度は cm/s (Unreal 単位)。
 */
UCLASS(BlueprintType)
class HUNTINGGAME_API UAnimalSpeciesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UAnimalSpeciesData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species")
	EAnimalTemperament Temperament = EAnimalTemperament::Skittish;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Species", meta = (ClampMin = "1"))
	float LiveWeightKg = 60.f;

	// ---- 移動 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 140.f;

	/** シカの全力疾走は約 45km/h */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float RunSpeed = 1250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float ChargeSpeed = 900.f;

	/** 全力疾走を続けられる秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float MaxStamina = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WanderRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float FleeDistance = 15000.f;

	// ---- 感覚 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float SightRange = 15000.f;

	/** 草食獣は目が横についており視野が広い (片側 150° ≒ 全周 300°) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float SightHalfAngleDeg = 150.f;

	/** 静止している人間を認識する速さ。草食獣は静止物の識別が苦手 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float StillDetectionRate = 0.05f;

	/** 動きへの感度。草食獣は動きに非常に敏感 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float MotionSensitivity = 1.5f;

	/** 最大音量 (全力疾走) の足音が聞こえる距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float HearingRange = 8000.f;

	/** 風速 4m/s 前後で人間の匂いを感知できる距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float SmellRange = 30000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float AlertThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float FleeThreshold = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float AwarenessDecayPerSec = 0.06f;

	/** 群れの仲間に警戒を伝える範囲 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses")
	float HerdAlarmRadius = 4000.f;

	// ---- 戦闘 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float AttackDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float AttackRange = 180.f;

	/** Defensive/Aggressive 種がこの距離内の脅威に反撃する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float ChargeTriggerDistance = 2500.f;

	// ---- 被弾 ----
	/** 確実に仕留めるのに必要な命中エネルギー (ライフル弾)。シカで約 1350J (1000ft-lb) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	float MinBulletEnergyJ = 1350.f;

	/** 矢は切り裂いて出血させるため必要エネルギーは小さい。シカで約 55J (40ft-lb) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	float MinArrowEnergyJ = 55.f;

	/** 十分なエネルギーの弾が体内を進む最大距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	float BodyDepthCm = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	TArray<FVitalOrgan> VitalOrgans;

	/** ボーン名 → 部位。親ボーンを辿って解決する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	TMap<FName, EHitZone> BoneToZone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	TMap<EHitZone, FHitZoneEffect> ZoneEffects;

	// ---- 解体 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
	TArray<FHarvestYield> Yields;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest")
	float HarvestSeconds = 8.f;
};
