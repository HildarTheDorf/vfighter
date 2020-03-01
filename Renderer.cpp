#include "Renderer.hpp"

#include "BadVkResult.hpp"

#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

constexpr uint32_t NUM_LIGHTS = 1;
constexpr uint32_t MAX_MATERIALS = 1;

struct Light {
    glm::vec3 position;
    float _padding;
    glm::vec3 color;
    float power;
};

struct Material {
    glm::vec3 ambient;
    float _padding0;
    glm::vec3 diffuse;
    float _padding1;
    glm::vec3 specular;
    float shininess;
};

struct SpecConstants {
    uint32_t numLights;
    uint32_t maxMaterials;
};

struct PushConstants
{
    uint32_t materialIndex;
};

struct LightingUniforms {
    Light lights[NUM_LIGHTS];
    Material materials[MAX_MATERIALS];
};

struct TransformUniforms
{
    glm::mat4 modelViewMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 normalMatrix;
};

constexpr uint32_t DEFAULT_IMAGE_COUNT = 3;

constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D16_UNORM;
constexpr float FIELD_OF_VIEW = glm::radians(45.0f);
constexpr float NEAR_CLIP_PLANE = 1.0f;
constexpr char PIPELINE_CACHE_FILENAME[] = "pipelinecache.bin";
constexpr VkDeviceSize STAGING_BUFFER_SIZE = 1 << 17;

constexpr void check_success(VkResult vkResult)
{
    if (vkResult)
    {
        throw BadVkResult(vkResult);
    }
}

static std::vector<uint8_t> load_file(const std::string& name)
{
    std::ifstream file(name, std::ios::binary);
    std::noskipws(file);

    std::vector<uint8_t> ret;
    std::copy(std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>(), std::back_inserter(ret));
    return ret;
}

static std::vector<uint32_t> load_shader(const std::string& name)
{
    const auto raw = load_file("shaders/" + name + ".spv");
    std::vector<uint32_t> ret(raw.size() / sizeof(uint32_t));
    memcpy(ret.data(), raw.data(), ret.size() * sizeof(uint32_t));
    return ret;
}

template<typename T>
static void save_file(const std::string& name, T begin, T end)
{
    std::ofstream file(name, std::ios::binary | std::ios::trunc);
    std::copy(begin, end, std::ostream_iterator<uint8_t>(file));
}

static VkSurfaceFormatKHR select_format(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t numSurfaceFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numSurfaceFormats, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(numSurfaceFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numSurfaceFormats, surfaceFormats.data());

    for (const auto surfaceFormat : surfaceFormats)
    {
        switch (surfaceFormat.format)
        {
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return surfaceFormat;
        default:
            break;
        }            
    }

    return surfaceFormats[0];
}

static VkPresentModeKHR select_present_mode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t numPresentModes;
    check_success(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numPresentModes, nullptr));
    std::vector<VkPresentModeKHR> presentModes(numPresentModes);
    check_success(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numPresentModes, presentModes.data()));

    constexpr std::array desiredPresentModes = { VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR };
    for (const auto desiredPresentMode : desiredPresentModes)
    {
        if (presentModes.end() != std::find(presentModes.begin(), presentModes.end(), desiredPresentMode))
        {
            return desiredPresentMode;
        }
    }

    throw std::runtime_error("No supported present mode");
}

Renderer::Renderer(RendererFlags flags, xcb_connection_t *connection, xcb_window_t window)
    :_mesh("../models/monkey.obj")
{
    create_instance(flags);
    create_surface(connection, window);
    select_physical_device();
    create_device(flags);
    create_upload_objects();
    allocate_static_memory();
    begin_data_upload();
    create_common();
    create_descriptors();
    create_pipeline();
    create_swapchain();
    finish_data_upload();
}

void Renderer::render(const Scene& scene)
{
    uint32_t imageIndex;
    const auto acquireResult = vkAcquireNextImageKHR(d.device, d.swapchain, UINT64_MAX, d.acquireCompleteSemaphore, nullptr, &imageIndex);

    bool swapchainOutOfDate;
    switch (acquireResult)
    {
    case VK_SUCCESS:
        swapchainOutOfDate = false;
        break;
    case VK_SUBOPTIMAL_KHR:
        swapchainOutOfDate = true;
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        swapchainOutOfDate = true;
        break;
    default:
        check_success(acquireResult);
    }

    if (!swapchainOutOfDate)
    {
        frameIndex = (frameIndex + 1) % RENDERER_MAX_FRAMES_IN_FLIGHT;

        const auto& frameData = d.perFrameData[frameIndex];
        const auto& imageData = d.perImageData[imageIndex];

        check_success(vkWaitForFences(d.device, 1, &frameData.fence, VK_TRUE, UINT64_MAX));
        check_success(vkResetFences(d.device, 1, &frameData.fence));

        record_command_buffer(frameIndex, imageIndex, scene);

        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &d.acquireCompleteSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frameData.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &imageData.renderCompleteSemaphore;
        check_success(vkQueueSubmit(queue, 1, &submitInfo, frameData.fence));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &imageData.renderCompleteSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &d.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        const auto presentResult = vkQueuePresentKHR(queue, &presentInfo);
        switch (presentResult)
        {
        case VK_SUCCESS:
            break;
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            swapchainOutOfDate = true;
            break;
        default:
            check_success(presentResult);
        }
    }

    if (swapchainOutOfDate)
    {
        recreate_swapchain();
    }
}

void Renderer::save_caches()
{
    size_t dataSize;
    check_success(vkGetPipelineCacheData(d.device, d.pipelineCache, &dataSize, nullptr));
    std::vector<uint8_t> data(dataSize);
    check_success(vkGetPipelineCacheData(d.device, d.pipelineCache, &dataSize, data.data()));

    save_file(PIPELINE_CACHE_FILENAME, data.begin(), data.end());
}

void Renderer::create_instance(RendererFlags flags)
{
    VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char *> enabledLayers;
    std::vector instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME };

    if (RendererFlags::None != (RendererFlags::EnableValidation & flags))
    {
        enabledLayers.emplace_back("VK_LAYER_KHRONOS_validation");
        instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = enabledLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
    instanceCreateInfo.enabledExtensionCount = instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    check_success(vkCreateInstance(&instanceCreateInfo, nullptr, &d.instance));
}

void Renderer::create_surface(xcb_connection_t *connection, xcb_window_t window)
{
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = { VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
    surfaceCreateInfo.connection = connection;
    surfaceCreateInfo.window = window;

    check_success(vkCreateXcbSurfaceKHR(d.instance, &surfaceCreateInfo, nullptr, &d.surface));
}

void Renderer::select_physical_device()
{
    uint32_t numPhysicalDevices;
    check_success(vkEnumeratePhysicalDevices(d.instance, &numPhysicalDevices, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
    check_success(vkEnumeratePhysicalDevices(d.instance, &numPhysicalDevices, physicalDevices.data()));

    for (const auto physicalDevice : physicalDevices)
    {
        uint32_t numQueueFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

        for (size_t i = 0; i < queueFamilyProperties.size(); ++i)
        {
            const auto& queueFamily = queueFamilyProperties[i];
            VkBool32 surfaceSupported;
            check_success(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, d.surface, &surfaceSupported));

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && surfaceSupported)
            {
                this->physicalDevice = physicalDevice;
                queueFamilyIndex = i;
                return;
            }
        }
    }

    throw std::runtime_error("No supported device found");
}

void Renderer::create_device(RendererFlags flags)
{
    VkPhysicalDeviceFeatures availableFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &availableFeatures);

    constexpr auto queuePriority = 0.0f;

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    constexpr std::array deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures enabledFeatures = {};

    if (RendererFlags::None != (RendererFlags::SupportGpuAssistedDebugging & flags))
    {
        enabledFeatures.fragmentStoresAndAtomics = availableFeatures.fragmentStoresAndAtomics;
        enabledFeatures.vertexPipelineStoresAndAtomics = availableFeatures.vertexPipelineStoresAndAtomics;
    }

    VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO } ;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    check_success(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &d.device));
    vkGetDeviceQueue(d.device, queueFamilyIndex, 0, &queue);

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = d.device;
    allocatorCreateInfo.instance = d.instance;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    check_success(vmaCreateAllocator(&allocatorCreateInfo, &d.allocator));
}

void Renderer::create_upload_objects()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    check_success(vkCreateCommandPool(d.device, &commandPoolCreateInfo, nullptr, &d.uploadCommandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commandBufferAllocateInfo.commandPool = d.uploadCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    check_success(vkAllocateCommandBuffers(d.device, &commandBufferAllocateInfo, &d.uploadCommandBuffer));

    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    check_success(vkCreateFence(d.device, &fenceCreateInfo, nullptr, &d.uploadFence));
}

void Renderer::allocate_static_memory()
{
    VkBufferCreateInfo stagingBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    stagingBufferCreateInfo.size = STAGING_BUFFER_SIZE;
    stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingBufferAllocationCreateInfo = {};
    stagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    check_success(vmaCreateBuffer(d.allocator, &stagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &d.stagingBuffer, &d.stagingMemory, nullptr));

    VkBufferCreateInfo transformUniformBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    transformUniformBufferCreateInfo.size = RENDERER_MAX_FRAMES_IN_FLIGHT * sizeof(TransformUniforms);
    transformUniformBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo transformUniformBufferAllocationInfo = { };
    transformUniformBufferAllocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    transformUniformBufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    
    check_success(vmaCreateBuffer(d.allocator, &transformUniformBufferCreateInfo, &transformUniformBufferAllocationInfo, &d.transformUniformBuffer, &d.transformUniformMemory, nullptr));

    VkBufferCreateInfo lightingUniformBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    lightingUniformBufferCreateInfo.size = sizeof(LightingUniforms);
    lightingUniformBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo lightingUniformBufferAllocationInfo = { };
    lightingUniformBufferAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    check_success(vmaCreateBuffer(d.allocator, &lightingUniformBufferCreateInfo, &lightingUniformBufferAllocationInfo, &d.lightingUniformBuffer, &d.lightingUniformMemory, nullptr));

    VkBufferCreateInfo vertexBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vertexBufferCreateInfo.size = sizeof(PerVertex) * _mesh.verticies.size();
    vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vertexBufferAllocationCreateInfo = {};
    vertexBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check_success(vmaCreateBuffer(d.allocator, &vertexBufferCreateInfo, &vertexBufferAllocationCreateInfo, &d.vertexBuffer, &d.vertexMemory, nullptr));
}

void Renderer::begin_data_upload()
{  
    void *pData;
    check_success(vmaMapMemory(d.allocator, d.stagingMemory, &pData));

    constexpr VkDeviceSize lightingOffset = 0;
    constexpr VkDeviceSize lightingSize = sizeof(LightingUniforms);
    constexpr VkBufferCopy lightingRegion { lightingOffset, 0, lightingSize };

    LightingUniforms lightingData;
    lightingData.lights[0].position = { 2.0f, 2.0f, 0.0f };
    lightingData.lights[0].color = { 1.0f, 1.0f, 1.0f };
    lightingData.lights[0].power = 40.0f;
    lightingData.materials[0].ambient = { 0.1, 0.1, 0.1 };
    lightingData.materials[0].diffuse = { 0.5, 0.3, 0.3 };
    lightingData.materials[0].specular = { 1.0, 1.0, 1.0 };
    lightingData.materials[0].shininess = 16.0f;

    memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pData) + lightingOffset), &lightingData, lightingSize);

    const VkDeviceSize vertexOffset = lightingOffset + lightingSize;
    const VkDeviceSize vertexSize = sizeof(PerVertex) * _mesh.verticies.size();
    const VkBufferCopy vertexRegion { vertexOffset, 0, vertexSize };

    memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pData) + vertexOffset), _mesh.verticies.data(), vertexSize);

    const VkDeviceSize finalOffset = vertexOffset + vertexSize;
    if (finalOffset > STAGING_BUFFER_SIZE)
    {
        throw std::runtime_error("Out of GPU memory");
    }

    vmaUnmapMemory(d.allocator, d.stagingMemory);

    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    check_success(vkBeginCommandBuffer(d.uploadCommandBuffer, &commandBufferBeginInfo));
        vkCmdCopyBuffer(d.uploadCommandBuffer, d.stagingBuffer, d.lightingUniformBuffer, 1, &lightingRegion);
        vkCmdCopyBuffer(d.uploadCommandBuffer, d.stagingBuffer, d.vertexBuffer, 1, &vertexRegion);
    check_success(vkEndCommandBuffer(d.uploadCommandBuffer));

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &d.uploadCommandBuffer;
    check_success(vkQueueSubmit(queue, 1, &submitInfo, d.uploadFence));
}

void Renderer::create_common()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    check_success(vkCreateCommandPool(d.device, &commandPoolCreateInfo, nullptr, &d.commandPool));

    std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetBindings = {};
    descriptorSetBindings[0].binding = 0;
    descriptorSetBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorSetBindings[0].descriptorCount = 1;
    descriptorSetBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descriptorSetBindings[1].binding = 1;
    descriptorSetBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetBindings[1].descriptorCount = 1;
    descriptorSetBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutCreateInfo.bindingCount = descriptorSetBindings.size();
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetBindings.data();
    check_success(vkCreateDescriptorSetLayout(d.device, &descriptorSetLayoutCreateInfo, nullptr, &d.descriptorSetLayout));

    constexpr VkPushConstantRange pushConstantRange { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &d.descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    check_success(vkCreatePipelineLayout(d.device, &pipelineLayoutCreateInfo, nullptr, &d.pipelineLayout));

    constexpr VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    check_success(vkCreateSemaphore(d.device, &semaphoreCreateInfo, nullptr, &d.acquireCompleteSemaphore));

    std::vector<uint32_t> fragmentShaderData = load_shader("main.frag");
    VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    fragmentShaderModuleCreateInfo.codeSize = fragmentShaderData.size() * sizeof(uint32_t);
    fragmentShaderModuleCreateInfo.pCode = fragmentShaderData.data();

    check_success(vkCreateShaderModule(d.device, &fragmentShaderModuleCreateInfo, nullptr, &d.fragmentModule));

    std::vector<uint32_t> vertexShaderData = load_shader("main.vert");
    VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vertexShaderModuleCreateInfo.codeSize = vertexShaderData.size() * sizeof(uint32_t);
    vertexShaderModuleCreateInfo.pCode = vertexShaderData.data();

    check_success(vkCreateShaderModule(d.device, &vertexShaderModuleCreateInfo, nullptr, &d.vertexModule));

    const auto pipelineCacheData = load_file(PIPELINE_CACHE_FILENAME);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    pipelineCacheCreateInfo.initialDataSize = pipelineCacheData.size();
    pipelineCacheCreateInfo.pInitialData = pipelineCacheData.data();

    check_success(vkCreatePipelineCache(d.device, &pipelineCacheCreateInfo, nullptr, &d.pipelineCache));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = d.commandPool;
    commandBufferAllocateInfo.commandBufferCount = RENDERER_MAX_FRAMES_IN_FLIGHT;

    std::array<VkCommandBuffer, RENDERER_MAX_FRAMES_IN_FLIGHT> commandBuffers;
    check_success(vkAllocateCommandBuffers(d.device, &commandBufferAllocateInfo, commandBuffers.data()));

    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < RENDERER_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        auto& perFence = d.perFrameData[i];

        perFence.commandBuffer = commandBuffers[i];

        check_success(vkCreateFence(d.device, &fenceCreateInfo, nullptr, &perFence.fence));
    }
}

void Renderer::create_descriptors()
{
    constexpr std::array<VkDescriptorPoolSize, 2> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1}
    }};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    check_success(vkCreateDescriptorPool(d.device, &descriptorPoolCreateInfo, nullptr, &d.descriptorPool));

    VkDescriptorSetAllocateInfo descriptorAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorAllocateInfo.descriptorPool = d.descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &d.descriptorSetLayout;

    check_success(vkAllocateDescriptorSets(d.device, &descriptorAllocateInfo, &d.descriptorSet));

    const VkDescriptorBufferInfo transformUniformBufferInfo = { d.transformUniformBuffer, 0, sizeof(TransformUniforms) };
    const VkDescriptorBufferInfo lightingUniformBufferInfo = { d.lightingUniformBuffer, 0, sizeof(LightingUniforms) };

    std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = d.descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorWrites[0].pBufferInfo = &transformUniformBufferInfo;
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = d.descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].pBufferInfo = &lightingUniformBufferInfo;
    vkUpdateDescriptorSets(d.device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void Renderer::create_pipeline()
{
    surfaceFormat = select_format(physicalDevice, d.surface);

    std::array<VkAttachmentDescription, 2> attachmentDescriptions = {};
    attachmentDescriptions[0].format = surfaceFormat.format;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescriptions[1].format = DEPTH_FORMAT;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 1> colorAttachmentRefs = {};
    colorAttachmentRefs[0].attachment = 0;
    colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkSubpassDescription, 1> subpasses = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = colorAttachmentRefs.size();
    subpasses[0].pColorAttachments = colorAttachmentRefs.data();
    subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 4> subpassDependencies = {};
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask = 0;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstAccessMask = 0;
    subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[2].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[2].dstSubpass = 0;
    subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependencies[2].srcAccessMask = 0;
    subpassDependencies[2].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[3].srcSubpass = 0;
    subpassDependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[3].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[3].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependencies[3].dstAccessMask = 0;
    subpassDependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.attachmentCount = attachmentDescriptions.size();
    renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
    renderPassCreateInfo.subpassCount = subpasses.size();
    renderPassCreateInfo.pSubpasses = subpasses.data();
    renderPassCreateInfo.dependencyCount = subpassDependencies.size();
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    check_success(vkCreateRenderPass(d.device, &renderPassCreateInfo, nullptr, &d.renderPass));

    std::array<VkSpecializationMapEntry, 2> fragmentSpecMap = {};
    fragmentSpecMap[0].constantID = 0;
    fragmentSpecMap[0].offset = offsetof(SpecConstants, maxMaterials);
    fragmentSpecMap[0].size = sizeof(SpecConstants::maxMaterials);
    fragmentSpecMap[1].constantID = 1;
    fragmentSpecMap[1].offset = offsetof(SpecConstants, numLights);
    fragmentSpecMap[1].size = sizeof(SpecConstants::numLights);

    constexpr SpecConstants fragmentSpecData = { MAX_MATERIALS, NUM_LIGHTS };

    const VkSpecializationInfo fragmentSpecInfo = {
        fragmentSpecMap.size(), fragmentSpecMap.data(),
        sizeof(fragmentSpecData), &fragmentSpecData
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = d.vertexModule;
    shaderStages[0].pName = "main";
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = d.fragmentModule;
    shaderStages[1].pName = "main";
    shaderStages[1].pSpecializationInfo = &fragmentSpecInfo;

    std::array<VkVertexInputBindingDescription, 1> vertexBindings = {};
    vertexBindings[0].binding = 0;
    vertexBindings[0].stride = sizeof(PerVertex);
    vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> vertexAttributes = {};
    vertexAttributes[0].location = 0;
    vertexAttributes[0].binding = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[0].offset = offsetof(PerVertex, position);
    vertexAttributes[1].location = 1;
    vertexAttributes[1].binding = 0;
    vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributes[1].offset = offsetof(PerVertex, normal);

    VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputState.vertexBindingDescriptionCount = vertexBindings.size();
    vertexInputState.pVertexBindingDescriptions = vertexBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = vertexAttributes.size();
    vertexInputState.pVertexAttributeDescriptions = vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    std::array<VkPipelineColorBlendAttachmentState, 1> colorAttachmentStates = {};
    colorAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.attachmentCount = colorAttachmentStates.size();
    colorBlendState.pAttachments = colorAttachmentStates.data();

    constexpr std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = d.pipelineLayout;
    pipelineCreateInfo.renderPass = d.renderPass;
    pipelineCreateInfo.subpass = 0;

    check_success(vkCreateGraphicsPipelines(d.device, d.pipelineCache, 1, &pipelineCreateInfo, nullptr, &d.pipeline));
}

void Renderer::create_swapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    check_success(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, d.surface, &surfaceCaps));

    auto minImageCount = std::max(surfaceCaps.minImageCount + 1, DEFAULT_IMAGE_COUNT);
    if (surfaceCaps.maxImageCount)
    {
        minImageCount = std::min(minImageCount, surfaceCaps.maxImageCount);
    }

    surfaceExtent = surfaceCaps.currentExtent;
    if ((0 == surfaceExtent.width && 0 == surfaceExtent.height) || (-1 == surfaceExtent.width && -1 == surfaceExtent.height))
    {
        throw std::runtime_error("Bad surface extent");
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha;
    if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    else if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    {
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else
    {
        throw std::runtime_error("Bad composite alpha");
    }

    const auto presentMode = select_present_mode(physicalDevice, d.surface);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCreateInfo.surface = d.surface;
    swapchainCreateInfo.minImageCount = minImageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = surfaceExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
    swapchainCreateInfo.compositeAlpha = compositeAlpha;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;

    check_success(vkCreateSwapchainKHR(d.device, &swapchainCreateInfo, nullptr, &d.swapchain));

    uint32_t numSwapchainImages;
    check_success(vkGetSwapchainImagesKHR(d.device, d.swapchain, &numSwapchainImages, nullptr));
    std::vector<VkImage> swapchainImages(numSwapchainImages);
    check_success(vkGetSwapchainImagesKHR(d.device, d.swapchain, &numSwapchainImages, swapchainImages.data()));

    VkImageCreateInfo depthImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.format = DEPTH_FORMAT;
    depthImageCreateInfo.extent = { surfaceExtent.width, surfaceExtent.height, 1 };
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocationCreateInfo = {};
    depthAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    depthAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // TODO: Lazy allocate if available

    check_success(vmaCreateImage(d.allocator, &depthImageCreateInfo, &depthAllocationCreateInfo, &d.depthImage, &d.depthMemory, nullptr));

    VkImageViewCreateInfo depthImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    depthImageViewCreateInfo.image = d.depthImage;
    depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = DEPTH_FORMAT;
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;
    depthImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;

    check_success(vkCreateImageView(d.device, &depthImageViewCreateInfo, nullptr, &d.depthView));

    d.perImageData.resize(numSwapchainImages);
    for (size_t i = 0; i < numSwapchainImages; ++i)
    {
        auto& imageData = d.perImageData[i];

        constexpr VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        check_success(vkCreateSemaphore(d.device, &semaphoreCreateInfo, nullptr, &imageData.renderCompleteSemaphore));

        imageData.image = swapchainImages[i];

        VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = imageData.image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        check_success(vkCreateImageView(d.device, &imageViewCreateInfo, nullptr, &imageData.imageView));

        std::array<VkImageView, 2> framebufferAttachments = { imageData.imageView, d.depthView };

        VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferCreateInfo.renderPass = d.renderPass;
        framebufferCreateInfo.attachmentCount = framebufferAttachments.size();
        framebufferCreateInfo.pAttachments = framebufferAttachments.data();
        framebufferCreateInfo.width = surfaceExtent.width;
        framebufferCreateInfo.height = surfaceExtent.height;
        framebufferCreateInfo.layers = 1;

        check_success(vkCreateFramebuffer(d.device, &framebufferCreateInfo, nullptr, &imageData.framebuffer));
    }
}

void Renderer::finish_data_upload()
{
    check_success(vkWaitForFences(d.device, 1, &d.uploadFence, VK_TRUE, UINT64_MAX));
}

void Renderer::recreate_swapchain()
{
    std::array<VkFence, RENDERER_MAX_FRAMES_IN_FLIGHT> fences;
    for (uint32_t i = 0; i < RENDERER_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        fences[i] = d.perFrameData[i].fence;
    }
    check_success(vkWaitForFences(d.device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX));

    destroy_swapchain();

    create_swapchain();
}

void Renderer::record_command_buffer(uint32_t frameIndex, uint32_t imageIndex, const Scene& scene)
{
    const auto& frameData = d.perFrameData[frameIndex];
    const auto& imageData = d.perImageData[imageIndex];

    constexpr VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = d.renderPass;
    renderPassBeginInfo.framebuffer = imageData.framebuffer;
    renderPassBeginInfo.renderArea = {{}, surfaceExtent};
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    const uint32_t uniformOffset = frameIndex * sizeof(TransformUniforms);

    const std::array<VkRect2D, 1> scissors = {{
        {{0, 0}, surfaceExtent}
    }};

    const std::array<VkViewport, 1> viewports = {{
        { 0.0f, 0.0f, static_cast<float>(surfaceExtent.width), static_cast<float>(surfaceExtent.height), 0.0f, 1.0f }
    }};

    const std::array<VkBuffer, 1> vertexBuffers = { d.vertexBuffer };
    const std::array<VkDeviceSize, 1> vertexOffsets = { 0 };
    static_assert(vertexBuffers.size() == vertexOffsets.size());

    const auto modelMatrix = glm::translate(scene.modelLocation) * glm::mat4_cast(scene.modelRotation);

    constexpr glm::vec3 cameraUp{ 0, 1, 0 };
    const auto viewMatrix = glm::lookAt(scene.cameraLocation, scene.modelLocation, cameraUp);

    const auto modelViewMatrix = viewMatrix * modelMatrix;

    // TODO: Depth clamp or use a non-infinite perspective
    auto projectionMatrix = glm::infinitePerspective(FIELD_OF_VIEW, viewports[0].width / viewports[0].height, NEAR_CLIP_PLANE);
    projectionMatrix[1][1] *= -1; // Correct for OriginUpperLeft (Vulkan) vs OriginLowerLeft (GLM)

    const auto normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));

    const TransformUniforms uniforms { modelViewMatrix, projectionMatrix, normalMatrix };

    void *pData;
    vmaMapMemory(d.allocator, d.transformUniformMemory, &pData);
        memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pData) + uniformOffset), &uniforms, sizeof(TransformUniforms));
        vmaFlushAllocation(d.allocator, d.transformUniformMemory, uniformOffset, sizeof(TransformUniforms));
    vmaUnmapMemory(d.allocator, d.transformUniformMemory);

    constexpr PushConstants pushConstants = { 0 };

    check_success(vkResetCommandBuffer(frameData.commandBuffer, 0));
    check_success(vkBeginCommandBuffer(frameData.commandBuffer, &commandBufferBeginInfo));

    vkCmdSetScissor(frameData.commandBuffer, 0, scissors.size(), scissors.data());
    vkCmdSetViewport(frameData.commandBuffer, 0, viewports.size(), viewports.data());

    vkCmdBeginRenderPass(frameData.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindDescriptorSets(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, d.pipelineLayout, 0, 1, &d.descriptorSet, 1, &uniformOffset);
        vkCmdBindPipeline(frameData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, d.pipeline);
        vkCmdBindVertexBuffers(frameData.commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexOffsets.data());

        vkCmdPushConstants(frameData.commandBuffer, d.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
        
        vkCmdDraw(frameData.commandBuffer, _mesh.verticies.size(), 1, 0, 0);

    vkCmdEndRenderPass(frameData.commandBuffer);

    check_success(vkEndCommandBuffer(frameData.commandBuffer));
}