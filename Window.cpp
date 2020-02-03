#include "Window.hpp"

#include <climits>
#include <cstring>

constexpr char WM_DELETE_WINDOW_NAME[] = "WM_DELETE_WINDOW";
constexpr char WM_PROTOCOLS_NAME[] = "WM_PROTOCOLS";

Window::Window(std::string_view window_name)
{
    int screen_id;
    _connection.reset(xcb_connect(nullptr, &screen_id));

    const auto wm_delete_window_intern_cookie = xcb_intern_atom(_connection.get(), false, strlen(WM_DELETE_WINDOW_NAME), WM_DELETE_WINDOW_NAME);
    const auto wm_protocols_intern_cookie = xcb_intern_atom(_connection.get(), false, strlen(WM_PROTOCOLS_NAME), WM_PROTOCOLS_NAME);

    const auto setup = xcb_get_setup(_connection.get());

    auto screen = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_id; ++i)
    {
        xcb_screen_next(&screen);
    }

    _window = xcb_generate_id(_connection.get());
    xcb_create_window(
        _connection.get(),
        screen.data->root_depth,
        _window,
        screen.data->root,
        0, 0, 1600, 900, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen.data->root_visual,
        0, nullptr
    );

    xcb_change_property(_connection.get(),
        XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING, CHAR_BIT,
        window_name.size(), window_name.data());

    std::unique_ptr<xcb_intern_atom_reply_t, FreeDeleter> wm_delete_window_intern_reply{
        xcb_intern_atom_reply(_connection.get(), wm_delete_window_intern_cookie, nullptr)
    };
    _wm_delete_window_atom = wm_delete_window_intern_reply->atom;

    std::unique_ptr<xcb_intern_atom_reply_t, FreeDeleter> wm_protocols_intern_reply{
        xcb_intern_atom_reply(_connection.get(), wm_protocols_intern_cookie, nullptr)
    };
    _wm_protocols_atom = wm_protocols_intern_reply->atom;

    xcb_change_property(_connection.get(),
        XCB_PROP_MODE_REPLACE, _window, _wm_protocols_atom,
        XCB_ATOM_ATOM, sizeof(xcb_atom_t) * CHAR_BIT,
        1, &_wm_delete_window_atom);
    
    xcb_map_window(_connection.get(), _window);
}

xcb_connection_t *Window::connection() const noexcept
{
    return _connection.get();
}

std::unique_ptr<Event> Window::poll_event()
{
    std::unique_ptr<xcb_generic_event_t, FreeDeleter> event{ xcb_wait_for_event(_connection.get()) };
    switch (event->response_type & ~0x80)
    {
    case XCB_CLIENT_MESSAGE: {
        const auto clientMessage = reinterpret_cast<const xcb_client_message_event_t*>(event.get());
        if (_window == clientMessage->window
            && _wm_protocols_atom == clientMessage->type
            && _wm_delete_window_atom == clientMessage->data.data32[0])
        {
            return std::unique_ptr<Event>(new QuitEvent);
        }
        break;
    }
    default:
        break;
    }
    return nullptr;
}

xcb_window_t Window::window() const noexcept
{
    return _window;
}

void Window::ConnectionDeleter::operator()(xcb_connection_t *c) const noexcept
{
    xcb_disconnect(c);
}

void Window::FreeDeleter::operator()(void *ptr) const noexcept
{
    free(ptr);
}