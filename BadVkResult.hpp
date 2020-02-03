#pragma once

#include <vulkan/vulkan.h>

#include <exception>

struct BadVkResult : public std::exception
{
public:
    BadVkResult(VkResult vkResult);

    const char *what() const noexcept final;
private:
    const char *_what;
};
