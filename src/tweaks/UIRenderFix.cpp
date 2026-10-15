#include "Tweaks.h"

#include <cstring>
#include <mutex>
#include <vector>

#include <d3d11.h>

// Skyrim Together's overlay renderer (TiltedUI OverlayRenderHandlerD3D11) copies the whole CEF frame into a
// dynamic texture on every OnPaint, while holding the same mutex the game's render thread needs to draw.
// At 1440p that is ~14.7 MB per paint, and the game stalls until the copy finishes.
//
// This tweak keeps the overlay's own drawing code but feeds it a different texture:
//  - OnPaint (CEF thread) only copies the dirty rectangles into a CPU-side buffer.
//  - Render (game thread) uploads just the dirty region with UpdateSubresource, and never waits for CEF:
//    if CEF is writing, the previous frame is drawn instead.
namespace
{
    // OverlayRenderHandlerD3D11 layout in Skyrim Together 1.8.x (checked against the patterns below)
    constexpr size_t kCursorVisible = 0x11;  // true while the STR menu is open (InputService SetUIActive)
    constexpr size_t kWidth = 0x24;
    constexpr size_t kHeight = 0x28;
    constexpr size_t kTextureView = 0x40;
    constexpr size_t kDevice = 0xF8;
    constexpr size_t kImmediateContext = 0x108;

    constexpr int kPetView = 0;
    constexpr int kPetPopup = 1;

    template <class T>
    T& Member(void* a_self, size_t a_offset)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(a_self) + a_offset);
    }

    // CefRect and CefRenderHandler::RectList (std::vector<CefRect>) as laid out by MSVC
    struct CefRectRaw
    {
        int x, y, width, height;
    };

    struct RectListRaw
    {
        const CefRectRaw* first;
        const CefRectRaw* last;
        const CefRectRaw* end;
    };

    struct FrameState
    {
        std::mutex           lock;
        std::vector<uint8_t> pixels;
        int                  width = 0;
        int                  height = 0;
        bool                 dirty = false;
        int                  dirtyLeft = 0, dirtyTop = 0, dirtyRight = 0, dirtyBottom = 0;

        // Only touched on the game's render thread
        ID3D11Device*             device = nullptr;
        ID3D11Texture2D*          texture = nullptr;
        ID3D11ShaderResourceView* view = nullptr;
        int                       textureWidth = 0;
        int                       textureHeight = 0;
    };

    FrameState g_frame;

    using OnPaint_t = void(void* a_self, void* a_browser, int a_type, const RectListRaw* a_dirtyRects,
        const void* a_buffer, int a_width, int a_height);
    using Render_t = void(void* a_self);

    OnPaint_t* g_originalOnPaint = nullptr;
    Render_t*  g_originalRender = nullptr;

    // Must be called with g_frame.lock held
    void MarkDirty(int a_left, int a_top, int a_right, int a_bottom)
    {
        if (!g_frame.dirty)
        {
            g_frame.dirtyLeft = a_left;
            g_frame.dirtyTop = a_top;
            g_frame.dirtyRight = a_right;
            g_frame.dirtyBottom = a_bottom;
            g_frame.dirty = true;
            return;
        }

        g_frame.dirtyLeft = (std::min)(g_frame.dirtyLeft, a_left);
        g_frame.dirtyTop = (std::min)(g_frame.dirtyTop, a_top);
        g_frame.dirtyRight = (std::max)(g_frame.dirtyRight, a_right);
        g_frame.dirtyBottom = (std::max)(g_frame.dirtyBottom, a_bottom);
    }

    // Must be called with g_frame.lock held
    void CopyRect(const uint8_t* a_source, int a_x, int a_y, int a_w, int a_h)
    {
        const int left = (std::max)(a_x, 0);
        const int top = (std::max)(a_y, 0);
        const int right = (std::min)(a_x + a_w, g_frame.width);
        const int bottom = (std::min)(a_y + a_h, g_frame.height);
        if (left >= right || top >= bottom)
            return;

        const size_t pitch = static_cast<size_t>(g_frame.width) * 4;
        const size_t rowBytes = static_cast<size_t>(right - left) * 4;
        for (int row = top; row < bottom; ++row)
        {
            const size_t offset = row * pitch + static_cast<size_t>(left) * 4;
            std::memcpy(g_frame.pixels.data() + offset, a_source + offset, rowBytes);
        }

        MarkDirty(left, top, right, bottom);
    }

    void OnPaintHook(void* a_self, void* a_browser, int a_type, const RectListRaw* a_dirtyRects, const void* a_buffer,
        int a_width, int a_height)
    {
        const bool matchesView = a_width == Member<int>(a_self, kWidth) && a_height == Member<int>(a_self, kHeight);

        if (a_type == kPetView && a_buffer && a_width > 0 && a_height > 0 && matchesView)
        {
            std::scoped_lock _(g_frame.lock);

            const auto* source = static_cast<const uint8_t*>(a_buffer);
            const bool  resized = g_frame.width != a_width || g_frame.height != a_height;

            if (resized)
            {
                g_frame.width = a_width;
                g_frame.height = a_height;
                g_frame.pixels.assign(static_cast<size_t>(a_width) * a_height * 4, 0);
            }

            if (resized || !a_dirtyRects || a_dirtyRects->first == a_dirtyRects->last)
            {
                CopyRect(source, 0, 0, a_width, a_height);
            }
            else
            {
                for (auto* rect = a_dirtyRects->first; rect != a_dirtyRects->last; ++rect)
                    CopyRect(source, rect->x, rect->y, rect->width, rect->height);
            }
        }

        // The original skips its full-frame copy for anything but PET_VIEW, but it still has to run:
        // it owns the CefRefPtr<CefBrowser> argument and releases it on the way out.
        g_originalOnPaint(a_self, a_browser, kPetPopup, a_dirtyRects, a_buffer, a_width, a_height);
    }

    void ReleaseTexture()
    {
        if (g_frame.view)
            g_frame.view->Release();
        if (g_frame.texture)
            g_frame.texture->Release();

        g_frame.view = nullptr;
        g_frame.texture = nullptr;
        g_frame.textureWidth = g_frame.textureHeight = 0;
    }

    bool CreateTexture(ID3D11Device* a_device, int a_width, int a_height)
    {
        ReleaseTexture();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(a_width);
        desc.Height = static_cast<UINT>(a_height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(a_device->CreateTexture2D(&desc, nullptr, &g_frame.texture)))
            return false;

        if (FAILED(a_device->CreateShaderResourceView(g_frame.texture, nullptr, &g_frame.view)))
        {
            ReleaseTexture();
            return false;
        }

        g_frame.textureWidth = a_width;
        g_frame.textureHeight = a_height;
        return true;
    }

    void UploadAndBind(void* a_self)
    {
        auto* device = Member<ID3D11Device*>(a_self, kDevice);
        auto* context = Member<ID3D11DeviceContext*>(a_self, kImmediateContext);
        if (!device || !context)
            return;

        // The overlay recreates its device objects on Reset(); never keep a texture from an old device
        if (device != g_frame.device)
        {
            ReleaseTexture();
            g_frame.device = device;
        }

        // Never wait for CEF: if it's writing right now, draw the previous frame
        std::unique_lock lock(g_frame.lock, std::try_to_lock);
        if (lock.owns_lock() && g_frame.width > 0)
        {
            if (g_frame.textureWidth != g_frame.width || g_frame.textureHeight != g_frame.height)
            {
                if (CreateTexture(device, g_frame.width, g_frame.height))
                    MarkDirty(0, 0, g_frame.width, g_frame.height);
            }

            if (g_frame.texture && g_frame.dirty)
            {
                const size_t pitch = static_cast<size_t>(g_frame.width) * 4;
                const D3D11_BOX box{
                    static_cast<UINT>(g_frame.dirtyLeft), static_cast<UINT>(g_frame.dirtyTop), 0,
                    static_cast<UINT>(g_frame.dirtyRight), static_cast<UINT>(g_frame.dirtyBottom), 1
                };
                const uint8_t* source = g_frame.pixels.data() + g_frame.dirtyTop * pitch + static_cast<size_t>(g_frame.dirtyLeft) * 4;

                context->UpdateSubresource(g_frame.texture, 0, &box, source, static_cast<UINT>(pitch), 0);
                g_frame.dirty = false;
            }
        }

        // Point the overlay's draw call at our texture, only when it matches the current view size
        const bool sizeMatches = g_frame.textureWidth == Member<int>(a_self, kWidth) &&
                                 g_frame.textureHeight == Member<int>(a_self, kHeight);
        auto& overlayView = Member<ID3D11ShaderResourceView*>(a_self, kTextureView);

        if (g_frame.view && sizeMatches && overlayView != g_frame.view)
        {
            g_frame.view->AddRef();
            auto* previous = overlayView;
            overlayView = g_frame.view;
            if (previous)
                previous->Release();
        }
    }

    // The game's render thread usually keeps the GPU at 100%, which starves Chromium's own GPU process:
    // the UI then only gets a few frames per second. Capping the game while the UI is open frees the GPU for it.
    struct FrameLimiter
    {
        LONGLONG frequency = 0;
        LONGLONG nextFrame = 0;
        LONGLONG interval = 0;
        LONGLONG keepUntil = 0;
        HANDLE   timer = nullptr;
    };

    constexpr LONGLONG kCloseGraceMs = 1000;

    FrameLimiter g_limiter;
    bool         g_smoothRendering = false;

    LONGLONG Now()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }

    void LimitFrameRate()
    {
        LONGLONG now = Now();
        if (now < g_limiter.nextFrame)
        {
            const LONGLONG remaining = g_limiter.nextFrame - now;
            // Sleep for most of the wait, then spin for the last ~1 ms so the pacing stays even
            const LONGLONG sleepTicks = remaining - g_limiter.frequency / 1000;
            if (sleepTicks > 0 && g_limiter.timer)
            {
                LARGE_INTEGER due;
                due.QuadPart = -(sleepTicks * 10'000'000 / g_limiter.frequency);  // relative, in 100 ns units
                if (SetWaitableTimer(g_limiter.timer, &due, 0, nullptr, nullptr, FALSE))
                    WaitForSingleObject(g_limiter.timer, INFINITE);
            }
            while ((now = Now()) < g_limiter.nextFrame)
                YieldProcessor();
        }

        // Don't try to catch up after a slow frame, just start pacing again from now
        g_limiter.nextFrame = (std::max)(g_limiter.nextFrame + g_limiter.interval, now);
    }

    void RenderHook(void* a_self)
    {
        if (g_smoothRendering)
            UploadAndBind(a_self);

        if (g_limiter.interval)
        {
            // Keep the cap for a moment after the menu closes, so the fade-out animation and its last
            // (fully transparent) frame still get GPU time; otherwise the old frame can stay on screen
            const LONGLONG now = Now();
            if (Member<bool>(a_self, kCursorVisible))
                g_limiter.keepUntil = now + g_limiter.frequency * kCloseGraceMs / 1000;

            if (now < g_limiter.keepUntil)
                LimitFrameRate();
            else
                g_limiter.nextFrame = 0;
        }

        g_originalRender(a_self);
    }
}

void UIRenderFix::Install()
{
    g_smoothRendering = settings::bSmoothUIRendering.GetValue();
    const uint32_t fpsLimit = settings::uUiOpenFpsLimit.GetValue();

    if (!g_smoothRendering && fpsLimit == 0)
    {
        logger::info("bSmoothUIRendering is false and uUiOpenFpsLimit is 0, not installing UI rendering tweaks");
        return;
    }

    const uintptr_t onPaint = memscan::FindPatternInST(memscan::aob::OverlayRenderHandlerD3D11_OnPaint);
    const uintptr_t render = memscan::FindPatternInST(memscan::aob::OverlayRenderHandlerD3D11_Render);
    if (!onPaint || !render)
    {
        logger::warn("UI rendering tweaks: OverlayRenderHandlerD3D11 patterns not found (OnPaint: {:X}, Render: {:X}), skipping", onPaint, render);
        return;
    }

    // Render reads the texture view from the offset this tweak writes to; make sure that hasn't moved
    const uintptr_t renderViewRead = memscan::FindPattern(memscan::aob::OverlayRenderHandlerD3D11_Render_textureViewRead, render);
    if (!renderViewRead || renderViewRead - render > 0x300)
    {
        logger::warn("UI rendering tweaks: unexpected OverlayRenderHandlerD3D11 layout, skipping");
        return;
    }

    if (fpsLimit > 0)
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        g_limiter.frequency = frequency.QuadPart;
        g_limiter.interval = frequency.QuadPart / fpsLimit;
        g_limiter.timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!g_limiter.timer)
            g_limiter.timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }

    if (MH_CreateHookTyped(render, RenderHook, &g_originalRender) != MH_OK)
    {
        logger::warn("UI rendering tweaks: failed to hook Render, skipping");
        return;
    }

    if (g_smoothRendering)
    {
        if (MH_CreateHookTyped(onPaint, OnPaintHook, &g_originalOnPaint) != MH_OK)
        {
            logger::warn("smooth UI rendering: failed to hook OnPaint, disabling it");
            g_smoothRendering = false;
        }
        else
        {
            logger::info("installed smooth UI rendering (OnPaint: {:X})", onPaint);
        }
    }

    if (fpsLimit > 0)
        logger::info("installed UI-open frame rate limit: {} fps (Render: {:X})", fpsLimit, render);
}
