#include "RendererBase.hpp"

RendererBase::RendererBase()
    :d{}
{

}

RendererBase::~RendererBase()
{
    if (d.device)
    {
        vkDeviceWaitIdle(d.device);

        destroy_swapchain();
     
        vkDestroyPipeline(d.device, d.pipeline, nullptr);
        vkDestroyRenderPass(d.device, d.renderPass, nullptr);

        vkDestroyPipelineCache(d.device, d.pipelineCache, nullptr);
        vkDestroyShaderModule(d.device, d.vertexModule, nullptr);
        vkDestroyShaderModule(d.device, d.fragmentModule, nullptr);
        vkDestroySemaphore(d.device, d.acquireCompleteSemaphore, nullptr);
        vkDestroyPipelineLayout(d.device, d.pipelineLayout, nullptr);
        vkDestroyCommandPool(d.device, d.commandPool, nullptr);

        vmaDestroyBuffer(d.allocator, d.vertexBuffer, d.vertexMemory);
        vmaDestroyBuffer(d.allocator, d.indexBuffer, d.indexMemory);
        vmaDestroyBuffer(d.allocator, d.stagingBuffer, d.stagingMemory);

        vkDestroyFence(d.device, d.uploadFence, nullptr);
        vkDestroyCommandPool(d.device, d.uploadCommandPool, nullptr);

        vmaDestroyAllocator(d.allocator);
        vkDestroyDevice(d.device, nullptr);
    }
    if (d.instance)
    {
        vkDestroySurfaceKHR(d.instance, d.surface, nullptr);
        vkDestroyInstance(d.instance, nullptr);
    }
}

void RendererBase::destroy_swapchain()
{
    std::vector<VkFence> fences;
    std::vector<VkCommandBuffer> commandBuffers;
    for (const auto& perImage : perImageData)
    {
        fences.emplace_back(perImage.fence);
        commandBuffers.emplace_back(perImage.commandBuffer);
    }
    vkWaitForFences(d.device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);

    for (const auto& perImage : perImageData)
    {
        vkDestroyFramebuffer(d.device, perImage.framebuffer, nullptr);
        vkDestroyImageView(d.device, perImage.imageView, nullptr);
        vkDestroySemaphore(d.device, perImage.renderCompleteSemaphore, nullptr);
        vkDestroyFence(d.device, perImage.fence, nullptr);
    }
    vkFreeCommandBuffers(d.device, d.commandPool, commandBuffers.size(), commandBuffers.data());
    perImageData.clear();

    vkDestroyImageView(d.device, d.depthView, nullptr);
    d.depthView = nullptr;

    vmaDestroyImage(d.allocator, d.depthImage, d.depthMemory);
    d.depthMemory = VK_NULL_HANDLE;
    d.depthImage = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(d.device, d.swapchain, nullptr);
    d.swapchain = nullptr;
}