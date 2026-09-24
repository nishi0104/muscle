#include "Core/HuntingGameMode.h"
#include "Core/HuntingHUD.h"
#include "Player/HunterCharacter.h"

AHuntingGameMode::AHuntingGameMode()
{
	DefaultPawnClass = AHunterCharacter::StaticClass();
	HUDClass = AHuntingHUD::StaticClass();
}
