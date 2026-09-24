#include "Weapons/HuntingWeapon.h"
#include "Animals/WildAnimal.h"
#include "Player/HunterCharacter.h"
#include "Weapons/BallisticProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AHuntingWeapon::AHuntingWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileClass = ABallisticProjectile::StaticClass();
}

void AHuntingWeapon::BeginPlay()
{
	Super::BeginPlay();
	Loaded = Kind == EWeaponKind::Bow ? 1 : MagazineCapacity;
}

void AHuntingWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (BusyTimeRemaining > 0.f)
	{
		BusyTimeRemaining -= DeltaSeconds;
		if (BusyTimeRemaining <= 0.f)
		{
			FinishBusy();
		}
	}

	if (bDrawing)
	{
		DrawFraction = FMath::Min(1.f, DrawFraction + DeltaSeconds / FullDrawTime);
		if (DrawFraction >= 1.f)
		{
			HoldTime += DeltaSeconds;
		}
	}
}

void AHuntingWeapon::PressTrigger()
{
	AHunterCharacter* Hunter = Cast<AHunterCharacter>(GetOwner());
	if (IsBusy() || !Hunter)
	{
		return;
	}

	if (Loaded <= 0)
	{
		if (DryFireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, GetActorLocation());
		}
		Hunter->PushMessage(Kind == EWeaponKind::Rifle ? TEXT("Click. Magazine empty - reload (R).") : TEXT("Quiver empty."), 2.f);
		return;
	}

	if (Kind == EWeaponKind::Rifle)
	{
		FireShot(1.f);
		--Loaded;
		if (Loaded > 0)
		{
			StartBusy(EBusyReason::Cycling, CycleTime);
		}
	}
	else
	{
		bDrawing = true;
		DrawFraction = 0.f;
		HoldTime = 0.f;
	}
}

void AHuntingWeapon::ReleaseTrigger()
{
	if (Kind != EWeaponKind::Bow || !bDrawing)
	{
		return;
	}
	bDrawing = false;

	// 引きが浅いと失速する (蓄えられるエネルギーは引き量に比例)
	if (DrawFraction < 0.3f)
	{
		return;
	}
	FireShot(FMath::Sqrt(DrawFraction));
	Loaded = 0;
	if (ReserveAmmo > 0)
	{
		StartBusy(EBusyReason::Nocking, ReloadTime);
	}
}

void AHuntingWeapon::Reload()
{
	if (IsBusy() || bDrawing || ReserveAmmo <= 0)
	{
		return;
	}
	if (Kind == EWeaponKind::Rifle && Loaded < MagazineCapacity)
	{
		StartBusy(EBusyReason::Reloading, ReloadTime);
	}
	else if (Kind == EWeaponKind::Bow && Loaded == 0)
	{
		StartBusy(EBusyReason::Nocking, ReloadTime);
	}
}

void AHuntingWeapon::StartBusy(EBusyReason Reason, float Time)
{
	BusyReason = Reason;
	BusyTimeRemaining = Time;
	if (Reason == EBusyReason::Cycling && CycleSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CycleSound, GetActorLocation());
	}
}

void AHuntingWeapon::FinishBusy()
{
	BusyTimeRemaining = 0.f;
	switch (BusyReason)
	{
	case EBusyReason::Reloading:
	{
		const int32 Needed = FMath::Min(MagazineCapacity - Loaded, ReserveAmmo);
		Loaded += Needed;
		ReserveAmmo -= Needed;
		break;
	}
	case EBusyReason::Nocking:
		if (ReserveAmmo > 0)
		{
			Loaded = 1;
			--ReserveAmmo;
		}
		break;
	default:
		break;
	}
	BusyReason = EBusyReason::None;
}

void AHuntingWeapon::FireShot(float VelocityScale)
{
	AHunterCharacter* Hunter = Cast<AHunterCharacter>(GetOwner());
	if (!Hunter || !ProjectileClass)
	{
		return;
	}

	// ゼロイン: 照準線と弾道がゼロイン距離で交わるよう、銃身をわずかに上に向ける。
	// 照準器は固定なので、弓を引き切らないと矢は下に落ちる。
	FRotator AimRot = Hunter->GetAimRotation();
	const float FlightTime = ZeroDistanceM / FMath::Max(Ammo.MuzzleVelocityMS, 1.f);
	const float DropM = 0.5f * 9.81f * FlightTime * FlightTime;
	AimRot.Pitch += FMath::RadiansToDegrees(FMath::Atan2(DropM, ZeroDistanceM));

	float SpreadDeg = AccuracyMOA / 60.f;
	if (!Hunter->IsAiming())
	{
		SpreadDeg += HipSpreadDeg;
	}
	const FVector Dir = FMath::VRandCone(AimRot.Vector(), FMath::DegreesToRadians(SpreadDeg * 0.5f));
	const FVector Start = Hunter->GetAimLocation() + Dir * 30.f;
	const float SpeedMS = Ammo.MuzzleVelocityMS * VelocityScale;

	FActorSpawnParameters Params;
	Params.Owner = Hunter;
	Params.Instigator = Hunter;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABallisticProjectile* Projectile = GetWorld()->SpawnActor<ABallisticProjectile>(ProjectileClass, Start, Dir.Rotation(), Params))
	{
		Projectile->Launch(Ammo, Dir * SpeedMS * 100.f, Hunter);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// 銃声 (弓なら弦音) は周囲の動物に届く
	for (TActorIterator<AWildAnimal> It(GetWorld()); It; ++It)
	{
		It->HearLoudNoise(Start, AudibleRangeCm);
	}

	Hunter->OnWeaponFired(RecoilPitchDeg);
}

float AHuntingWeapon::GetSwayMultiplier() const
{
	if (Kind != EWeaponKind::Bow || !bDrawing)
	{
		return 1.f;
	}
	// 引いている最中は揺れ、保持が長引くと筋肉が疲れて震える
	const float Straining = DrawFraction < 1.f ? 1.6f : 1.f;
	const float Fatigue = FMath::Max(0.f, HoldTime - ComfortHoldTime) * 0.5f;
	return Straining + Fatigue;
}

FString AHuntingWeapon::GetStatusText() const
{
	FString Status;
	switch (BusyReason)
	{
	case EBusyReason::Cycling:   Status = TEXT(" | cycling bolt"); break;
	case EBusyReason::Reloading: Status = TEXT(" | reloading"); break;
	case EBusyReason::Nocking:   Status = TEXT(" | nocking arrow"); break;
	default: break;
	}
	if (bDrawing)
	{
		Status = FString::Printf(TEXT(" | draw %d%%%s"), FMath::RoundToInt(DrawFraction * 100.f),
			HoldTime > ComfortHoldTime ? TEXT(" (arms shaking)") : TEXT(""));
	}
	return FString::Printf(TEXT("%s  [%s]  %d / %d%s"),
		*DisplayName.ToString(), *Ammo.Name.ToString(), Loaded, ReserveAmmo, *Status);
}

AHuntingBow::AHuntingBow()
{
	Kind = EWeaponKind::Bow;
	DisplayName = NSLOCTEXT("Hunting", "Bow", "Compound bow 70lb");
	Ammo.Name = TEXT("Carbon arrow 420gr");
	Ammo.MassGrams = 27.2f;
	Ammo.MuzzleVelocityMS = 88.f;
	Ammo.DiameterMM = 8.f;
	Ammo.DragCoefficient = 1.5f;
	Ammo.WoundFactor = 1.1f;
	Ammo.bIsArrow = true;
	MagazineCapacity = 1;
	ReserveAmmo = 11;
	ReloadTime = 2.f;
	AccuracyMOA = 6.f;
	HipSpreadDeg = 5.f;
	ZeroDistanceM = 20.f;
	AudibleRangeCm = 2500.f; // 近距離なら弦音で「矢をかわす」
	RecoilPitchDeg = 0.3f;
	AimFOV = 60.f;
}
