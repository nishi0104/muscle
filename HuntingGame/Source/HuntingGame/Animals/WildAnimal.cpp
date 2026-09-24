#include "Animals/WildAnimal.h"
#include "Animals/AnimalSpeciesData.h"
#include "Environment/WindSubsystem.h"
#include "Items/InventoryComponent.h"
#include "Player/HunterCharacter.h"
#include "Tracking/TrackSign.h"
#include "HuntingGame.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 仮ボックスの寸法 (cm)。UAnimalSpeciesData の既定臓器配置と対応している
	const FVector BodyExtent(70.f, 20.f, 28.f);
	const FVector BodyCenter(0.f, 0.f, 10.f);
	const FVector HeadExtent(28.f, 10.f, 12.f);
	const FVector HeadCenter(88.f, 0.f, 45.f);

	int32 BloodSignSeverity(EBloodSign Sign)
	{
		return static_cast<int32>(Sign);
	}

	void SetupPlaceholder(UStaticMeshComponent* Comp, UStaticMesh* Cube, const FVector& Center, const FVector& Extent)
	{
		if (Cube)
		{
			Comp->SetStaticMesh(Cube);
		}
		Comp->SetRelativeLocation(Center);
		Comp->SetRelativeScale3D(Extent / 50.f); // BasicShapes/Cube は 100cm
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
		Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Comp->SetCanEverAffectNavigation(false);
	}
}

AWildAnimal::AWildAnimal()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(45.f, 75.f);
	// 弾は Capsule を素通りさせ、メッシュ (物理アセット) か仮ボックスで判定する
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 220.f, 0.f);
	Move->MaxWalkSpeed = 140.f;
	Move->bUseRVOAvoidance = true;
	Move->MaxAcceleration = 900.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	SetupPlaceholder(PlaceholderBody, CubeFinder.Object, BodyCenter, BodyExtent);

	PlaceholderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
	PlaceholderHead->SetupAttachment(GetCapsuleComponent());
	SetupPlaceholder(PlaceholderHead, CubeFinder.Object, HeadCenter, HeadExtent);
}

void AWildAnimal::BeginPlay()
{
	Super::BeginPlay();

	if (!Species)
	{
		Species = NewObject<UAnimalSpeciesData>(this);
	}

	MaxHealthCached = Species->MaxHealth;
	Health = MaxHealthCached;
	Stamina = Species->MaxStamina;
	HomeLocation = GetActorLocation();
	LastFootprintLocation = GetActorLocation();
	NextDecisionTime = FMath::FRandRange(3.f, 12.f);

	// 本物のメッシュがあれば仮ボックスは消す
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		for (UStaticMeshComponent* Comp : { PlaceholderBody.Get(), PlaceholderHead.Get() })
		{
			Comp->SetVisibility(false);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void AWildAnimal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDebugDrawVitals && Species)
	{
		for (const FVitalOrgan& Organ : Species->VitalOrgans)
		{
			DrawDebugSphere(GetWorld(), GetOrganWorldLocation(Organ), Organ.RadiusCm, 8, FColor::Red);
		}
	}

	if (IsDead())
	{
		return;
	}

	AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);

	UpdateWounds(DeltaSeconds);
	if (IsDead())
	{
		return;
	}
	UpdateSenses(DeltaSeconds);
	UpdateBehavior(DeltaSeconds);
	UpdateLocomotion(DeltaSeconds);
	UpdateTracks();
}

// ------------------------------------------------------------------
// 出血
// ------------------------------------------------------------------

void AWildAnimal::UpdateWounds(float Dt)
{
	if (BleedRate <= 0.f)
	{
		return;
	}

	Health -= BleedRate * Dt;

	// 浅い傷 (筋肉など) は凝固して止血していく
	const float ClotThreshold = MaxHealthCached * 0.0025f;
	if (BleedRate < ClotThreshold)
	{
		BleedRate = FMath::Max(0.f, BleedRate - MaxHealthCached * 0.0002f * Dt);
	}

	if (Health <= 0.f)
	{
		Die();
		return;
	}

	// 走ると心拍が上がり血痕が増える
	const float Speed = GetVelocity().Size2D();
	BloodAccumulator += BleedRate * Dt * (1.f + Speed / 300.f);
	const float Now = GetWorld()->GetTimeSeconds();
	if (BloodAccumulator >= MaxHealthCached * 0.04f && Now - LastBloodSpawnTime > 0.15f)
	{
		BloodAccumulator = 0.f;
		LastBloodSpawnTime = Now;
		const float Intensity = FMath::Clamp(BleedRate / (MaxHealthCached * 0.05f), 0.3f, 2.f);
		SpawnTrackSign(BloodClass, ETrackSignType::Blood, Intensity);
	}
}

// ------------------------------------------------------------------
// 感覚
// ------------------------------------------------------------------

void AWildAnimal::UpdateSenses(float Dt)
{
	float Stimulus = 0.f;
	AHunterCharacter* Hunter = GetHunter();

	if (Hunter && !Hunter->IsDead())
	{
		const FVector Eye = GetEyeLocation();
		const FVector HunterEye = Hunter->GetEyeLocation();
		const FVector ToHunter = HunterEye - Eye;
		const float Dist = ToHunter.Size();

		// 視覚: 動いているものに強く反応し、静止物はなかなか見分けられない
		if (Dist < Species->SightRange)
		{
			const float Cos = FVector::DotProduct(GetActorForwardVector(), ToHunter.GetSafeNormal());
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.f, 1.f)));
			if (AngleDeg <= Species->SightHalfAngleDeg && HasLineOfSight(Eye, Hunter, HunterEye))
			{
				const float DistFactor = FMath::Square(1.f - Dist / Species->SightRange);
				const float Peripheral = FMath::Lerp(1.f, 0.5f, AngleDeg / Species->SightHalfAngleDeg);
				const float Motion = Hunter->GetMovementFactor();
				const float Rate = Hunter->GetVisibilityFactor() * DistFactor * Peripheral
					* (Species->StillDetectionRate + Motion * Species->MotionSensitivity);
				if (Rate > KINDA_SMALL_NUMBER)
				{
					Stimulus += Rate;
					ThreatLocation = HunterEye;
				}
			}
		}

		// 聴覚: 足音の大きさに比例した範囲。方向はおおよそしか分からない
		const float AudibleRange = Species->HearingRange * Hunter->GetNoiseLevel();
		if (Dist < AudibleRange)
		{
			Stimulus += (1.f - Dist / AudibleRange) * 1.2f;
			ThreatLocation = Hunter->GetActorLocation() + FMath::VRand() * Dist * 0.15f;
		}

		// 嗅覚: ハンターの風下にいると匂いで確実に気づく
		if (const UWindSubsystem* Wind = GetWorld()->GetSubsystem<UWindSubsystem>())
		{
			const FVector FromHunter = (GetActorLocation() - Hunter->GetActorLocation()).GetSafeNormal2D();
			const float Downwind = FVector::DotProduct(Wind->GetWindDirection(), FromHunter);
			const float ScentRange = Species->SmellRange
				* FMath::Clamp(Wind->GetWindSpeedMS() / 4.f, 0.3f, 1.5f)
				* Hunter->GetScentLevel();
			if (Downwind > 0.6f && Dist < ScentRange)
			{
				const float Cone = (Downwind - 0.6f) / 0.4f;
				Stimulus += Cone * (1.f - Dist / ScentRange) * 3.f;
				ThreatLocation = Hunter->GetActorLocation();
			}
		}
	}

	if (Stimulus > 0.f)
	{
		Awareness = FMath::Min(Awareness + Stimulus * Dt, Species->FleeThreshold * 1.5f);
	}
	else
	{
		Awareness = FMath::Max(0.f, Awareness - Species->AwarenessDecayPerSec * Dt);
	}
}

bool AWildAnimal::HasLineOfSight(const FVector& From, const AActor* Target, const FVector& TargetPoint) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnimalSight), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, TargetPoint, ECC_Visibility, Params))
	{
		return true;
	}
	return Hit.GetActor() == Target;
}

FVector AWildAnimal::GetEyeLocation() const
{
	if (GetMesh()->GetSkeletalMeshAsset() && GetMesh()->DoesSocketExist(EyeSocketName))
	{
		return GetMesh()->GetSocketLocation(EyeSocketName);
	}
	return GetActorTransform().TransformPosition(HeadCenter + FVector(10.f, 0.f, 8.f));
}

void AWildAnimal::HearLoudNoise(const FVector& Location, float AudibleRangeCm)
{
	if (IsDead())
	{
		return;
	}
	const float Dist = FVector::Dist(Location, GetActorLocation());
	if (Dist > AudibleRangeCm)
	{
		return;
	}

	// 遠くの銃声は方向を取り違えることが多い
	ThreatLocation = Location + FMath::VRand() * Dist * 0.3f;
	if (Dist < AudibleRangeCm * 0.4f)
	{
		Awareness = FMath::Max(Awareness, Species->FleeThreshold);
		ReactToThreat();
	}
	else
	{
		Awareness = FMath::Max(Awareness, Species->AlertThreshold + 0.1f);
	}
}

void AWildAnimal::ReceiveHerdAlarm(const FVector& InThreatLocation)
{
	if (IsDead() || State == EAnimalState::Fleeing || State == EAnimalState::Charging)
	{
		return;
	}
	ThreatLocation = InThreatLocation;
	Awareness = FMath::Max(Awareness, Species->FleeThreshold);
	ReactToThreat();
}

// ------------------------------------------------------------------
// 行動
// ------------------------------------------------------------------

void AWildAnimal::SetState(EAnimalState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.f;

	if (State == EAnimalState::Alert || State == EAnimalState::Bedded || State == EAnimalState::Grazing)
	{
		StopMoving();
	}
	if (State == EAnimalState::Alert)
	{
		OnAlarmCall();
	}
	if (State == EAnimalState::Bedded && IsWounded())
	{
		SpawnTrackSign(BedClass, ETrackSignType::Bed, 1.f);
	}
	if (State == EAnimalState::Grazing)
	{
		NextDecisionTime = FMath::FRandRange(5.f, 20.f);
	}
	OnStateChanged.Broadcast(State);
}

void AWildAnimal::UpdateBehavior(float Dt)
{
	StateTime += Dt;
	const bool bThreatened = Awareness >= Species->FleeThreshold;
	const bool bSuspicious = Awareness >= Species->AlertThreshold;

	switch (State)
	{
	case EAnimalState::Grazing:
	case EAnimalState::Wandering:
		if (bThreatened)
		{
			ReactToThreat();
		}
		else if (bSuspicious)
		{
			SetState(EAnimalState::Alert);
		}
		else if (State == EAnimalState::Grazing && StateTime > NextDecisionTime)
		{
			StartWander();
		}
		else if (State == EAnimalState::Wandering && IsMoveFinished())
		{
			SetState(EAnimalState::Grazing);
		}
		break;

	case EAnimalState::Alert:
		// 頭を上げて怪しい方向を凝視する。ここで動くと見つかる
		FaceLocation(ThreatLocation, Dt);
		if (bThreatened)
		{
			ReactToThreat();
		}
		else if (Awareness < Species->AlertThreshold * 0.5f && StateTime > 4.f)
		{
			SetState(EAnimalState::Grazing);
		}
		break;

	case EAnimalState::Fleeing:
		if (IsMoveFinished())
		{
			if (Awareness >= Species->AlertThreshold)
			{
				FleeFrom(ThreatLocation);
			}
			else
			{
				// 手負いの獣は逃げた先で伏せて様子をうかがう
				SetState(IsWounded() ? EAnimalState::Bedded : EAnimalState::Alert);
			}
		}
		break;

	case EAnimalState::Charging:
		UpdateCharge(Dt);
		break;

	case EAnimalState::Bedded:
		if (bIncapacitated)
		{
			break;
		}
		// 追跡を急いで近づくと再び飛び出す
		if (bThreatened)
		{
			ReactToThreat();
		}
		else if (!IsWounded() && StateTime > 30.f)
		{
			SetState(EAnimalState::Grazing);
		}
		break;

	default:
		break;
	}
}

void AWildAnimal::ReactToThreat()
{
	if (IsDead() || bIncapacitated)
	{
		return;
	}

	const AHunterCharacter* Hunter = GetHunter();
	const float HunterDist = Hunter ? FVector::Dist(Hunter->GetActorLocation(), GetActorLocation()) : BIG_NUMBER;
	const bool bHunterClose = HunterDist < Species->ChargeTriggerDistance && Species->AttackDamage > 0.f;

	switch (Species->Temperament)
	{
	case EAnimalTemperament::Aggressive:
		if (bHunterClose)
		{
			StartCharge();
			return;
		}
		break;
	case EAnimalTemperament::Defensive:
		if (bHunterClose && (IsWounded() || HunterDist < Species->ChargeTriggerDistance * 0.4f))
		{
			StartCharge();
			return;
		}
		break;
	default:
		break;
	}

	if (State != EAnimalState::Fleeing)
	{
		FleeFrom(ThreatLocation);
	}
}

void AWildAnimal::FleeFrom(const FVector& From)
{
	FVector Away = (GetActorLocation() - From).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FMath::VRand().GetSafeNormal2D();
	}
	Away = Away.RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);

	const float Distance = FMath::FRandRange(Species->FleeDistance * 0.6f, Species->FleeDistance);
	MoveTo(GetActorLocation() + Away * Distance);

	const bool bWasFleeing = State == EAnimalState::Fleeing;
	SetState(EAnimalState::Fleeing);
	if (!bWasFleeing)
	{
		OnAlarmCall();
		AlarmHerd();
	}
}

void AWildAnimal::AlarmHerd()
{
	for (TActorIterator<AWildAnimal> It(GetWorld()); It; ++It)
	{
		AWildAnimal* Other = *It;
		if (Other != this && Other->Species == Species
			&& FVector::Dist(Other->GetActorLocation(), GetActorLocation()) < Species->HerdAlarmRadius)
		{
			Other->ReceiveHerdAlarm(ThreatLocation);
		}
	}
}

void AWildAnimal::StartCharge()
{
	AHunterCharacter* Hunter = GetHunter();
	AAIController* AI = Cast<AAIController>(GetController());
	if (!Hunter || !AI)
	{
		FleeFrom(ThreatLocation);
		return;
	}
	SetState(EAnimalState::Charging);
	AI->MoveToActor(Hunter, Species->AttackRange * 0.5f);
	Hunter->NotifyThreatened();
}

void AWildAnimal::UpdateCharge(float Dt)
{
	AHunterCharacter* Hunter = GetHunter();
	if (!Hunter || Hunter->IsDead())
	{
		SetState(EAnimalState::Alert);
		return;
	}

	const float Dist = FVector::Dist(Hunter->GetActorLocation(), GetActorLocation());
	if (Dist <= Species->AttackRange && AttackCooldown <= 0.f)
	{
		AttackCooldown = 1.5f;
		UGameplayStatics::ApplyDamage(Hunter, Species->AttackDamage, GetController(), this, UDamageType::StaticClass());
		OnAttack();
	}

	if (IsMoveFinished() && Dist > Species->AttackRange)
	{
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->MoveToActor(Hunter, Species->AttackRange * 0.5f);
		}
	}

	// 深手を負った防御型、または見失った場合は退く
	const bool bBadlyHurt = Health < MaxHealthCached * 0.25f && Species->Temperament == EAnimalTemperament::Defensive;
	if (bBadlyHurt || StateTime > 20.f || Awareness < Species->AlertThreshold * 0.5f)
	{
		FleeFrom(Hunter->GetActorLocation());
	}
}

void AWildAnimal::StartWander()
{
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Out;
		if (Nav->GetRandomReachablePointInRadius(HomeLocation, Species->WanderRadius, Out))
		{
			MoveTo(Out.Location);
			SetState(EAnimalState::Wandering);
			return;
		}
	}
	StateTime = 0.f;
}

void AWildAnimal::FaceLocation(const FVector& Target, float Dt)
{
	const FVector To = (Target - GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return;
	}
	const FRotator Desired(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Desired, Dt, 120.f));
}

void AWildAnimal::UpdateLocomotion(float Dt)
{
	float TargetSpeed = Species->WalkSpeed;
	bool bExerting = false;
	switch (State)
	{
	case EAnimalState::Fleeing:
		TargetSpeed = Species->RunSpeed;
		bExerting = true;
		break;
	case EAnimalState::Charging:
		TargetSpeed = Species->ChargeSpeed;
		bExerting = true;
		break;
	default:
		break;
	}

	Stamina = bExerting
		? FMath::Max(0.f, Stamina - Dt)
		: FMath::Min(Species->MaxStamina, Stamina + Dt * 0.5f);

	const float Fatigue = Stamina > 0.f ? 1.f : 0.45f;
	const float HealthFrac = Health / MaxHealthCached;
	const float Injury = HealthFrac < 0.4f ? FMath::Lerp(0.35f, 1.f, HealthFrac / 0.4f) : 1.f;

	GetCharacterMovement()->MaxWalkSpeed = TargetSpeed * Mobility * Fatigue * Injury;
}

float AWildAnimal::GetGaitAlpha() const
{
	return Species ? FMath::Clamp(GetVelocity().Size2D() / Species->RunSpeed, 0.f, 1.f) : 0.f;
}

// ------------------------------------------------------------------
// 移動ヘルパー
// ------------------------------------------------------------------

bool AWildAnimal::MoveTo(const FVector& Destination)
{
	AAIController* AI = Cast<AAIController>(GetController());
	if (!AI)
	{
		return false;
	}

	FVector Target = Destination;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Out;
		if (Nav->ProjectPointToNavigation(Destination, Out, FVector(2000.f, 2000.f, 2000.f)))
		{
			Target = Out.Location;
		}
		else if (Nav->GetRandomReachablePointInRadius(GetActorLocation(), 3000.f, Out))
		{
			Target = Out.Location;
		}
	}
	return AI->MoveToLocation(Target, 150.f) != EPathFollowingRequestResult::Failed;
}

bool AWildAnimal::IsMoveFinished() const
{
	const AAIController* AI = Cast<AAIController>(GetController());
	return !AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle;
}

void AWildAnimal::StopMoving()
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
}

AHunterCharacter* AWildAnimal::GetHunter() const
{
	return Cast<AHunterCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

// ------------------------------------------------------------------
// 被弾
// ------------------------------------------------------------------

void AWildAnimal::ApplyBallisticHit(const FHitResult& Hit, const FVector& Direction, float ImpactEnergyJ, const FAmmoSpec& Ammo, AActor* Shooter)
{
	if (IsDead())
	{
		// 死体を撃つと肉が傷むだけ
		MeatQuality = FMath::Max(0.f, MeatQuality - 0.05f);
		return;
	}

	ThreatLocation = Shooter ? Shooter->GetActorLocation() : Hit.TraceStart;

	const float MinEnergy = Ammo.bIsArrow ? Species->MinArrowEnergyJ : Species->MinBulletEnergyJ;
	const float Penetration = FMath::Clamp(ImpactEnergyJ / FMath::Max(MinEnergy, 1.f), 0.f, 1.5f);

	// エネルギー不足: 骨で止まる・浅く刺さるだけ
	if (Penetration < 0.35f)
	{
		Health -= MaxHealthCached * 0.03f;
		BleedRate += MaxHealthCached * 0.001f;
		MeatQuality = FMath::Max(0.f, MeatQuality - 0.05f);
		UE_LOG(LogHunting, Log, TEXT("%s: shallow hit (%.0f J, penetration %.2f)"), *GetName(), ImpactEnergyJ, Penetration);
		OnHitReaction(EHitZone::Muscle);
		Awareness = Species->FleeThreshold * 1.2f;
		ReactToThreat();
		return;
	}

	// 弾道が体内を進む範囲で臓器を判定 (斜めからの射撃では複数臓器を貫く)
	const float DepthCm = Species->BodyDepthCm * FMath::Min(Penetration, 1.f);
	TArray<EHitZone> Zones = TraceWoundChannel(Hit.ImpactPoint, Direction, DepthCm);
	if (Zones.Num() == 0)
	{
		Zones.Add(ResolveBoneZone(Hit.BoneName));
	}

	const float Wound = FMath::Min(Penetration, 1.2f) * Ammo.WoundFactor;
	bool bKill = false;
	bool bIncap = false;
	FString ZoneNames;
	for (EHitZone Zone : Zones)
	{
		const FHitZoneEffect* Found = Species->ZoneEffects.Find(Zone);
		const FHitZoneEffect Effect = Found ? *Found : FHitZoneEffect();

		Health -= MaxHealthCached * Effect.ImmediateDamageFraction * Wound;
		BleedRate += MaxHealthCached * Effect.BleedFractionPerSecond * Wound;
		Mobility = FMath::Max(0.15f, Mobility - Effect.MobilityLoss);
		MeatQuality = FMath::Max(0.f, MeatQuality - Effect.MeatSpoilage * (Ammo.bIsArrow ? 0.5f : 1.f));
		if (BloodSignSeverity(Effect.BloodSign) > BloodSignSeverity(CurrentBloodSign))
		{
			CurrentBloodSign = Effect.BloodSign;
		}
		bKill |= Effect.bInstantKill;
		bIncap |= Effect.bIncapacitates;
		ZoneNames += UEnum::GetValueAsString(Zone) + TEXT(" ");
		OnHitReaction(Zone);
	}

	// 部位はプレイヤーに直接は伝えない (血痕から読み取る)
	UE_LOG(LogHunting, Log, TEXT("%s hit: %s| %.0f J | health %.1f | bleed %.2f/s"),
		*GetName(), *ZoneNames, ImpactEnergyJ, Health, BleedRate);

	SpawnTrackSign(BloodClass, ETrackSignType::Blood, 1.5f);

	if (bKill || Health <= 0.f)
	{
		Die();
		return;
	}
	if (bIncap)
	{
		bIncapacitated = true;
		Mobility = 0.f;
		SetState(EAnimalState::Bedded);
		return;
	}

	Awareness = Species->FleeThreshold * 1.2f;
	ReactToThreat();
}

TArray<EHitZone> AWildAnimal::TraceWoundChannel(const FVector& Entry, const FVector& Direction, float DepthCm) const
{
	TArray<EHitZone> Result;
	const FVector Exit = Entry + Direction.GetSafeNormal() * DepthCm;
	for (const FVitalOrgan& Organ : Species->VitalOrgans)
	{
		const FVector Center = GetOrganWorldLocation(Organ);
		if (FMath::PointDistToSegment(Center, Entry, Exit) <= Organ.RadiusCm)
		{
			Result.AddUnique(Organ.Zone);
		}
	}
	return Result;
}

FVector AWildAnimal::GetOrganWorldLocation(const FVitalOrgan& Organ) const
{
	if (!Organ.Bone.IsNone() && GetMesh()->GetSkeletalMeshAsset() && GetMesh()->DoesSocketExist(Organ.Bone))
	{
		return GetMesh()->GetSocketTransform(Organ.Bone).TransformPosition(Organ.Offset);
	}
	return GetActorTransform().TransformPosition(Organ.Offset);
}

EHitZone AWildAnimal::ResolveBoneZone(FName BoneName) const
{
	FName Bone = BoneName;
	for (int32 Guard = 0; !Bone.IsNone() && Guard < 32; ++Guard)
	{
		if (const EHitZone* Zone = Species->BoneToZone.Find(Bone))
		{
			return *Zone;
		}

		// 一般的な四足獣リグの命名から推測
		const FString Name = Bone.ToString().ToLower();
		if (Name.Contains(TEXT("leg")) || Name.Contains(TEXT("thigh")) || Name.Contains(TEXT("calf"))
			|| Name.Contains(TEXT("foot")) || Name.Contains(TEXT("hoof")))
		{
			return EHitZone::Leg;
		}
		if (Name.Contains(TEXT("neck")))
		{
			return EHitZone::Neck;
		}
		Bone = GetMesh()->GetParentBone(Bone);
	}
	return EHitZone::Muscle;
}

void AWildAnimal::Die()
{
	if (IsDead())
	{
		return;
	}

	StopMoving();
	State = EAnimalState::Dead;
	BleedRate = 0.f;
	Health = 0.f;
	OnStateChanged.Broadcast(State);

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetMesh()->GetSkeletalMeshAsset())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		GetMesh()->SetSimulatePhysics(true);
	}
	else
	{
		// 仮ボックスは横倒しにする
		SetActorRotation(GetActorRotation() + FRotator(0.f, 0.f, 80.f));
		SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, 45.f));
	}

	UE_LOG(LogHunting, Log, TEXT("%s died. Meat quality %.0f%%"), *GetName(), MeatQuality * 100.f);
	OnDied();
}

// ------------------------------------------------------------------
// 痕跡・解体
// ------------------------------------------------------------------

void AWildAnimal::UpdateTracks()
{
	if (!FootprintClass)
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	if (FVector::Dist2D(Loc, LastFootprintLocation) < FootprintSpacing)
	{
		return;
	}
	LastFootprintLocation = Loc;

	if (const AHunterCharacter* Hunter = GetHunter())
	{
		if (FVector::Dist(Hunter->GetActorLocation(), Loc) > FootprintSpawnRadius)
		{
			return;
		}
	}
	SpawnTrackSign(FootprintClass, ETrackSignType::Footprint, 1.f);
}

void AWildAnimal::SpawnTrackSign(TSubclassOf<ATrackSign> SignClass, ETrackSignType Type, float Intensity)
{
	if (!SignClass)
	{
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 200.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrackSign), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return;
	}

	// デカールの X 軸を地面に向け、Z 軸を進行方向 (血痕はランダム) に合わせる
	const FVector Up = Type == ETrackSignType::Blood ? FMath::VRand() : GetActorForwardVector();
	const FRotator Rot = FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal, Up).Rotator();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATrackSign* Sign = GetWorld()->SpawnActor<ATrackSign>(SignClass, Hit.ImpactPoint, Rot, SpawnParams))
	{
		const bool bRunning = GetVelocity().Size2D() > Species->WalkSpeed * 2.f;
		Sign->InitSign(Type, CurrentBloodSign, Species->DisplayName, Intensity, GetActorRotation().Yaw, bRunning);
	}
}

float AWildAnimal::GetHarvestSeconds() const
{
	return Species ? Species->HarvestSeconds : 5.f;
}

FText AWildAnimal::GetSpeciesName() const
{
	return Species ? Species->DisplayName : FText::FromString(TEXT("Animal"));
}

FString AWildAnimal::Harvest(UInventoryComponent* Inventory)
{
	if (!CanBeHarvested() || !Inventory)
	{
		return FString();
	}
	bHarvested = true;

	TArray<FString> Parts;
	for (const FHarvestYield& Yield : Species->Yields)
	{
		const float Scale = Yield.bScaledByMeatQuality ? MeatQuality : 1.f;
		const int32 Amount = FMath::RoundToInt(Yield.BaseAmount * Scale);
		if (Amount > 0)
		{
			Inventory->AddItem(Yield.ItemId, Amount);
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Yield.ItemId.ToString(), Amount));
		}
	}

	SetLifeSpan(60.f);
	return FString::Printf(TEXT("Field dressed the %s (meat quality %d%%): %s"),
		*GetSpeciesName().ToString(), FMath::RoundToInt(MeatQuality * 100.f), *FString::Join(Parts, TEXT(", ")));
}
