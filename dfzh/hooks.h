#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace DFHack {
namespace DFZH {
namespace Hooks {

    inline std::int64_t dfzh_init_elapsed_us = 0;

    bool init();
    void shutdown();
    void plugin_enable(bool enabled);
    void screen_changed(std::string screen_name);
    void do_command(const std::vector<std::string>& commands);

    void attach();
    void detach();

    void attach_basic_hooks();

    void detach_basic_hooks();

} // namespace Hooks
} // namespace DFZH
} // namespace DFHack
