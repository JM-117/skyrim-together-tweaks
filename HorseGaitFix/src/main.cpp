// HorseGaitFix: fuerza la variable "Speed" del grafo de caballos que se mueven sin animar
// (copias remotas de STR). No identifica caballos de STR: si Speed ya es >0, no toca nada.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace
{
    struct Sample { RE::NiPoint3 pos; std::chrono::steady_clock::time_point t; float smoothed{0.f}; };
    std::unordered_map<RE::FormID, Sample> g_samples;

    constexpr float kMinSpeed = 20.f;   // u/s, bajo esto se considera parado
    constexpr float kMaxSpeed = 1500.f; // u/s, sobre esto es un teletransporte, se ignora
    constexpr float kHoldFrac = 0.8f;   // histéresis simple: suaviza para evitar parpadeo trote/galope

    void Tick()
    {
        auto* lists = RE::ProcessLists::GetSingleton();
        if (!lists) return;
        const auto now = std::chrono::steady_clock::now();
        for (auto& handle : lists->highActorHandles)
        {
            auto actor = handle.get();
            if (!actor || actor->IsPlayerRef() || actor->IsDead() || !actor->Is3DLoaded()) continue;
            // shortcut: heurística por keyword animal + variable Speed presente; afinar con la raza del caballo si da falsos positivos
            if (!actor->HasKeywordString("ActorTypeAnimal")) continue;
            float graphSpeed = 0.f;
            if (!actor->GetGraphVariableFloat("Speed", graphSpeed)) continue;

            auto& s = g_samples[actor->GetFormID()];
            const auto pos = actor->GetPosition();
            const float dt = std::chrono::duration<float>(now - s.t).count();
            if (s.t.time_since_epoch().count() != 0 && dt > 0.01f)
            {
                const float v = pos.GetDistance(s.pos) / dt;
                if (v < kMaxSpeed) s.smoothed = s.smoothed * kHoldFrac + v * (1.f - kHoldFrac);
            }
            s.pos = pos; s.t = now;
            if (graphSpeed < 1.f && s.smoothed > kMinSpeed) actor->SetGraphVariableFloat("Speed", s.smoothed);
        }
    }

    void Loop()
    {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            SKSE::GetTaskInterface()->AddTask(Tick);
        }
    }
}

SKSEPluginInfo(
    .Version = {0, 1, 0},
    .Name = "HorseGaitFix",
    .RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(SKSE::VersionIndependence::AddressLibrary))

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* m) {
        if (m->type == SKSE::MessagingInterface::kDataLoaded) std::thread(Loop).detach();
    });
    return true;
}
