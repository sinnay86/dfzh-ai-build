#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace DFHack {
namespace DFZH {
namespace Config {
        std::unordered_map<std::string, std::string> loadConfigFile(const std::filesystem::path& configPath);

        std::filesystem::path getDataPath();
        // Returns DFHack installation directory
        std::filesystem::path getDFHackPath();
        std::filesystem::path getDFPath();
        inline std::filesystem::path getConfigPath() {
            return getDataPath() / "dfzh_config.txt";
        }

        // Lazy loading pattern for config - loads only when first accessed
        inline std::unordered_map<std::string, std::string>& getConfig() {
            static std::unordered_map<std::string, std::string> config = loadConfigFile(getConfigPath());
            return config;
        }
        inline void reloadConfig() {
            getConfig() = loadConfigFile(getConfigPath());
        }

        // Use the value of the FONT key from config, use default font if not found
        inline std::filesystem::path getFontFile() {
            return getDataPath() / (getConfig().find("FONT_FILE") != getConfig().end() ? getConfig().at("FONT_FILE") : "MapleMonoNL-CN-Bold.ttf");
        }
        inline std::filesystem::path getLogFile() {
            return getDataPath() / (getConfig().find("LOG_FILE") != getConfig().end() ? getConfig().at("LOG_FILE") : "logs/dfzh.log");
        }
        inline std::filesystem::path getDictFile(std::string dict_type) {
            return getDataPath() / (getConfig().find(dict_type) != getConfig().end() ? getConfig().at(dict_type) : "dfzh_dict_exact.csv");
        }

    }
  }
}
