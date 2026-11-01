// HorseGaitFix: fuerza la variable "Speed" del grafo de caballos que se mueven sin animar
// (copias remotas de STR). No identifica caballos de STR: si Speed ya es >0, no toca nada.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <chrono>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    struct Sample { RE::NiPoint3 pos; std::chrono::steady_clock::time_point t; float smoothed{0.f}; };
    std::unordered_map<RE::FormID, Sample> g_samples;

    std::unordered_set<RE::FormID> g_logged; // un log por actor y motivo (FormID | bit 63 = rechazado)
    constexpr float kMinSpeed = 20.f;   // u/s, bajo esto se considera parado
    constexpr float kMaxSpeed = 600.f;  // u/s, sobre esto es un salto de posición de STR, se ignora
    constexpr float kSprintSpeed = 450.f; // u/s, shortcut: valor adivinado para pasar de trote a galope; ajustar con el log
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
            const auto* race = actor->GetRace();
            const std::string_view raceId = race ? race->GetFormEditorID() : "";
            const bool animal = raceId == "HorseRace";
            float graphSpeed = 0.f;
            const bool hasSpeed = actor->GetGraphVariableFloat("Speed", graphSpeed);
            if (!animal || !hasSpeed)
            {
                // diagnóstico: actores que se mueven sin animar y el filtro descarta
                const auto it = g_samples.find(actor->GetFormID());
                if (it != g_samples.end() && it->second.smoothed > kMinSpeed && graphSpeed < 1.f && g_logged.insert(actor->GetFormID() | 0x80000000u).second)
                    spdlog::info("REJECTED {:08X} race={} animal={} hasSpeedVar={} graphSpeed={:.1f} v={:.1f}", actor->GetFormID(),
                        actor->GetRace() ? actor->GetRace()->GetFormEditorID() : "?", animal, hasSpeed, graphSpeed, it->second.smoothed);
                auto& r = g_samples[actor->GetFormID()];
                const auto p = actor->GetPosition();
                const float d = std::chrono::duration<float>(now - r.t).count();
                if (r.t.time_since_epoch().count() != 0 && d > 0.01f && p.GetDistance(r.pos) / d < kMaxSpeed)
                    r.smoothed = r.smoothed * kHoldFrac + (p.GetDistance(r.pos) / d) * (1.f - kHoldFrac);
                r.pos = p; r.t = now;
                continue;
            }

            auto& s = g_samples[actor->GetFormID()];
            const auto pos = actor->GetPosition();
            const float dt = std::chrono::duration<float>(now - s.t).count();
            if (s.t.time_since_epoch().count() != 0 && dt > 0.01f)
            {
                const float v = pos.GetDistance(s.pos) / dt;
                if (v < kMaxSpeed) s.smoothed = s.smoothed * kHoldFrac + v * (1.f - kHoldFrac);
            }
            s.pos = pos; s.t = now;
            if (graphSpeed < 1.f && s.smoothed > kMinSpeed)
            {
                // variables que PlayerHub fuerza en caballos remotos (nombres tomados de su PDB)
                actor->SetGraphVariableFloat("Speed", s.smoothed);
                actor->SetGraphVariableFloat("HorseSpeedSampled", s.smoothed);
                actor->SetGraphVariableBool("IsMoving", true);
                const bool sprint = s.smoothed > kSprintSpeed;
                actor->SetGraphVariableBool("IsSprinting", sprint);
                actor->SetGraphVariableInt("iState_HorseSprint", sprint ? 1 : 0);
                if (g_logged.insert(actor->GetFormID()).second)
                    spdlog::info("FORCED {:08X} race={} graphSpeed={:.1f} v={:.1f}", actor->GetFormID(),
                        actor->GetRace() ? actor->GetRace()->GetFormEditorID() : "?", graphSpeed, s.smoothed);
            }
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
    if (auto dir = SKSE::log::log_directory())
    {
        auto log = std::make_shared<spdlog::logger>("HorseGaitFix", std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / ("HorseGaitFix_" + std::to_string(REX::W32::GetCurrentProcessId()) + ".log")).string(), true));
        spdlog::set_default_logger(log);
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("HorseGaitFix loaded");
    }
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* m) {
        if (m->type == SKSE::MessagingInterface::kDataLoaded) std::thread(Loop).detach();
    });
    return true;
}
