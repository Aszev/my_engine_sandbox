#pragma once

#include "engine/application.hpp"
#include "engine/render_view_types.hpp"
#include "engine/scenario_controller.hpp"
#include "engine/scene.hpp"
#include "engine/screen_types.hpp"

namespace engine {

class SandboxApp final : public Application {
public:
    SandboxApp(SceneDocument scene, ScenarioScript scenario = {});

    void onInit(AppContext& ctx) override;
    void onUpdate(AppContext& ctx, FrameContext& frame) override;
    void onRender(AppContext& ctx, FrameContext& frame) override;
    void onShutdown(AppContext& ctx) override;

private:
    SceneDocument m_scene;
    ScenarioController m_scenario;
    bool m_hasScenario = false;
    bool m_scenarioFailureHandled = false;
    ScreenHandle m_screen;
    ViewportHandle m_mainViewport;
    RenderViewHandle m_mainRenderView;
};

}  // namespace engine
