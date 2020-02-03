#pragma once

#include "RendererBase.hpp"

class Renderer : protected RendererBase
{
public:
    Renderer() = delete;
#ifdef VK_USE_PLATFORM_XCB_KHR
    Renderer(xcb_connection_t *connection, xcb_window_t window);
#endif

    void render();
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
    void create_pipeline();
    void create_swapchain();
    void finish_data_upload();

    void record_command_buffer(uint32_t imageIndex);
    void recreate_swapchain();

private:
    VkPhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex;

    VkQueue queue;

    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D surfaceExtent;

    uint8_t timer;
};