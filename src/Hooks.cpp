#include "Hooks.h"
#include "tweaks/Tweaks.h"

namespace hooks
{
    void InstallPreloadHooks()
    {
        UIActivationFix::Install();
        UIRenderFix::Install();
        MH_EnableHook(MH_ALL_HOOKS);
    }

    void Install()
    {
        AppendVersionInCrashLogs::Install(); // Doesn't do anything as of now
        ChangeUIActivationKeys::Install();
        ChangeDebugMenuKey::Install();
        InterpolationBufferMs::Install();
        NoAnimationDelay::Install();
    }
}
