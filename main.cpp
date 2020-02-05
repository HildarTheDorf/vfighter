#include "Renderer.hpp"
#include "Window.hpp"

#include <chrono>
#include <mutex>
#include <queue>
#include <thread>

constexpr auto FRAME_DURATION = std::chrono::milliseconds(10);

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

static bool process_events(Scene& scene)
{
    std::unique_ptr<const Event> event;
    while (event = pop_event())
    {
        switch (event->type())
        {
        case EventType::Quit:
            return false;
        default:
            puts("Unhandled Event");
            break;
        }
    }
    return true;
}

static void update_scene(Scene& scene)
{
    static uint8_t timer = 0; // HACK

    constexpr glm::vec3 rotationAxis = { 0, 1, 0 };
    scene.modelRotation = glm::angleAxis(timer++ * glm::radians(360.0f) / UINT8_MAX, rotationAxis);
}

static void renderer_loop(Renderer& renderer, const Window& window)
{
    Scene scene = {};
    scene.cameraLocation = { 0, 3, 0 };
    scene.modelLocation = { 0, 0, 5 };

    std::chrono::high_resolution_clock clock;
    auto lastFrameTime = clock.now();

    while (process_events(scene))
    {
        renderer.render(scene);
        auto currentTime = clock.now();
        if (currentTime > lastFrameTime + FRAME_DURATION)
        {
            lastFrameTime += FRAME_DURATION;
            update_scene(scene);
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