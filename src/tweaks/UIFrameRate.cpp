#include "ScopedMemoryWrite.h"
#include "Settings.h"
#include "Tweaks.h"

// TiltedUI hardcodes CefBrowserSettings::windowless_frame_rate = 60 (OverlayApp.cpp). CEF 141 no longer clamps
// this value to 60, so the UI can be allowed to paint faster on high refresh rate monitors.
void UIFrameRate::Install()
{
    const uint32_t frameRate = settings::uUiFrameRate.GetValue();
    if (frameRate == 60 || frameRate == 0)
    {
        logger::info("uUiFrameRate is {}, keeping Skyrim Together's default UI frame rate", frameRate);
        return;
    }

    // mov qword ptr [rbp+...], 108h (sizeof(CefBrowserSettings)); mov dword ptr [rbp+...], 3Ch (windowless_frame_rate)
    const uintptr_t address = memscan::FindPatternInST(memscan::aob::OverlayApp_Initialize_windowlessFrameRate);
    if (!address)
    {
        logger::warn("UI frame rate: pattern not found, skipping");
        return;
    }

    auto* value = reinterpret_cast<uint32_t*>(address + 0x11);
    ScopedMemoryWrite _{ value, sizeof(uint32_t) };
    *value = (std::min)(frameRate, 240u);

    logger::info("installed UI frame rate tweak: {} fps", *value);
}
