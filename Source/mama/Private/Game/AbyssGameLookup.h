#pragma once

#include "Game/AbyssGameMode.h"
#include "Kismet/GameplayStatics.h"

namespace Abyss {
inline AAbyssGameMode* Game(const UObject* Context) { return Cast<AAbyssGameMode>(UGameplayStatics::GetGameMode(Context)); }
}
