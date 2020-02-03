#include "BadVkResult.hpp"

BadVkResult::BadVkResult(VkResult vkResult)
{
    switch (vkResult)
    {
    case VK_SUCCESS:
        _what = "Success";
        break;
    default:
        _what = "Bad VkResult";
        break;
    }
}

const char *BadVkResult::what() const noexcept
{
    return _what;
}
