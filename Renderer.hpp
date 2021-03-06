#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define VK_USE_PLATFORM_XCB_KHR

#include "Mesh.hpp"
#include "RendererBase.hpp"

#include <glm/gtc/quaternion.hpp>

struct Scene
{
    glm::vec3 cameraLocation;
    glm::vec3 modelLocation;
    glm::quat modelRotation;
};

enum class RendererFlags
{
    None = 0,
    EnableValidation = 1 << 0,
    SupportGpuAssistedDebugging = 1 << 1
};

inline RendererFlags operator|(RendererFlags lhs, RendererFlags rhs)
{
    return static_cast<RendererFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline RendererFlags operator&(RendererFlags lhs, RendererFlags rhs)
{
    return static_cast<RendererFlags>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

class Renderer : protected RendererBase
{
public:
    Renderer() = delete;
#ifdef VK_USE_PLATFORM_XCB_KHR
    Renderer(RendererFlags flags, xcb_connection_t *connection, xcb_window_t window);
#endif

    void render(const Scene& scene);
    void save_caches();

private:
    void create_instance(RendererFlags flags);
    void create_surface(xcb_connection_t *connection, xcb_window_t window);
    void select_physical_device();
    void create_device(RendererFlags flags);
    void create_upload_objects();
    void allocate_static_memory();
    void begin_data_upload();
    void create_common();
    void create_descriptors();
    void create_pipeline();
    void create_swapchain();
    void finish_data_upload();

    void record_command_buffer(uint32_t frameIndex, uint32_t imageIndex, const Scene& scene);
    void recreate_swapchain();

private:
    const Mesh _mesh;

    VkPhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex;

    VkQueue queue;

    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D surfaceExtent;

    uint32_t frameIndex;
};