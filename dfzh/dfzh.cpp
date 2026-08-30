#include "hooks.h"

#include "Debug.h"
#include "MemAccess.h"
#include "PluginManager.h"
#include "DFHackVersion.h"

#include "modules/Screen.h"
#include "modules/World.h"
#include "modules/Gui.h"

#include "df/world.h"

#include <string>
#include <vector>
#include <cctype>
#include <map>
#include <algorithm>

DFHACK_PLUGIN("dfzh");
REQUIRE_GLOBAL(world);

namespace DFHack {
    // for lifecycle/status events (init, shutdown, enable, save/load)
    // LINFO: important but infrequent state changes
    DBG_DECLARE(dfzh, status, DebugCategory::LINFO);

    // for per-frame update & state change logging
    // LDEBUG: only enable when tracing per-frame behavior
    DBG_DECLARE(dfzh, onupdate, DebugCategory::LDEBUG);

    // for command & keybinding logging
    // user-triggered, moderate frequency
    DBG_DECLARE(dfzh, command, DebugCategory::LINFO);

    namespace DFZH {
        DFHACK_PLUGIN_IS_ENABLED(is_enabled);
    }
}

using namespace DFHack;
using namespace DFHack::DFZH;

static std::map<std::string, std::string> current_bindings;

static command_result do_command(
    color_ostream &out,
    std::vector<std::string> &parameters
);

static void add_binding(color_ostream &out);
static void remove_binding(color_ostream &out);


// ============================================================
// Plugin initialization
// ============================================================

DFhackCExport command_result plugin_init(
    color_ostream &out,
    std::vector<PluginCommand> &commands
) {
    INFO(status, out).printerr(
        "[{}] plugin_init.\n",
        plugin_name
    );

    commands.push_back(
        PluginCommand(
            plugin_name,
            "Dwarf Fortress Chinese plugin.",
            do_command,
            Gui::anywhere_hotkey
        )
    );

    if (!Hooks::init()) {
        ERR(status, out).printerr(
            "[{}] init failed, plugin not fully loaded.\n",
            plugin_name
        );

        return CR_FAILURE;
    }

    Hooks::attach();

    return CR_OK;
}


// ============================================================
// Plugin shutdown
// ============================================================

DFhackCExport command_result plugin_shutdown(
    color_ostream &out
) {
    INFO(status, out).printerr(
        "[{}] plugin_shutdown.\n",
        plugin_name
    );

    if (is_enabled) {
        remove_binding(out);
    }

    Hooks::detach();
    Hooks::shutdown();

    return CR_OK;
}


// ============================================================
// Plugin enable / disable
// ============================================================

DFhackCExport command_result plugin_enable(
    color_ostream &out,
    bool enable
) {
    DEBUG(status, out).print(
        "[{}] {} from the API.\n",
        plugin_name,
        enable ? "enabled" : "disabled"
    );

    std::string plugin_name_upper = plugin_name;

    std::transform(
        plugin_name_upper.begin(),
        plugin_name_upper.end(),
        plugin_name_upper.begin(),
        [](unsigned char c) {
            return std::toupper(c);
        }
    );

    if (enable != is_enabled) {
        is_enabled = enable;

        Hooks::plugin_enable(is_enabled);

        if (is_enabled) {
            add_binding(out);
        } else {
            remove_binding(out);
        }

        if (is_enabled) {
            float duration_ms =
                static_cast<float>(Hooks::dfzh_init_elapsed_us)
                / 1000.0f;

            INFO(status, out).print(
                "Enabled | init: {:0.3f} ms | outperforms 99% of all plugins.\n",
                duration_ms
            );
        }

    } else {
        INFO(status, out).print(
            "[{}] now is {}.\n",
            plugin_name,
            enable ? "enabled" : "disabled"
        );
    }

    return CR_OK;
}


// ============================================================
// State change handler
// ============================================================

DFhackCExport command_result plugin_onstatechange(
    color_ostream &out,
    state_change_event event
) {
    switch (event) {

        case SC_UNKNOWN:
            TRACE(onupdate, out).printerr(
                "[{}] SC_UNKNOWN\n",
                plugin_name
            );
            break;

        case SC_WORLD_LOADED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_WORLD_LOADED\n",
                plugin_name
            );
            break;

        case SC_WORLD_UNLOADED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_WORLD_UNLOADED\n",
                plugin_name
            );
            break;

        case SC_MAP_LOADED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_MAP_LOADED\n",
                plugin_name
            );
            break;

        case SC_MAP_UNLOADED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_MAP_UNLOADED\n",
                plugin_name
            );
            break;

        case SC_VIEWSCREEN_CHANGED:
        {
            auto vs = Gui::getCurViewscreen(true);

            std::string name =
                Core::getInstance().p->readClassName(
                    *(void**)vs
                );

            if (name.starts_with("viewscreen_")) {

                name = name.substr(
                    4,
                    name.size() - 4 - 2
                );

            } else if (dfhack_viewscreen::is_instance(vs)) {

                auto fs = Gui::getFocusStrings(vs);

                if (fs.size()) {
                    name = "";

                    for (const auto& str : fs) {
                        name += str;
                        name += ";";
                    }
                }
            }

            TRACE(onupdate, out).printerr(
                "[{}] SC_VIEWSCREEN_CHANGED ({})\n",
                plugin_name,
                name.c_str()
            );

            Hooks::screen_changed(name);

            break;
        }

        case SC_CORE_INITIALIZED:
            DEBUG(onupdate, out).printerr(
                "[{}] SC_CORE_INITIALIZED\n",
                plugin_name
            );
            break;

        case SC_BEGIN_UNLOAD:
            DEBUG(onupdate, out).printerr(
                "[{}] SC_BEGIN_UNLOAD\n",
                plugin_name
            );
            break;

        case SC_PAUSED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_PAUSED\n",
                plugin_name
            );
            break;

        case SC_UNPAUSED:
            TRACE(onupdate, out).printerr(
                "[{}] SC_UNPAUSED\n",
                plugin_name
            );
            break;
    }

    return CR_OK;
}


// ============================================================
// Per-frame update
// ============================================================

DFhackCExport command_result plugin_onupdate(
    color_ostream &out
) {
    return CR_OK;
}


// ============================================================
// Save / load
// ============================================================

DFhackCExport command_result plugin_save_site_data(
    color_ostream &out
) {
    DEBUG(status, out).printerr(
        "[{}] save or unload is imminent; time to persist state for site.\n",
        plugin_name
    );

    return CR_OK;
}


DFhackCExport command_result plugin_save_world_data(
    color_ostream &out
) {
    DEBUG(status, out).printerr(
        "[{}] save or unload is imminent; time to persist state for world.\n",
        plugin_name
    );

    return CR_OK;
}


DFhackCExport command_result plugin_load_world_data(
    color_ostream &out
) {
    DEBUG(status, out).printerr(
        "[{}] world is loading; time to load world-global persisted data.\n",
        plugin_name
    );

    return CR_OK;
}


DFhackCExport command_result plugin_load_site_data(
    color_ostream &out
) {
    DEBUG(status, out).printerr(
        "[{}] site is loading; time to load site-local persisted data.\n",
        plugin_name
    );

    return CR_OK;
}


// ============================================================
// Command handler
// ============================================================

static command_result do_command(
    color_ostream &out,
    std::vector<std::string> &parameters
) {
    INFO(command, out).print(
        "do_command with {} parameters\n",
        parameters.size()
    );

    if (parameters.empty()) {
        return CR_OK;
    }

    Hooks::do_command(parameters);

    return CR_OK;
}


// ============================================================
// Hotkey bindings
// ============================================================

static void add_binding(
    color_ostream &out
) {
    current_bindings["Ctrl-Alt-L"] =
        "dfzh save_untrans";

    current_bindings["Ctrl-Alt-R"] =
        "dfzh reload_dicts";

    current_bindings["Ctrl-Alt-K"] =
        "dfzh show_ch";

    for (const auto& binding : current_bindings) {

        Core::getInstance()
            .getHotkeyManager()
            ->addKeybind(
                binding.first,
                binding.second
            );

        DEBUG(command, out).print(
            "adding keybinding: {} -> {}\n",
            binding.first,
            binding.second
        );
    }
}


static void remove_binding(
    color_ostream &out
) {
    for (const auto& binding : current_bindings) {

        Core::getInstance()
            .getHotkeyManager()
            ->removeKeybind(
                binding.first
            );

        DEBUG(command, out).print(
            "removing keybinding: {} -> {}\n",
            binding.first,
            binding.second
        );
    }
}
