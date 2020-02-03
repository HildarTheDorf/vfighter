#pragma once

#include <xcb/xcb.h>

#include <atomic>
#include <memory>

class Window
{
public:
    explicit Window(std::string_view window_name);

    bool closed() const noexcept;
    xcb_connection_t *connection() const noexcept;
    void poll_event();
    xcb_window_t window() const noexcept;

private:
    struct ConnectionDeleter
    {
        void operator()(xcb_connection_t *c) const noexcept;
    };

    struct FreeDeleter
    {
        void operator()(void *ptr) const noexcept;
    };

    std::unique_ptr<xcb_connection_t, ConnectionDeleter> _connection;
    xcb_window_t _window;
    xcb_atom_t _wm_delete_window_atom, _wm_protocols_atom;
    std::atomic_bool _closed;
};
