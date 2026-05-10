#include "sandbox_app.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/engine.hpp"
#include "engine/plugin_validation.hpp"
#include "engine/project_manifest.hpp"
#include "engine/scenario_script.hpp"
#include "engine/scene.hpp"
#include "engine/scene_loader.hpp"
#include "engine/structured_data.hpp"
#include "engine/world/world_document.hpp"
#include "io/file_system.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {

#if defined(_WIN32)
std::wstring utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return L"Sandbox startup failed";
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}
#endif

void writeStartupErrorLog(const std::filesystem::path& projectRoot, std::string_view message) {
    if (projectRoot.empty()) {
        return;
    }

    std::error_code error;
    const std::filesystem::path logRoot = projectRoot / "user" / "logs";
    std::filesystem::create_directories(logRoot, error);
    if (error) {
        return;
    }

    std::ofstream stream(logRoot / "startup_error.log", std::ios::binary | std::ios::app);
    if (stream) {
        stream << message << "\n\n";
    }
}

void clearStartupErrorLog(const std::filesystem::path& projectRoot) {
    if (projectRoot.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::remove(projectRoot / "user" / "logs" / "startup_error.log", error);
}

int failStartup(const std::filesystem::path& projectRoot, std::string message) {
    std::cerr << message << '\n';
    writeStartupErrorLog(projectRoot, message);
#if defined(_WIN32)
    MessageBoxW(
        nullptr,
        utf8ToWide(message).c_str(),
        L"sandbox_app startup failed",
        MB_OK | MB_ICONERROR | MB_TASKMODAL
    );
#endif
    return 1;
}

std::string pluginValidationMessage(const engine::PluginValidationResult& validation) {
    std::ostringstream stream;
    stream << "runtime plugin selection is invalid:";
    for (const engine::PluginValidationIssue& issue : validation.issues) {
        stream << "\n- " << issue.pluginId << ": " << issue.message;
    }
    return stream.str();
}

struct RuntimeCommandLine {
    std::unordered_map<std::string, std::string> options;
    std::unordered_set<std::string> flags;

    [[nodiscard]] bool hasOption(std::string_view key) const {
        return options.contains(std::string(key));
    }

    [[nodiscard]] std::string_view option(std::string_view key, std::string_view fallback = {}) const {
        const auto it = options.find(std::string(key));
        return it != options.end() ? std::string_view(it->second) : fallback;
    }
};

RuntimeCommandLine parseCommandLine(int argc, char** argv) {
    RuntimeCommandLine cli;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index] != nullptr ? argv[index] : "";
        if (!arg.starts_with("--")) {
            continue;
        }

        const std::string name = arg.substr(2);
        if (index + 1 < argc) {
            const std::string next = argv[index + 1] != nullptr ? argv[index + 1] : "";
            if (!next.starts_with("--")) {
                cli.options[name] = next;
                ++index;
                continue;
            }
        }

        cli.flags.insert(name);
    }
    return cli;
}

struct LocalProjectConfig {
    std::filesystem::path engineRoot;
    std::filesystem::path assetRoot;
    std::filesystem::path assetSourcesRoot;
    std::filesystem::path assetProjectsRoot;
};

std::filesystem::path resolveProjectRelativePath(const std::filesystem::path& projectRoot, std::string_view value) {
    if (value.empty()) {
        return {};
    }

    std::filesystem::path path{std::string(value)};
    if (path.is_relative()) {
        path = projectRoot / path;
    }
    return std::filesystem::absolute(path).lexically_normal();
}

LocalProjectConfig loadLocalProjectConfig(const std::filesystem::path& projectRoot) {
    LocalProjectConfig config;
    const std::filesystem::path localConfig = projectRoot / "project.local.json";
    if (!std::filesystem::exists(localConfig)) {
        return config;
    }

    engine::StructuredDocument document;
    std::string error;
    if (!engine::StructuredDocument::loadFromFile(localConfig, document, error)) {
        return config;
    }

    const engine::StructuredObjectView root = document.rootObject();
    config.engineRoot = resolveProjectRelativePath(projectRoot, root.string("engineRoot"));
    config.assetRoot = resolveProjectRelativePath(projectRoot, root.string("assetRoot"));
    config.assetSourcesRoot = resolveProjectRelativePath(projectRoot, root.string("assetSourcesRoot"));
    config.assetProjectsRoot = resolveProjectRelativePath(projectRoot, root.string("assetProjectsRoot"));
    return config;
}

std::uint32_t parseUnsignedOption(const RuntimeCommandLine& cli, std::string_view key, std::uint32_t fallback) {
    if (!cli.hasOption(key)) {
        return fallback;
    }

    const std::string value(cli.option(key));
    return static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
}

std::filesystem::path executableDirectory(char** argv) {
    const std::filesystem::path executable = argv != nullptr && argv[0] != nullptr
        ? std::filesystem::absolute(std::filesystem::path(argv[0]))
        : std::filesystem::path{};
    return executable.empty() ? std::filesystem::current_path() : executable.parent_path();
}

std::filesystem::path resolveProjectRoot(const RuntimeCommandLine& cli, char** argv) {
    if (cli.hasOption("project-root")) {
        return std::filesystem::absolute(std::filesystem::path(cli.option("project-root")));
    }
    const std::filesystem::path cwd = std::filesystem::current_path();
    if (std::filesystem::exists(cwd / "project.json")) {
        return cwd;
    }
    const std::filesystem::path exeDir = executableDirectory(argv);
    if (std::filesystem::exists(exeDir / "project.json")) {
        return exeDir;
    }
    return std::filesystem::path(SANDBOX_PROJECT_ROOT);
}

std::filesystem::path resolveConfiguredEngineRoot(
    const RuntimeCommandLine& cli,
    const std::filesystem::path& projectRoot,
    const LocalProjectConfig& localConfig
) {
    if (cli.hasOption("engine-root")) {
        return std::filesystem::absolute(std::filesystem::path(cli.option("engine-root"))).lexically_normal();
    }

    if (!localConfig.engineRoot.empty()) {
        return localConfig.engineRoot;
    }

#if defined(_MSC_VER)
    char* envEngineRoot = nullptr;
    std::size_t envEngineRootLength = 0;
    if (_dupenv_s(&envEngineRoot, &envEngineRootLength, "MY_ENGINE_ROOT") == 0 && envEngineRoot != nullptr) {
        const std::unique_ptr<char, decltype(&std::free)> ownedValue(envEngineRoot, &std::free);
        return std::filesystem::absolute(std::filesystem::path(ownedValue.get())).lexically_normal();
    }
#else
    if (const char* envEngineRoot = std::getenv("MY_ENGINE_ROOT")) {
        return std::filesystem::absolute(std::filesystem::path(envEngineRoot)).lexically_normal();
    }
#endif

    return std::filesystem::absolute(std::filesystem::path(SANDBOX_ENGINE_ROOT)).lexically_normal();
}

std::filesystem::path resolveConfiguredAssetRoot(
    const RuntimeCommandLine& cli,
    const std::filesystem::path& projectRoot,
    const LocalProjectConfig& localConfig
) {
    if (cli.hasOption("asset-root")) {
        return std::filesystem::absolute(std::filesystem::path(cli.option("asset-root"))).lexically_normal();
    }
    if (!localConfig.assetRoot.empty()) {
        return localConfig.assetRoot;
    }
    return (projectRoot / "assets").lexically_normal();
}

std::filesystem::path resolveAliasSourceRoot(
    std::string_view sourceRoot,
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& engineRoot
) {
    constexpr std::string_view enginePrefix = "engine:";
    if (sourceRoot.starts_with(enginePrefix)) {
        std::string suffix(sourceRoot.substr(enginePrefix.size()));
        while (!suffix.empty() && (suffix.front() == '/' || suffix.front() == '\\')) {
            suffix.erase(suffix.begin());
        }
        return (engineRoot / std::filesystem::path(suffix)).lexically_normal();
    }

    std::filesystem::path path(sourceRoot);
    if (path.is_relative()) {
        path = projectRoot / path;
    }
    return path.lexically_normal();
}

void mountProjectAliases(
    engine::FileSystem& files,
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& engineRoot,
    const std::filesystem::path& assetRoot,
    const LocalProjectConfig& localConfig,
    const engine::ProjectManifest& manifest
) {
    files.setAlias("project", projectRoot);
    files.setAlias("project_engine", engineRoot);
    files.setAssetRoot(assetRoot);
    files.setAlias("project_assets", projectRoot / "assets");
    files.setAlias("asset_root", assetRoot);
    if (!localConfig.assetSourcesRoot.empty()) {
        files.setAlias("asset_sources", localConfig.assetSourcesRoot);
    }
    if (!localConfig.assetProjectsRoot.empty()) {
        files.setAlias("asset_projects", localConfig.assetProjectsRoot);
    }
    files.setCacheRoot(projectRoot / "cache");
    files.setTempRoot(projectRoot / "temp");
    files.setUserRoot(projectRoot / "user");
    for (const engine::ResourceAliasMount& mount : manifest.resourceAliases) {
        const std::filesystem::path sourceRoot = resolveAliasSourceRoot(mount.sourceRoot, projectRoot, engineRoot);
        files.setAlias(mount.alias, sourceRoot);
    }
}

int runSandboxApp(int argc, char** argv) {
    const RuntimeCommandLine cli = parseCommandLine(argc, argv);

    const std::filesystem::path projectRoot = resolveProjectRoot(cli, argv);
    const LocalProjectConfig localConfig = loadLocalProjectConfig(projectRoot);
    const std::filesystem::path engineRoot = resolveConfiguredEngineRoot(cli, projectRoot, localConfig);
    const std::filesystem::path assetRoot = resolveConfiguredAssetRoot(cli, projectRoot, localConfig);
    const std::filesystem::path projectFile = projectRoot / "project.json";

    engine::ProjectManifest manifest;
    std::string manifestError;
    if (!engine::loadProjectManifest(projectFile, manifest, manifestError)) {
        return failStartup(projectRoot, "failed to load project manifest: " + manifestError);
    }

    engine::WorldDocument startupWorld;
    std::string worldError;
    const std::string startupWorldPath = std::string(cli.option("world", manifest.startupWorld));
    std::filesystem::path startupWorldFile(startupWorldPath);
    if (startupWorldFile.is_relative()) {
        startupWorldFile = projectRoot / startupWorldFile;
    }
    if (!engine::loadWorldDocument(startupWorldFile, startupWorld, worldError)) {
        return failStartup(projectRoot, "failed to load startup world: " + worldError);
    }
    if (startupWorld.scenes.empty()) {
        return failStartup(projectRoot, "startup world has no startupScenes");
    }
    const engine::WorldSceneLayerDesc* primaryScene = &startupWorld.scenes.front();
    for (const engine::WorldSceneLayerDesc& scene : startupWorld.scenes) {
        if (scene.primary) {
            primaryScene = &scene;
            break;
        }
    }

    std::filesystem::path primarySceneFile(primaryScene->scene);
    if (primarySceneFile.is_relative()) {
        primarySceneFile = startupWorldFile.parent_path() / primarySceneFile;
    }

    engine::SceneLoader sceneLoader;
    engine::SceneLoadResult primarySceneResult;
    if (!sceneLoader.load(engine::SceneLoadRequest{primarySceneFile.lexically_normal()}, primarySceneResult)) {
        return failStartup(projectRoot, "failed to load startup world primary scene: " + primarySceneResult.error);
    }

    engine::ScenarioScript scenario;
    if (cli.hasOption("scenario")) {
        std::string scenarioError;
        if (!engine::loadScenarioScript(std::filesystem::path(cli.option("scenario")), scenario, scenarioError)) {
            return failStartup(projectRoot, "failed to load scenario: " + scenarioError);
        }
    }

    engine::EngineDesc desc;
    desc.appName = manifest.displayName.empty()
        ? (manifest.name.empty() ? std::string("sandbox_app") : manifest.name)
        : manifest.displayName;
    desc.windowWidth = 1280;
    desc.windowHeight = 720;
    desc.maxFrames = 0;
    desc.assetRoot = assetRoot;
    desc.configRoot = projectRoot / "config";
    desc.projectRoot = projectRoot;
    desc.runtimePlugins = manifest.runtimePlugins;
    desc.profileOutput = std::filesystem::path(cli.option("profile-output"));
    desc.profileCompareWith = std::filesystem::path(cli.option("profile-baseline"));
    desc.profileLabel = std::string(cli.option(
        "profile-label",
        cli.hasOption("scenario")
            ? (scenario.name.empty() ? std::string_view("sandbox_scenario") : std::string_view(scenario.name))
            : std::string_view("sandbox_runtime")));
    desc.maxFrames = parseUnsignedOption(cli, "max-frames", desc.maxFrames);

    const engine::PluginValidationResult pluginValidation = engine::validateRuntimePluginSelection(desc.runtimePlugins);
    if (pluginValidation.hasErrors()) {
        return failStartup(projectRoot, pluginValidationMessage(pluginValidation));
    }

#if defined(_MSC_VER)
    char* autoCloseFrames = nullptr;
    std::size_t autoCloseFramesLength = 0;
    if (_dupenv_s(&autoCloseFrames, &autoCloseFramesLength, "ENGINE_AUTOCLOSE_FRAMES") == 0 && autoCloseFrames != nullptr) {
        const std::unique_ptr<char, decltype(&std::free)> ownedValue(autoCloseFrames, &std::free);
        desc.maxFrames = static_cast<std::uint32_t>(std::strtoul(ownedValue.get(), nullptr, 10));
    }
#else
    if (const char* autoCloseFrames = std::getenv("ENGINE_AUTOCLOSE_FRAMES")) {
        desc.maxFrames = static_cast<std::uint32_t>(std::strtoul(autoCloseFrames, nullptr, 10));
    }
#endif

#if defined(__EMSCRIPTEN__)
    auto* engine = new engine::Engine();
    if (!engine->init(desc)) {
        return failStartup(projectRoot, "engine initialization failed; check user/logs/startup_error.log and runtime log output for details");
    }
    clearStartupErrorLog(projectRoot);
    mountProjectAliases(engine->fileSystem(), projectRoot, engineRoot, assetRoot, localConfig, manifest);

    auto* app = new engine::SandboxApp(std::move(primarySceneResult.scene), std::move(scenario));
    return engine->run(*app);
#else
    engine::Engine engine;
    if (!engine.init(desc)) {
        return failStartup(projectRoot, "engine initialization failed; check user/logs/startup_error.log and runtime log output for details");
    }
    clearStartupErrorLog(projectRoot);
    mountProjectAliases(engine.fileSystem(), projectRoot, engineRoot, assetRoot, localConfig, manifest);

    engine::SandboxApp app(std::move(primarySceneResult.scene), std::move(scenario));
    return engine.run(app);
#endif
}

}  // namespace

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> arguments;
    std::vector<char*> argv;
    if (wideArgv != nullptr) {
        arguments.reserve(static_cast<std::size_t>(argc));
        argv.reserve(static_cast<std::size_t>(argc));
        for (int index = 0; index < argc; ++index) {
            const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideArgv[index], -1, nullptr, 0, nullptr, nullptr);
            std::string value(static_cast<std::size_t>(std::max(utf8Length, 0)), '\0');
            if (utf8Length > 0) {
                WideCharToMultiByte(CP_UTF8, 0, wideArgv[index], -1, value.data(), utf8Length, nullptr, nullptr);
                value.resize(static_cast<std::size_t>(utf8Length - 1));
            }
            arguments.push_back(std::move(value));
        }
        LocalFree(wideArgv);
    }
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    return runSandboxApp(static_cast<int>(argv.size()), argv.data());
}
#else
int main(int argc, char** argv) {
    return runSandboxApp(argc, argv);
}
#endif
