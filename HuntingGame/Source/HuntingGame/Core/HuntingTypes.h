#pragma once

#include "CoreMinimal.h"
#include "HuntingTypes.generated.h"

/** 被弾部位。臓器ごとに致死性・出血量・肉質への影響が異なる */
UENUM(BlueprintType)
enum class EHitZone : uint8
{
	None,
	Brain,
	Spine,
	Neck,
	Heart,
	Lungs,
	Liver,
	Gut,
	Leg,
	Muscle
};

/** 血痕の見た目。ハンターは血の色から被弾部位を推測する */
UENUM(BlueprintType)
enum class EBloodSign : uint8
{
	None,
	SparseDrops,   // 筋肉・脚: まばらな滴
	GutMatter,     // 腹部: 暗色で内容物混じり
	DarkRed,       // 肝臓: 暗赤色
	BrightFrothy,  // 肺: 泡混じりのピンク
	BrightRed      // 心臓・動脈: 鮮血が多量
};

UENUM(BlueprintType)
enum class EHunterStance : uint8
{
	Standing,
	Crouching,
	Prone
};

UENUM(BlueprintType)
enum class EAnimalState : uint8
{
	Grazing,
	Wandering,
	Alert,
	Fleeing,
	Charging,
	Bedded,
	Dead
};

UENUM(BlueprintType)
enum class EAnimalTemperament : uint8
{
	Skittish,   // シカ: 常に逃げる
	Defensive,  // イノシシ: 手負い・至近距離なら反撃
	Aggressive  // クマ: 一定距離内なら突進
};

UENUM(BlueprintType)
enum class ETrackSignType : uint8
{
	Footprint,
	Blood,
	Bed
};

USTRUCT(BlueprintType)
struct FHitZoneEffect
{
	GENERATED_BODY()

	/** 命中時に即座に失う体力 (最大体力比) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ImmediateDamageFraction = 0.06f;

	/** 毎秒の出血 (最大体力比)。小さい値は時間とともに凝固する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float BleedFractionPerSecond = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInstantKill = false;

	/** 脊椎損傷など、その場で倒れて動けなくなる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIncapacitates = false;

	/** 移動能力の低下 (0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float MobilityLoss = 0.f;

	/** 肉質の低下 (0..1)。腹部は内容物で汚染され大きく下がる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float MeatSpoilage = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EBloodSign BloodSign = EBloodSign::SparseDrops;

	FHitZoneEffect() = default;
	FHitZoneEffect(float InImmediate, float InBleed, bool bInKill, bool bInIncap, float InMobility, float InSpoil, EBloodSign InSign)
		: ImmediateDamageFraction(InImmediate)
		, BleedFractionPerSecond(InBleed)
		, bInstantKill(bInKill)
		, bIncapacitates(bInIncap)
		, MobilityLoss(InMobility)
		, MeatSpoilage(InSpoil)
		, BloodSign(InSign)
	{
	}
};

/** 臓器の当たり判定 (球)。弾道がこの球を通過すると臓器に命中した扱い */
USTRUCT(BlueprintType)
struct FVitalOrgan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHitZone Zone = EHitZone::Heart;

	/** 基準ボーン。None の場合はアクター空間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Bone = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float RadiusCm = 10.f;

	FVitalOrgan() = default;
	FVitalOrgan(EHitZone InZone, const FVector& InOffset, float InRadius, FName InBone = NAME_None)
		: Zone(InZone), Bone(InBone), Offset(InOffset), RadiusCm(InRadius)
	{
	}
};

/** 弾薬・矢の物理パラメータ (SI 単位) */
USTRUCT(BlueprintType)
struct FAmmoSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name = TEXT(".308 Win 150gr");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float MassGrams = 9.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float MuzzleVelocityMS = 860.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float DiameterMM = 7.82f;

	/** 抗力係数。ライフル弾 ~0.3、矢は羽根の抵抗が大きく ~1.5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float DragCoefficient = 0.29f;

	/** 創傷係数。エキスパンディング弾 >1、FMJ <1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float WoundFactor = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsArrow = false;

	float GetMassKg() const { return MassGrams * 0.001f; }
	float GetCrossSectionM2() const
	{
		const float RadiusM = DiameterMM * 0.0005f;
		return PI * RadiusM * RadiusM;
	}
	float GetEnergyJ(float SpeedMS) const { return 0.5f * GetMassKg() * SpeedMS * SpeedMS; }
};

USTRUCT(BlueprintType)
struct FHarvestYield
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 BaseAmount = 1;

	/** 肉質 (被弾部位・発数) によって量が減るか */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bScaledByMeatQuality = false;

	FHarvestYield() = default;
	FHarvestYield(FName InItem, int32 InAmount, bool bInScaled)
		: ItemId(InItem), BaseAmount(InAmount), bScaledByMeatQuality(bInScaled)
	{
	}
};
