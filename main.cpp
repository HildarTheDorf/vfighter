#define VK_USE_PLATFORM_XCB_KHR
#include "Renderer.hpp"
#include "Window.hpp"

#include <thread>

void renderer_entry(const Window *window)
try
{
    Renderer renderer(window->connection(), window->window());

    while (!window->closed())
    {
        renderer.render();
    }

    renderer.save_caches();
}
catch (const std::exception& e)
{
    printf("Error: Renderer Crashed with '%s'\n", e.what());
    std::terminate();
}
catch (...)
{
    puts("Error: Renderer Crashed!");
    std::terminate();
}

int main()
{
    Window window("vfighter");
    std::thread renderer_thread(renderer_entry, &window);

    while (!window.closed())
    {
        window.poll_event();
    }

    renderer_thread.join();

    return EXIT_SUCCESS;
}