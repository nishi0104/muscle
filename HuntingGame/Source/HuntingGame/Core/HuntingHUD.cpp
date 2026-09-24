#include "Core/HuntingHUD.h"
#include "Environment/WindSubsystem.h"
#include "Items/InventoryComponent.h"
#include "Player/HunterCharacter.h"
#include "Weapons/HuntingWeapon.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

void AHuntingHUD::DrawHUD()
{
	Super::DrawHUD();

	const AHunterCharacter* Hunter = Cast<AHunterCharacter>(GetOwningPawn());
	if (!Hunter || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const FLinearColor Text(0.9f, 0.9f, 0.85f, 0.9f);

	// ---- 左上: 身体の状態 ----
	float Y = 20.f;
	DrawText(TEXT("Health"), Text, 20.f, Y, Font);
	DrawBar(110.f, Y + 4.f, 160.f, 10.f, Hunter->GetHealth() / Hunter->GetMaxHealth(), FLinearColor(0.7f, 0.1f, 0.1f));
	Y += 22.f;
	DrawText(TEXT("Stamina"), Text, 20.f, Y, Font);
	DrawBar(110.f, Y + 4.f, 160.f, 10.f, Hunter->GetStamina() / Hunter->GetMaxStamina(), FLinearColor(0.8f, 0.7f, 0.2f));
	Y += 22.f;
	DrawText(FString::Printf(TEXT("Heart  %d bpm"), FMath::RoundToInt(Hunter->GetHeartRate())), Text, 20.f, Y, Font);
	Y += 22.f;
	static const TCHAR* StanceNames[] = { TEXT("Standing"), TEXT("Crouching"), TEXT("Prone") };
	DrawText(StanceNames[static_cast<int32>(Hunter->GetStance())], Text, 20.f, Y, Font);
	Y += 22.f;
	if (Hunter->IsAiming())
	{
		DrawText(Hunter->IsHoldingBreath() ? TEXT("Holding breath") : TEXT("Breath"), Text, 20.f, Y, Font);
		DrawBar(150.f, Y + 4.f, 120.f, 10.f, Hunter->GetBreathFraction(), FLinearColor(0.4f, 0.7f, 0.9f));
	}

	// ---- 右上: 風 ----
	DrawWind(W - 80.f, 80.f, Hunter->GetAimRotation().Yaw);

	// ---- 中央: 構えたときだけ細い照準線 (スコープの代わり) ----
	if (Hunter->IsAiming())
	{
		const float CX = W * 0.5f;
		const float CY = H * 0.5f;
		const FLinearColor Reticle(0.f, 0.f, 0.f, 0.85f);
		DrawLine(CX - 60.f, CY, CX - 6.f, CY, Reticle, 1.f);
		DrawLine(CX + 6.f, CY, CX + 60.f, CY, Reticle, 1.f);
		DrawLine(CX, CY + 6.f, CX, CY + 60.f, Reticle, 1.f);
		DrawLine(CX, CY - 60.f, CX, CY - 6.f, Reticle, 1.f);
	}

	// ---- 解体の進捗 ----
	const float Harvest = Hunter->GetHarvestProgress();
	if (Harvest >= 0.f)
	{
		DrawBar(W * 0.5f - 120.f, H * 0.6f, 240.f, 12.f, Harvest, FLinearColor(0.6f, 0.4f, 0.3f));
	}

	// ---- 右下: 武器 ----
	if (const AHuntingWeapon* Weapon = Hunter->GetWeapon())
	{
		const FString Status = Weapon->GetStatusText();
		float TW = 0.f, TH = 0.f;
		GetTextSize(Status, TW, TH, Font);
		DrawText(Status, Text, W - TW - 20.f, H - 40.f, Font);
	}

	// ---- 下中央: メッセージ ----
	float MY = H - 80.f;
	const TArray<FHuntMessage>& Messages = Hunter->GetMessages();
	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Messages[i].Text, TW, TH, Font);
		DrawText(Messages[i].Text, Text, (W - TW) * 0.5f, MY, Font);
		MY -= TH + 6.f;
	}

	// ---- 左下: 所持品 (Tab) ----
	if (Hunter->IsInventoryVisible())
	{
		float IY = H * 0.45f;
		DrawText(TEXT("Pack"), Text, 20.f, IY, Font);
		IY += 22.f;
		for (const TPair<FName, int32>& Item : Hunter->GetInventory()->GetItems())
		{
			DrawText(FString::Printf(TEXT("  %s  x%d"), *Item.Key.ToString(), Item.Value), Text, 20.f, IY, Font);
			IY += 20.f;
		}
	}
}

void AHuntingHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
	DrawRect(Color, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

void AHuntingHUD::DrawWind(float CenterX, float CenterY, float CameraYaw)
{
	const UWindSubsystem* Wind = GetWorld()->GetSubsystem<UWindSubsystem>();
	if (!Wind)
	{
		return;
	}

	// 画面上向き = 自分の正面方向へ風が吹いている (正面の獲物は風下 = 匂いが届く)
	const float Relative = FMath::DegreesToRadians(Wind->GetWindHeadingDeg() - CameraYaw);
	const FVector2D Dir(FMath::Sin(Relative), -FMath::Cos(Relative));
	const float Len = 40.f;
	const FVector2D Tip = FVector2D(CenterX, CenterY) + Dir * Len;
	const FVector2D Tail = FVector2D(CenterX, CenterY) - Dir * Len;
	const FVector2D Side(-Dir.Y, Dir.X);
	const FLinearColor Color(0.85f, 0.9f, 1.f, 0.9f);

	DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, Color, 2.f);
	const FVector2D Wing1 = Tip - Dir * 12.f + Side * 8.f;
	const FVector2D Wing2 = Tip - Dir * 12.f - Side * 8.f;
	DrawLine(Tip.X, Tip.Y, Wing1.X, Wing1.Y, Color, 2.f);
	DrawLine(Tip.X, Tip.Y, Wing2.X, Wing2.Y, Color, 2.f);

	DrawText(FString::Printf(TEXT("Wind %.1f m/s"), Wind->GetWindSpeedMS()), Color, CenterX - 50.f, CenterY + Len + 10.f, GEngine->GetSmallFont());
}
