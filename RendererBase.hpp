#pragma once

#include <vk_mem_alloc.h>

#include <vector>

struct PerImage
{
    VkCommandBuffer commandBuffer;

    VkFence fence;
    VkSemaphore renderCompleteSemaphore;

    VkImage image;
    VkImageView imageView;
    VkFramebuffer framebuffer;
};

class RendererBase
{
protected:
    RendererBase();
    RendererBase(const RendererBase&) = delete;
    RendererBase(RendererBase&&) = default;
    ~RendererBase();

    RendererBase& operator=(const RendererBase&) = delete;
    RendererBase& operator=(RendererBase&&) = default;

    void destroy_swapchain();

    struct
    {
        // Instance
        VkInstance instance;

        // Surface
        VkSurfaceKHR surface;

        // Device
        VkDevice device;
        VmaAllocator allocator;

        // Upload
        VkCommandPool uploadCommandPool;
        VkCommandBuffer uploadCommandBuffer;
        VkFence uploadFence;

        // Static memory
        VkBuffer stagingBuffer, indexBuffer, vertexBuffer;
        VmaAllocation stagingMemory, indexMemory, vertexMemory;

        // Common
        VkCommandPool commandPool;
        VkPipelineLayout pipelineLayout;
        VkSemaphore acquireCompleteSemaphore;
        VkShaderModule fragmentModule, vertexModule;
        VkPipelineCache pipelineCache;

        // Pipeline
        VkRenderPass renderPass;
        VkPipeline pipeline;

        // Swapchain
        VkSwapchainKHR swapchain;
        VkImage depthImage;
        VmaAllocation depthMemory;
        VkImageView depthView;
    } d;
    std::vector<PerImage> perImageData;
};
