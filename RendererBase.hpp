#pragma once

#include <vk_mem_alloc.h>

#include <array>
#include <vector>

constexpr uint32_t RENDERER_MAX_FRAMES_IN_FLIGHT = 2;

struct PerFrame
{
    VkCommandBuffer commandBuffer;
    VkFence fence;
};

struct PerImage
{
    VkImage image;
    VkImageView imageView;
    VkFramebuffer framebuffer;
    VkSemaphore renderCompleteSemaphore;
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
        VkDescriptorSetLayout descriptorSetLayout;
        VkPipelineLayout pipelineLayout;
        VkSemaphore acquireCompleteSemaphore;
        VkShaderModule fragmentModule, vertexModule;
        VkPipelineCache pipelineCache;
        std::array<PerFrame, RENDERER_MAX_FRAMES_IN_FLIGHT> perFrameData;

        // Descriptors
        VkBuffer uniformBuffer;
        VmaAllocation uniformMemory;
        VkDescriptorPool descriptorPool;
        VkDescriptorSet descriptorSet;

        // Pipeline
        VkRenderPass renderPass;
        VkPipeline pipeline;

        // Swapchain
        VkSwapchainKHR swapchain;
        VkImage depthImage;
        VmaAllocation depthMemory;
        VkImageView depthView;

        std::vector<PerImage> perImageData;
    } d;
};
