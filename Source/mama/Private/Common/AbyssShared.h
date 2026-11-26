#pragma once
#include "CoreMinimal.h"
namespace Abyss {
inline constexpr float MapScale=0.55f;
inline FVector Map(const FVector& P) { return FVector(P.X*MapScale,P.Y*MapScale,P.Z); }
inline const FLinearColor Teal(0.03f,0.52f,0.64f);
inline const FLinearColor Gold(0.95f,0.56f,0.12f);
inline const TCHAR* Items[]={TEXT("POWER CELL"),TEXT("HULL ALLOY"),TEXT("DRIVE PART"),TEXT("OXYGEN TANK")};
}
