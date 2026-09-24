#include "Weapons/BallisticProjectile.h"
#include "Animals/WildAnimal.h"
#include "Environment/WindSubsystem.h"
#include "Player/HunterCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ABallisticProjectile::ABallisticProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	InitialLifeSpan = 8.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABallisticProjectile::Launch(const FAmmoSpec& InAmmo, const FVector& InVelocityCmS, AActor* InShooter)
{
	Ammo = InAmmo;
	VelocityMS = InVelocityCmS / 100.f;
	Shooter = InShooter;
	bInFlight = true;
}

void ABallisticProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInFlight)
	{
		return;
	}

	UWorld* World = GetWorld();
	const UWindSubsystem* Wind = World->GetSubsystem<UWindSubsystem>();
	const FVector WindMS = Wind ? Wind->GetWindVelocityMS() : FVector::ZeroVector;
	const float Mass = Ammo.GetMassKg();
	const float DragK = 0.5f * AirDensity * Ammo.DragCoefficient * Ammo.GetCrossSectionM2() / Mass;

	// 高速弾でも壁抜けしないよう 4ms 刻みで積分
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / 0.004f), 1, 16);
	const float H = DeltaSeconds / Steps;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Ballistic), true, this);
	Params.bReturnPhysicalMaterial = true;
	if (AActor* ShooterActor = Shooter.Get())
	{
		Params.AddIgnoredActor(ShooterActor);
	}

	for (int32 i = 0; i < Steps; ++i)
	{
		// 空気抵抗は対気速度 (弾速 - 風速) の二乗に比例
		const FVector Relative = VelocityMS - WindMS;
		const FVector Drag = -DragK * Relative.Size() * Relative;
		VelocityMS += (Drag + FVector(0.f, 0.f, -9.81f)) * H;

		const FVector Start = GetActorLocation();
		const FVector End = Start + VelocityMS * 100.f * H;

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			HandleImpact(Hit);
			return;
		}

		SetActorLocationAndRotation(End, VelocityMS.Rotation());
		TraveledCm += (End - Start).Size();
	}

	if (TraveledCm > MaxRangeCm || VelocityMS.SizeSquared() < 1.f)
	{
		Destroy();
	}
}

void ABallisticProjectile::HandleImpact(const FHitResult& Hit)
{
	const float Speed = VelocityMS.Size();
	const float Energy = Ammo.GetEnergyJ(Speed);
	const FVector Dir = VelocityMS.GetSafeNormal();
	AActor* ShooterActor = Shooter.Get();

	if (AWildAnimal* Animal = Cast<AWildAnimal>(Hit.GetActor()))
	{
		Animal->ApplyBallisticHit(Hit, Dir, Energy, Ammo, ShooterActor);

		// 命中音 (「ボスッ」) は実際の猟でも聞こえる手がかり
		if (AHunterCharacter* Hunter = Cast<AHunterCharacter>(ShooterActor))
		{
			Hunter->PushMessage(TEXT("*thwack* - that sounded like a hit."), 3.f);
		}
	}
	else if (AActor* Other = Hit.GetActor())
	{
		AController* InstigatorController = ShooterActor ? ShooterActor->GetInstigatorController() : nullptr;
		UGameplayStatics::ApplyPointDamage(Other, Energy * 0.02f, Dir, Hit, InstigatorController, this, UDamageType::StaticClass());
	}

	OnImpact(Hit, Energy);

	if (Ammo.bIsArrow && Hit.GetComponent())
	{
		// 矢は刺さったまま残る
		bInFlight = false;
		SetActorLocationAndRotation(Hit.ImpactPoint - Dir * 20.f, Dir.Rotation());
		AttachToComponent(Hit.GetComponent(), FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
		SetLifeSpan(60.f);
	}
	else
	{
		Destroy();
	}
}
