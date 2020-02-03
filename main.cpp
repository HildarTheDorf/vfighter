#define VK_USE_PLATFORM_XCB_KHR
#include "Renderer.hpp"
#include "Window.hpp"

#include <mutex>
#include <queue>
#include <thread>

static std::mutex g_eventMutex;
static std::queue<std::unique_ptr<const Event>> g_eventQueue;

static std::unique_ptr<const Event> pop_event()
{
    std::lock_guard lg{g_eventMutex};
    if (!g_eventQueue.empty())
    {
        auto event = std::move(g_eventQueue.front());
        g_eventQueue.pop();
        return event;
    }
    else
    {
        return nullptr;
    }
}

static void push_event(std::unique_ptr<const Event> event)
{
    std::lock_guard lg{g_eventMutex};
    g_eventQueue.emplace(std::move(event));
}

static void renderer_loop(Renderer& renderer, const Window& window)
{
    while (true)
    {
        renderer.render();

        std::unique_ptr<const Event> event;
        while (event = pop_event())
        {
            switch (event->type())
            {
            case EventType::Quit:
                return;
            default:
                puts("Unhandled Event");
                break;
            }
        }
    }
}

static void renderer_entry(const Window *window)
try
{
    Renderer renderer(window->connection(), window->window());

    renderer_loop(renderer, *window);

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

    bool should_quit = false;
    while(!should_quit)
    {
        std::unique_ptr<Event> event;
        if (event = window.poll_event())
        {
            should_quit = EventType::Quit == event->type();
            push_event(std::move(event));
        }
    }
    renderer_thread.join();

    return EXIT_SUCCESS;
}