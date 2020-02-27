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

        for (const auto& perFrame : d.perFrameData)
        {
            vkDestroyFence(d.device, perFrame.fence, nullptr);
        }
     
        vkDestroyPipeline(d.device, d.pipeline, nullptr);
        vkDestroyRenderPass(d.device, d.renderPass, nullptr);

        vkDestroyDescriptorPool(d.device, d.descriptorPool, nullptr);
        vmaDestroyBuffer(d.allocator, d.uniformBuffer, d.uniformMemory);

        vkDestroyPipelineCache(d.device, d.pipelineCache, nullptr);
        vkDestroyShaderModule(d.device, d.vertexModule, nullptr);
        vkDestroyShaderModule(d.device, d.fragmentModule, nullptr);
        vkDestroySemaphore(d.device, d.acquireCompleteSemaphore, nullptr);
        vkDestroyPipelineLayout(d.device, d.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(d.device, d.descriptorSetLayout, nullptr);
        vkDestroyCommandPool(d.device, d.commandPool, nullptr);

        vmaDestroyBuffer(d.allocator, d.vertexBuffer, d.vertexMemory);
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
    for (const auto& perImage : d.perImageData)
    {
        vkDestroyFramebuffer(d.device, perImage.framebuffer, nullptr);
        vkDestroyImageView(d.device, perImage.imageView, nullptr);
        vkDestroySemaphore(d.device, perImage.renderCompleteSemaphore, nullptr);
    }
    d.perImageData.clear();

    vkDestroyImageView(d.device, d.depthView, nullptr);
    d.depthView = nullptr;

    vmaDestroyImage(d.allocator, d.depthImage, d.depthMemory);
    d.depthMemory = VK_NULL_HANDLE;
    d.depthImage = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(d.device, d.swapchain, nullptr);
    d.swapchain = nullptr;
}