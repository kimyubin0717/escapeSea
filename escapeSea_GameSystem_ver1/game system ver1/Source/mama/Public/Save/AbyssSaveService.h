#pragma once
#include "CoreMinimal.h"
class UObject;
/** Owns save format, validation and disk IO. No UI or browser lifecycle. */
class FAbyssSaveService
{
public:
    static FString SavePath(const UObject* Context);
    static bool HasSave(const UObject* Context);
    static bool SaveRun(const UObject* Context);
    static bool LoadRun(const UObject* Context);
};
