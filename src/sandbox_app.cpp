#include "sandbox_app.hpp"

#include <utility>

#include "ecs/ecs_world.hpp"
#include "engine/app_context.hpp"
#include "engine/engine.hpp"
#include "engine/frame_context.hpp"
#include "engine/platform.hpp"
#include "engine/log.hpp"
#include "engine/render_view_manager.hpp"
#include "engine/render/pipeline/render_pipeline_runtime.hpp"
#include "engine/screen_manager.hpp"
#include "engine/viewport3d.hpp"
#include "engine/world_renderer.hpp"
#include "engine/world_loader.hpp"
#include "render/render_device.hpp"
#include "resources/resource_manager.hpp"
#include "ui_runtime/ui_backend.hpp"

namespace engine {

SandboxApp::SandboxApp(SceneDocument scene, ScenarioScript scenario)
    : m_scene(std::move(scene))
    , m_hasScenario(!scenario.steps.empty()) {
    if (m_hasScenario) {
        std::string error;
        if (!m_scenario.start(std::move(scenario), error)) {
            ENGINE_LOG_ERROR("SandboxApp scenario start failed during construction: {}", error);
            m_hasScenario = false;
        }
    }
}

void SandboxApp::onInit(AppContext& ctx) {
    ENGINE_LOG_INFO("SandboxApp init");

    WorldLoadStats stats;
    std::string error;
    if (!populateWorldFromScene(ctx.world, m_scene, stats, error, &ctx.resources, &ctx.components)) {
        ENGINE_LOG_ERROR("SandboxApp world load failed: {}", error);
    }
    m_screen = ctx.screens.primaryScreen();
    ViewportDesc viewportDesc;
    viewportDesc.name = "sandbox_main_viewport";
    viewportDesc.screen = m_screen;
    viewportDesc.rect = ViewportRect{
        m_scene.viewport.x,
        m_scene.viewport.y,
        m_scene.viewport.width,
        m_scene.viewport.height,
    };
    viewportDesc.bgfxViewId = m_scene.viewport.viewId;
    viewportDesc.domain = WorldDomain::PlayScene;
    viewportDesc.cameraRole = "play_main";
    viewportDesc.inputSlot = 1;
    viewportDesc.visible = true;
    viewportDesc.interactive = true;
    viewportDesc.focused = true;
    m_mainViewport = ctx.screens.createViewport(viewportDesc);
    ctx.screens.setFocusedViewport(m_mainViewport);

    RenderViewDesc renderViewDesc;
    renderViewDesc.name = "sandbox_main_render_view";
    renderViewDesc.domain = WorldDomain::PlayScene;
    renderViewDesc.targetKind = RenderTargetKind::ScreenViewport;
    renderViewDesc.viewport = m_mainViewport;
    renderViewDesc.cameraRole = "play_main";
    renderViewDesc.inputSlot = 1;
    renderViewDesc.bgfxViewId = m_scene.viewport.viewId;
    renderViewDesc.enabled = true;
    m_mainRenderView = ctx.renderViews.createRenderView(renderViewDesc);

    (void)ctx.renderViews.makeViewport3D(m_mainRenderView, ctx.screens);
}

void SandboxApp::onUpdate(AppContext& ctx, FrameContext& frame) {
    if (m_hasScenario) {
        m_scenario.update(frame, [&ctx]() {
            ctx.engine.requestQuit();
        });

        const ScenarioControllerStatus& status = m_scenario.status();
        if (status.failed && !m_scenarioFailureHandled) {
            m_scenarioFailureHandled = true;
            ENGINE_LOG_ERROR("SandboxApp scenario failed: {}", status.lastError);
            ctx.engine.requestQuit();
        }
    }
}

void SandboxApp::onRender(AppContext& ctx, FrameContext& frame) {
    auto& render = ctx.engine.renderDevice();
    const Viewport3D viewport = ctx.renderViews.makeViewport3D(m_mainRenderView, ctx.screens);

    render.setViewRect(0, 0, 0, ctx.platform.width(), ctx.platform.height());
    render.clear(0, 0.10f, 0.12f, 0.16f, 1.0f);

    RenderViewFrame scene = buildRenderViewFrame(
        m_mainRenderView,
        ctx.renderViews,
        ctx.screens,
        ctx.worlds,
        ctx.resources,
        ctx.renderPipeline.plan()
    );
    if (scene.valid) {
        render.setViewRect(viewport.bgfxViewId, viewport.x, viewport.y, viewport.width, viewport.height);
        render.clear(viewport.bgfxViewId, 0.04f, 0.04f, 0.05f, 1.0f);
        renderViewFrame(render, scene, ctx.resources, ctx.renderPipeline.plan());
    }

    if (UiBackendRegistry* registry = ctx.worlds.tryService<UiBackendRegistry>(); registry != nullptr) {
        if (UiBackend* backend = registry->defaultBackend(); backend != nullptr) {
            backend->renderOverlay(UiBackendFrame{
                .frame = frame.frameIndex,
                .deltaTime = frame.deltaTime,
                .time = frame.time,
            });
        }
    }
}

void SandboxApp::onShutdown(AppContext& ctx) {
    if (m_mainRenderView.valid()) {
        ctx.renderViews.destroyRenderView(m_mainRenderView);
        m_mainRenderView = {};
    }
    if (m_mainViewport.valid()) {
        ctx.screens.destroyViewport(m_mainViewport);
        m_mainViewport = {};
    }
    ENGINE_LOG_INFO("SandboxApp shutdown, entities={}", ctx.world.entityCount());
}

}  // namespace engine
