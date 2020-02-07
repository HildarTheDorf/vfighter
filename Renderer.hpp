#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define VK_USE_PLATFORM_XCB_KHR

#include "RendererBase.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Scene
{
    glm::vec3 cameraLocation;
    glm::vec3 modelLocation;
    glm::quat modelRotation;
};

class Renderer : protected RendererBase
{
public:
    Renderer() = delete;
#ifdef VK_USE_PLATFORM_XCB_KHR
    Renderer(xcb_connection_t *connection, xcb_window_t window);
#endif

    void render(const Scene& scene);
    void save_caches();

private:
    void create_instance();
    void create_surface(xcb_connection_t *connection, xcb_window_t window);
    void select_physical_device();
    void create_device();
    void create_upload_objects();
    void allocate_static_memory();
    void begin_data_upload();
    void create_common();
    void create_descriptors();
    void create_pipeline();
    void create_swapchain();
    void finish_data_upload();

    void record_command_buffer(uint32_t imageIndex, const Scene& scene);
    void recreate_swapchain();

private:
    VkPhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex;

    VkQueue queue;

    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D surfaceExtent;
};