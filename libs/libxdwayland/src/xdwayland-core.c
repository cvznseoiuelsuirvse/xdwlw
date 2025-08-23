#include "xdwayland-client.h"
#include "xdwayland-common.h"

struct xdwl_display;
void xdwl_display_add_listener(xdwl_proxy *proxy,
                               struct xdwl_display *interface,
                               void *user_data) {
  xdwl_add_listener(proxy, "wl_display", interface, user_data);
};

void xdwl_display_sync(xdwl_proxy *proxy, uint32_t _callback) {
  xdwl_send_request(proxy, "wl_display", 0, 0, 1, _callback);
};

void xdwl_display_get_registry(xdwl_proxy *proxy, uint32_t _registry) {
  xdwl_send_request(proxy, "wl_display", 1, 0, 1, _registry);
};

struct xdwl_registry;
void xdwl_registry_add_listener(xdwl_proxy *proxy,
                                struct xdwl_registry *interface,
                                void *user_data) {
  xdwl_add_listener(proxy, "wl_registry", interface, user_data);
};

void xdwl_registry_bind(xdwl_proxy *proxy, uint32_t _name,
                        const char *_interface, uint32_t _version,
                        uint32_t _new_id) {
  xdwl_send_request(proxy, "wl_registry", 0, 0, 4, _name, _interface, _version,
                    _new_id);
};

struct xdwl_callback;
void xdwl_callback_add_listener(xdwl_proxy *proxy,
                                struct xdwl_callback *interface,
                                void *user_data) {
  xdwl_add_listener(proxy, "wl_callback", interface, user_data);
};

void xdwl_compositor_create_surface(xdwl_proxy *proxy, uint32_t _id) {
  xdwl_send_request(proxy, "wl_compositor", 0, 0, 1, _id);
};

void xdwl_compositor_create_region(xdwl_proxy *proxy, uint32_t _id) {
  xdwl_send_request(proxy, "wl_compositor", 1, 0, 1, _id);
};

void xdwl_shm_pool_create_buffer(xdwl_proxy *proxy, uint32_t _id,
                                 int32_t _offset, int32_t _width,
                                 int32_t _height, int32_t _stride,
                                 uint32_t _format) {
  xdwl_send_request(proxy, "wl_shm_pool", 0, 0, 6, _id, _offset, _width,
                    _height, _stride, _format);
};

void xdwl_shm_pool_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_shm_pool", 1, 0, 0);
};

void xdwl_shm_pool_resize(xdwl_proxy *proxy, int32_t _size) {
  xdwl_send_request(proxy, "wl_shm_pool", 2, 0, 1, _size);
};

struct xdwl_shm;
void xdwl_shm_add_listener(xdwl_proxy *proxy, struct xdwl_shm *interface,
                           void *user_data) {
  xdwl_add_listener(proxy, "wl_shm", interface, user_data);
};

void xdwl_shm_create_pool(xdwl_proxy *proxy, uint32_t _id, int _fd,
                          int32_t _size) {
  xdwl_send_request(proxy, "wl_shm", 0, _fd, 2, _id, _size);
};

void xdwl_shm_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_shm", 1, 0, 0);
};

struct xdwl_buffer;
void xdwl_buffer_add_listener(xdwl_proxy *proxy, struct xdwl_buffer *interface,
                              void *user_data) {
  xdwl_add_listener(proxy, "wl_buffer", interface, user_data);
};

void xdwl_buffer_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_buffer", 0, 0, 0);
};

struct xdwl_data_offer;
void xdwl_data_offer_add_listener(xdwl_proxy *proxy,
                                  struct xdwl_data_offer *interface,
                                  void *user_data) {
  xdwl_add_listener(proxy, "wl_data_offer", interface, user_data);
};

void xdwl_data_offer_accept(xdwl_proxy *proxy, uint32_t _serial,
                            const char *_mime_type) {
  xdwl_send_request(proxy, "wl_data_offer", 0, 0, 2, _serial, _mime_type);
};

void xdwl_data_offer_receive(xdwl_proxy *proxy, const char *_mime_type,
                             int _fd) {
  xdwl_send_request(proxy, "wl_data_offer", 1, _fd, 1, _mime_type);
};

void xdwl_data_offer_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_data_offer", 2, 0, 0);
};

void xdwl_data_offer_finish(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_data_offer", 3, 0, 0);
};

void xdwl_data_offer_set_actions(xdwl_proxy *proxy, uint32_t _dnd_actions,
                                 uint32_t _preferred_action) {
  xdwl_send_request(proxy, "wl_data_offer", 4, 0, 2, _dnd_actions,
                    _preferred_action);
};

struct xdwl_data_source;
void xdwl_data_source_add_listener(xdwl_proxy *proxy,
                                   struct xdwl_data_source *interface,
                                   void *user_data) {
  xdwl_add_listener(proxy, "wl_data_source", interface, user_data);
};

void xdwl_data_source_offer(xdwl_proxy *proxy, const char *_mime_type) {
  xdwl_send_request(proxy, "wl_data_source", 0, 0, 1, _mime_type);
};

void xdwl_data_source_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_data_source", 1, 0, 0);
};

void xdwl_data_source_set_actions(xdwl_proxy *proxy, uint32_t _dnd_actions) {
  xdwl_send_request(proxy, "wl_data_source", 2, 0, 1, _dnd_actions);
};

struct xdwl_data_device;
void xdwl_data_device_add_listener(xdwl_proxy *proxy,
                                   struct xdwl_data_device *interface,
                                   void *user_data) {
  xdwl_add_listener(proxy, "wl_data_device", interface, user_data);
};

void xdwl_data_device_start_drag(xdwl_proxy *proxy, uint32_t _source,
                                 uint32_t _origin, uint32_t _icon,
                                 uint32_t _serial) {
  xdwl_send_request(proxy, "wl_data_device", 0, 0, 4, _source, _origin, _icon,
                    _serial);
};

void xdwl_data_device_set_selection(xdwl_proxy *proxy, uint32_t _source,
                                    uint32_t _serial) {
  xdwl_send_request(proxy, "wl_data_device", 1, 0, 2, _source, _serial);
};

void xdwl_data_device_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_data_device", 2, 0, 0);
};

void xdwl_data_device_manager_create_data_source(xdwl_proxy *proxy,
                                                 uint32_t _id) {
  xdwl_send_request(proxy, "wl_data_device_manager", 0, 0, 1, _id);
};

void xdwl_data_device_manager_get_data_device(xdwl_proxy *proxy, uint32_t _id,
                                              uint32_t _seat) {
  xdwl_send_request(proxy, "wl_data_device_manager", 1, 0, 2, _id, _seat);
};

void xdwl_shell_get_shell_surface(xdwl_proxy *proxy, uint32_t _id,
                                  uint32_t _surface) {
  xdwl_send_request(proxy, "wl_shell", 0, 0, 2, _id, _surface);
};

struct xdwl_shell_surface;
void xdwl_shell_surface_add_listener(xdwl_proxy *proxy,
                                     struct xdwl_shell_surface *interface,
                                     void *user_data) {
  xdwl_add_listener(proxy, "wl_shell_surface", interface, user_data);
};

void xdwl_shell_surface_pong(xdwl_proxy *proxy, uint32_t _serial) {
  xdwl_send_request(proxy, "wl_shell_surface", 0, 0, 1, _serial);
};

void xdwl_shell_surface_move(xdwl_proxy *proxy, uint32_t _seat,
                             uint32_t _serial) {
  xdwl_send_request(proxy, "wl_shell_surface", 1, 0, 2, _seat, _serial);
};

void xdwl_shell_surface_resize(xdwl_proxy *proxy, uint32_t _seat,
                               uint32_t _serial, uint32_t _edges) {
  xdwl_send_request(proxy, "wl_shell_surface", 2, 0, 3, _seat, _serial, _edges);
};

void xdwl_shell_surface_set_toplevel(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_shell_surface", 3, 0, 0);
};

void xdwl_shell_surface_set_transient(xdwl_proxy *proxy, uint32_t _parent,
                                      int32_t _x, int32_t _y, uint32_t _flags) {
  xdwl_send_request(proxy, "wl_shell_surface", 4, 0, 4, _parent, _x, _y,
                    _flags);
};

void xdwl_shell_surface_set_fullscreen(xdwl_proxy *proxy, uint32_t _method,
                                       uint32_t _framerate, uint32_t _output) {
  xdwl_send_request(proxy, "wl_shell_surface", 5, 0, 3, _method, _framerate,
                    _output);
};

void xdwl_shell_surface_set_popup(xdwl_proxy *proxy, uint32_t _seat,
                                  uint32_t _serial, uint32_t _parent,
                                  int32_t _x, int32_t _y, uint32_t _flags) {
  xdwl_send_request(proxy, "wl_shell_surface", 6, 0, 6, _seat, _serial, _parent,
                    _x, _y, _flags);
};

void xdwl_shell_surface_set_maximized(xdwl_proxy *proxy, uint32_t _output) {
  xdwl_send_request(proxy, "wl_shell_surface", 7, 0, 1, _output);
};

void xdwl_shell_surface_set_title(xdwl_proxy *proxy, const char *_title) {
  xdwl_send_request(proxy, "wl_shell_surface", 8, 0, 1, _title);
};

void xdwl_shell_surface_set_class(xdwl_proxy *proxy, const char *_class_) {
  xdwl_send_request(proxy, "wl_shell_surface", 9, 0, 1, _class_);
};

struct xdwl_surface;
void xdwl_surface_add_listener(xdwl_proxy *proxy,
                               struct xdwl_surface *interface,
                               void *user_data) {
  xdwl_add_listener(proxy, "wl_surface", interface, user_data);
};

void xdwl_surface_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_surface", 0, 0, 0);
};

void xdwl_surface_attach(xdwl_proxy *proxy, uint32_t _buffer, int32_t _x,
                         int32_t _y) {
  xdwl_send_request(proxy, "wl_surface", 1, 0, 3, _buffer, _x, _y);
};

void xdwl_surface_damage(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                         int32_t _width, int32_t _height) {
  xdwl_send_request(proxy, "wl_surface", 2, 0, 4, _x, _y, _width, _height);
};

void xdwl_surface_frame(xdwl_proxy *proxy, uint32_t _callback) {
  xdwl_send_request(proxy, "wl_surface", 3, 0, 1, _callback);
};

void xdwl_surface_set_opaque_region(xdwl_proxy *proxy, uint32_t _region) {
  xdwl_send_request(proxy, "wl_surface", 4, 0, 1, _region);
};

void xdwl_surface_set_input_region(xdwl_proxy *proxy, uint32_t _region) {
  xdwl_send_request(proxy, "wl_surface", 5, 0, 1, _region);
};

void xdwl_surface_commit(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_surface", 6, 0, 0);
};

void xdwl_surface_set_buffer_transform(xdwl_proxy *proxy, int32_t _transform) {
  xdwl_send_request(proxy, "wl_surface", 7, 0, 1, _transform);
};

void xdwl_surface_set_buffer_scale(xdwl_proxy *proxy, int32_t _scale) {
  xdwl_send_request(proxy, "wl_surface", 8, 0, 1, _scale);
};

void xdwl_surface_damage_buffer(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                                int32_t _width, int32_t _height) {
  xdwl_send_request(proxy, "wl_surface", 9, 0, 4, _x, _y, _width, _height);
};

void xdwl_surface_offset(xdwl_proxy *proxy, int32_t _x, int32_t _y) {
  xdwl_send_request(proxy, "wl_surface", 10, 0, 2, _x, _y);
};

struct xdwl_seat;
void xdwl_seat_add_listener(xdwl_proxy *proxy, struct xdwl_seat *interface,
                            void *user_data) {
  xdwl_add_listener(proxy, "wl_seat", interface, user_data);
};

void xdwl_seat_get_pointer(xdwl_proxy *proxy, uint32_t _id) {
  xdwl_send_request(proxy, "wl_seat", 0, 0, 1, _id);
};

void xdwl_seat_get_keyboard(xdwl_proxy *proxy, uint32_t _id) {
  xdwl_send_request(proxy, "wl_seat", 1, 0, 1, _id);
};

void xdwl_seat_get_touch(xdwl_proxy *proxy, uint32_t _id) {
  xdwl_send_request(proxy, "wl_seat", 2, 0, 1, _id);
};

void xdwl_seat_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_seat", 3, 0, 0);
};

struct xdwl_pointer;
void xdwl_pointer_add_listener(xdwl_proxy *proxy,
                               struct xdwl_pointer *interface,
                               void *user_data) {
  xdwl_add_listener(proxy, "wl_pointer", interface, user_data);
};

void xdwl_pointer_set_cursor(xdwl_proxy *proxy, uint32_t _serial,
                             uint32_t _surface, int32_t _hotspot_x,
                             int32_t _hotspot_y) {
  xdwl_send_request(proxy, "wl_pointer", 0, 0, 4, _serial, _surface, _hotspot_x,
                    _hotspot_y);
};

void xdwl_pointer_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_pointer", 1, 0, 0);
};

struct xdwl_keyboard;
void xdwl_keyboard_add_listener(xdwl_proxy *proxy,
                                struct xdwl_keyboard *interface,
                                void *user_data) {
  xdwl_add_listener(proxy, "wl_keyboard", interface, user_data);
};

void xdwl_keyboard_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_keyboard", 0, 0, 0);
};

struct xdwl_touch;
void xdwl_touch_add_listener(xdwl_proxy *proxy, struct xdwl_touch *interface,
                             void *user_data) {
  xdwl_add_listener(proxy, "wl_touch", interface, user_data);
};

void xdwl_touch_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_touch", 0, 0, 0);
};

struct xdwl_output;
void xdwl_output_add_listener(xdwl_proxy *proxy, struct xdwl_output *interface,
                              void *user_data) {
  xdwl_add_listener(proxy, "wl_output", interface, user_data);
};

void xdwl_output_release(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_output", 0, 0, 0);
};

void xdwl_region_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_region", 0, 0, 0);
};

void xdwl_region_add(xdwl_proxy *proxy, int32_t _x, int32_t _y, int32_t _width,
                     int32_t _height) {
  xdwl_send_request(proxy, "wl_region", 1, 0, 4, _x, _y, _width, _height);
};

void xdwl_region_subtract(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                          int32_t _width, int32_t _height) {
  xdwl_send_request(proxy, "wl_region", 2, 0, 4, _x, _y, _width, _height);
};

void xdwl_subcompositor_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_subcompositor", 0, 0, 0);
};

void xdwl_subcompositor_get_subsurface(xdwl_proxy *proxy, uint32_t _id,
                                       uint32_t _surface, uint32_t _parent) {
  xdwl_send_request(proxy, "wl_subcompositor", 1, 0, 3, _id, _surface, _parent);
};

void xdwl_subsurface_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_subsurface", 0, 0, 0);
};

void xdwl_subsurface_set_position(xdwl_proxy *proxy, int32_t _x, int32_t _y) {
  xdwl_send_request(proxy, "wl_subsurface", 1, 0, 2, _x, _y);
};

void xdwl_subsurface_place_above(xdwl_proxy *proxy, uint32_t _sibling) {
  xdwl_send_request(proxy, "wl_subsurface", 2, 0, 1, _sibling);
};

void xdwl_subsurface_place_below(xdwl_proxy *proxy, uint32_t _sibling) {
  xdwl_send_request(proxy, "wl_subsurface", 3, 0, 1, _sibling);
};

void xdwl_subsurface_set_sync(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_subsurface", 4, 0, 0);
};

void xdwl_subsurface_set_desync(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_subsurface", 5, 0, 0);
};

void xdwl_fixes_destroy(xdwl_proxy *proxy) {
  xdwl_send_request(proxy, "wl_fixes", 0, 0, 0);
};

void xdwl_fixes_destroy_registry(xdwl_proxy *proxy, uint32_t _registry) {
  xdwl_send_request(proxy, "wl_fixes", 1, 0, 1, _registry);
};
