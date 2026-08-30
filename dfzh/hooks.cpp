
#include "hooks.h"
#include "hook_common.h"
#include "sdl2_hooks.h"
#include "screen_manager.h"
#include "logger.h"


#include <SDL.h>
#include "detours/detours.h"

#include <iostream>
#include <format>
#include <string>
#include <vector>
#include <chrono>

#define ATTACH_HOOK(fn_name) DetourAttach(&(PVOID&)(ORIG_FUNC(fn_name)), (PVOID)(HOOK_FUNC(fn_name)))
#define DETACH_HOOK(fn_name) DetourDetach(&(PVOID&)(ORIG_FUNC(fn_name)), (PVOID)(HOOK_FUNC(fn_name)))
#define ATTACH_SDL_HOOK(fn_name) DetourAttach(&(PVOID&)(g_sdl2.ORIG_FUNC(fn_name)), (PVOID)(HOOK_FUNC(fn_name)))
#define DETACH_SDL_HOOK(fn_name) DetourDetach(&(PVOID&)(g_sdl2.ORIG_FUNC(fn_name)), (PVOID)(HOOK_FUNC(fn_name)))

namespace DFHack {
namespace DFZH {
namespace Hooks {
    // /* dfhoos.dll hook function */
    // typedef void(__cdecl* dfhooks_prerender)();
    // typedef void(__cdecl* dfhooks_sdl_loop)();
    // typedef void(__cdecl* dfhooks_update)();
    typedef bool(__cdecl* dfhooks_sdl_event)(SDL_Event* event);

    std::int64_t dfzh_proc_elapsed_us = 0;
    std::int64_t df_frame_elapsed_us = 0;

    bool hook_func_init_done = false;
    bool plugin_is_enabled = false;
    bool basic_func_hook_done = false;

    void __cdecl HOOK_FUNC(SDL_RenderPresent)(SDL_Renderer * renderer) {
        if (hook_func_init_done && plugin_is_enabled) {
            auto start = std::chrono::high_resolution_clock::now();

            if (!SCREENMANAGER.preSDLRenderPresent(renderer)) {
                return;
            }

            // ** Elapsed time of TextHook ** //
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            auto text_proc_time = dfzh_proc_elapsed_us;
            auto text_render_time = elapsed_us.count();
            dfzh_proc_elapsed_us = text_proc_time + text_render_time;
            // LOGGERMANAGER.getLogger()->info("TextHook: {} us, TextRender: {} us, Total: {} us", text_proc_time, text_render_time, dfzh_proc_elapsed_us);
        }

        static auto last_frame_time = std::chrono::high_resolution_clock::now(); // 只初始化一次
        auto current_frame_time = std::chrono::high_resolution_clock::now();
        // ** Calculate frame interval (milliseconds) ** //
        // auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_frame_time - last_frame_time);
        auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(current_frame_time - last_frame_time);
        last_frame_time = current_frame_time;   // 更新 last_frame_time 为当前帧时间
        df_frame_elapsed_us = delta_us.count();
        if (hook_func_init_done && plugin_is_enabled) {
            SCREENMANAGER.updatePerformanceStatistics(dfzh_proc_elapsed_us, df_frame_elapsed_us);
        }

        g_sdl2.ORIG_FUNC(SDL_RenderPresent)(renderer);
    }

    void __cdecl HOOK_FUNC(SDL_GetWindowSize)(SDL_Window * window, int* w, int* h) {
        g_sdl2.ORIG_FUNC(SDL_GetWindowSize)(window, w, h);
        // LOGGERMANAGER.getLogger()->info("SDL_GetWindowSize end. w:%d, h:%d\n", *w, *h);
        if (hook_func_init_done && plugin_is_enabled) {
            auto start = std::chrono::high_resolution_clock::now();
            SCREENMANAGER.onSDLGetWindowSize();

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            dfzh_proc_elapsed_us = elapsed_us.count();
        }
    }

    // int __cdecl HOOK_FUNC(SDL_PollEvent)(SDL_Event* event) {
    //     int ret = g_sdl2.ORIG_FUNC(SDL_PollEvent)(event);
    //     return ret;
    // }

    // SETUP_ORIG_FUNC_FNAME(dfhooks_prerender,dfhooks.dll);
    // void __cdecl HOOK_FUNC(dfhooks_prerender)() {
    //     SCREENMANAGER.preDFhooksPreRender();
    //     ORIG_FUNC(dfhooks_prerender)();
    // }
    SETUP_ORIG_FUNC_FNAME(dfhooks_sdl_event,dfhooks.dll);
    bool __cdecl HOOK_FUNC(dfhooks_sdl_event)(SDL_Event* event) {
        return ORIG_FUNC(dfhooks_sdl_event)(event);
    }
    // SETUP_ORIG_FUNC_FNAME(dfhooks_sdl_loop,dfhooks.dll);
    // void __cdecl HOOK_FUNC(dfhooks_sdl_loop)() {
    //     // printf("dfhooks_sdl_loop start.\n");
    //     ORIG_FUNC(dfhooks_sdl_loop)();
    // }
    // SETUP_ORIG_FUNC_FNAME(dfhooks_update,dfhooks.dll);
    // void __cdecl HOOK_FUNC(dfhooks_update)() {
    //     // printf("dfhooks_update start.\n");
    //     ORIG_FUNC(dfhooks_update)();
    // }

    bool init() {
        // std::cerr << std::format("[dfzh] Hooks::init ({}).", hook_func_init_done) << std::endl;
        if(hook_func_init_done) {
            return true;
        }
        auto start = std::chrono::high_resolution_clock::now();

        LOGGERMANAGER.init();
        if(!g_sdl2.loadFunc()){
            LOGGERMANAGER.getLogger()->error("Hooks::init: g_sdl2.loadFunc failed");
            return false;
        }
        if(!SCREENMANAGER.init()){
            LOGGERMANAGER.getLogger()->error("Hooks::init: SCREENMANAGER.init failed");
            return false;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        dfzh_init_elapsed_us = elapsed_us.count();
        LOGGERMANAGER.getLogger()->info("SCREENMANAGER.init elapsed time: {} us", dfzh_init_elapsed_us);
        hook_func_init_done = true;
        // std::cerr << "[dfzh] Hooks::init done." << std::endl;
        return true;
    }

    void shutdown() {
        // std::cerr << std::format("[dfzh] Hooks::shutdown ({}).", hook_func_init_done) << std::endl;
        if(!hook_func_init_done) return;
        hook_func_init_done = false;
        SCREENMANAGER.shutdown();
        LOGGERMANAGER.shutdown();
        // std::cerr << "[dfzh] Hooks::shutdown done." << std::endl;
    }
    void plugin_enable(bool enabled) {
        plugin_is_enabled = enabled;
    }
    void attach() {
        // std::cerr << std::format("[dfzh] Hooks::attach ({},{}).", hook_func_init_done, basic_func_hook_done) << std::endl;
        if(!hook_func_init_done) return;
        if(!basic_func_hook_done) {
            attach_basic_hooks();
            basic_func_hook_done = true;
        }
        // std::cerr << "[dfzh] Hooks::attach done." << std::endl;
    }

    void detach() {
        // std::cerr << std::format("[dfzh] Hooks::detach ({}).", basic_func_hook_done) << std::endl;
        if(basic_func_hook_done) {
            detach_basic_hooks();
            basic_func_hook_done = false;
        }
        // std::cerr << "[dfzh] Hooks::detach done." << std::endl;
    }

    void screen_changed(std::string screen_name) {
        if (!hook_func_init_done) return;
        SCREENMANAGER.screenChanged(screen_name);
    }

    void do_command(const std::vector<std::string>& commands) {
        if (!hook_func_init_done) return;
        SCREENMANAGER.doCommand(commands);
    }

    void attach_basic_hooks() {
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        ATTACH_SDL_HOOK(SDL_RenderPresent);
        ATTACH_SDL_HOOK(SDL_GetWindowSize);
        // ATTACH_SDL_HOOK(SDL_PollEvent);

        // ATTACH_HOOK(dfhooks_prerender);
        // ATTACH_HOOK(dfhooks_sdl_loop);
        // ATTACH_HOOK(dfhooks_update);
        ATTACH_HOOK(dfhooks_sdl_event);

        DetourTransactionCommit();
    }

    void detach_basic_hooks() {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DETACH_SDL_HOOK(SDL_RenderPresent);
        DETACH_SDL_HOOK(SDL_GetWindowSize);
        // DETACH_SDL_HOOK(SDL_PollEvent);
        // DETACH_HOOK(dfhooks_prerender);
        // DETACH_HOOK(dfhooks_sdl_loop);
        // DETACH_HOOK(dfhooks_update);
        DETACH_HOOK(dfhooks_sdl_event);

        DetourTransactionCommit();
    }
} // namespace Hooks
} // namespace DFZH
} // namespace DFHack
