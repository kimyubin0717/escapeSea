#include "mama.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Misc/CoreDelegates.h"
#include "ShaderCompiler.h"
#endif

class FMamaModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
    FDelegateHandle ExitHandle;

    void StartupModule() override
    {
        ExitHandle = FCoreDelegates::OnEnginePreExit.AddLambda([]
        {
            // Editor and standalone preview share the shader compiler until engine teardown.
            if (GShaderCompilingManager)
                GShaderCompilingManager->FinishAllCompilation();
        });
    }

    void ShutdownModule() override
    {
        FCoreDelegates::OnEnginePreExit.Remove(ExitHandle);
    }
#endif
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMamaModule, mama, "mama");
