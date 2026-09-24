#include "Animals/AnimalSpeciesData.h"

UAnimalSpeciesData::UAnimalSpeciesData()
{
	DisplayName = NSLOCTEXT("Hunting", "SikaDeer", "Sika Deer");

	// 即時ダメージ, 出血/秒, 即死, 行動不能, 移動低下, 肉質低下, 血痕
	// 心臓: 7秒前後、肺: 15秒前後 (50〜150m 走って倒れる)、肝臓: 1分強、腹部: 数分 (長い追跡が必要)
	ZoneEffects.Add(EHitZone::Brain,  FHitZoneEffect(1.00f, 0.000f, true,  false, 0.00f, 0.05f, EBloodSign::BrightRed));
	ZoneEffects.Add(EHitZone::Spine,  FHitZoneEffect(0.50f, 0.010f, false, true,  1.00f, 0.15f, EBloodSign::SparseDrops));
	ZoneEffects.Add(EHitZone::Neck,   FHitZoneEffect(0.30f, 0.040f, false, false, 0.00f, 0.08f, EBloodSign::BrightRed));
	ZoneEffects.Add(EHitZone::Heart,  FHitZoneEffect(0.35f, 0.090f, false, false, 0.00f, 0.05f, EBloodSign::BrightRed));
	ZoneEffects.Add(EHitZone::Lungs,  FHitZoneEffect(0.25f, 0.050f, false, false, 0.00f, 0.05f, EBloodSign::BrightFrothy));
	ZoneEffects.Add(EHitZone::Liver,  FHitZoneEffect(0.15f, 0.012f, false, false, 0.00f, 0.12f, EBloodSign::DarkRed));
	ZoneEffects.Add(EHitZone::Gut,    FHitZoneEffect(0.08f, 0.003f, false, false, 0.10f, 0.35f, EBloodSign::GutMatter));
	ZoneEffects.Add(EHitZone::Leg,    FHitZoneEffect(0.08f, 0.004f, false, false, 0.45f, 0.10f, EBloodSign::SparseDrops));
	ZoneEffects.Add(EHitZone::Muscle, FHitZoneEffect(0.06f, 0.002f, false, false, 0.05f, 0.08f, EBloodSign::SparseDrops));

	// プレースホルダー胴体 (AWildAnimal の仮ボックス) に合わせたアクター空間の臓器配置。
	// スケルタルメッシュを使う場合は Bone を設定して置き換える。X+ が頭側。
	VitalOrgans.Add(FVitalOrgan(EHitZone::Brain, FVector(100.f, 0.f, 52.f), 5.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Neck,  FVector(72.f, 0.f, 38.f), 11.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Spine, FVector(40.f, 0.f, 33.f), 6.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Spine, FVector(0.f, 0.f, 33.f), 6.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Spine, FVector(-40.f, 0.f, 33.f), 6.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Heart, FVector(45.f, 0.f, -5.f), 7.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Lungs, FVector(40.f, 0.f, 12.f), 15.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Liver, FVector(15.f, 0.f, 8.f), 10.f));
	VitalOrgans.Add(FVitalOrgan(EHitZone::Gut,   FVector(-25.f, 0.f, 5.f), 22.f));

	Yields.Add(FHarvestYield(TEXT("Venison"), 25, true));
	Yields.Add(FHarvestYield(TEXT("DeerHide"), 1, false));
	Yields.Add(FHarvestYield(TEXT("Antler"), 1, false));
}
