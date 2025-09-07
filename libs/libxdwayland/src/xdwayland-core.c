#include "xdwayland-private.h"

struct xdwl_display;
int xdwl_display_add_listener(xdwl_proxy *proxy, struct xdwl_display *interface,
                              void *user_data) {
  return xdwl_add_listener(proxy, "wl_display", interface, user_data);
};

int xdwl_display_sync(xdwl_proxy *proxy, uint32_t _callback) {
  return xdwl_send_request(proxy, "wl_display", 0, 1, _callback);
};

int xdwl_display_get_registry(xdwl_proxy *proxy, uint32_t _registry) {
  return xdwl_send_request(proxy, "wl_display", 1, 1, _registry);
};

struct xdwl_registry;
int xdwl_registry_add_listener(xdwl_proxy *proxy,
                               struct xdwl_registry *interface,
                               void *user_data) {
  return xdwl_add_listener(proxy, "wl_registry", interface, user_data);
};

int xdwl_registry_bind(xdwl_proxy *proxy, uint32_t _name,
                       const char *_interface, uint32_t _version,
                       uint32_t _new_id) {
  return xdwl_send_request(proxy, "wl_registry", 0, 4, _name, _interface,
                           _version, _new_id);
};

struct xdwl_callback;
int xdwl_callback_add_listener(xdwl_proxy *proxy,
                               struct xdwl_callback *interface,
                               void *user_data) {
  return xdwl_add_listener(proxy, "wl_callback", interface, user_data);
};

int xdwl_compositor_create_surface(xdwl_proxy *proxy, uint32_t _id) {
  return xdwl_send_request(proxy, "wl_compositor", 0, 1, _id);
};

int xdwl_compositor_create_region(xdwl_proxy *proxy, uint32_t _id) {
  return xdwl_send_request(proxy, "wl_compositor", 1, 1, _id);
};

int xdwl_shm_pool_create_buffer(xdwl_proxy *proxy, uint32_t _id,
                                int32_t _offset, int32_t _width,
                                int32_t _height, int32_t _stride,
                                uint32_t _format) {
  return xdwl_send_request(proxy, "wl_shm_pool", 0, 6, _id, _offset, _width,
                           _height, _stride, _format);
};

int xdwl_shm_pool_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_shm_pool", 1, 0);
};

int xdwl_shm_pool_resize(xdwl_proxy *proxy, int32_t _size) {
  return xdwl_send_request(proxy, "wl_shm_pool", 2, 1, _size);
};

struct xdwl_shm;
int xdwl_shm_add_listener(xdwl_proxy *proxy, struct xdwl_shm *interface,
                          void *user_data) {
  return xdwl_add_listener(proxy, "wl_shm", interface, user_data);
};

int xdwl_shm_create_pool(xdwl_proxy *proxy, uint32_t _id, int _fd,
                         int32_t _size) {
  return xdwl_send_request(proxy, "wl_shm", 0, 3, _id, _fd, _size);
};

int xdwl_shm_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_shm", 1, 0);
};

struct xdwl_buffer;
int xdwl_buffer_add_listener(xdwl_proxy *proxy, struct xdwl_buffer *interface,
                             void *user_data) {
  return xdwl_add_listener(proxy, "wl_buffer", interface, user_data);
};

int xdwl_buffer_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_buffer", 0, 0);
};

struct xdwl_data_offer;
int xdwl_data_offer_add_listener(xdwl_proxy *proxy,
                                 struct xdwl_data_offer *interface,
                                 void *user_data) {
  return xdwl_add_listener(proxy, "wl_data_offer", interface, user_data);
};

int xdwl_data_offer_accept(xdwl_proxy *proxy, uint32_t _serial,
                           const char *_mime_type) {
  return xdwl_send_request(proxy, "wl_data_offer", 0, 2, _serial, _mime_type);
};

int xdwl_data_offer_receive(xdwl_proxy *proxy, const char *_mime_type,
                            int _fd) {
  return xdwl_send_request(proxy, "wl_data_offer", 1, 2, _mime_type, _fd);
};

int xdwl_data_offer_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_data_offer", 2, 0);
};

int xdwl_data_offer_finish(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_data_offer", 3, 0);
};

int xdwl_data_offer_set_actions(xdwl_proxy *proxy, uint32_t _dnd_actions,
                                uint32_t _preferred_action) {
  return xdwl_send_request(proxy, "wl_data_offer", 4, 2, _dnd_actions,
                           _preferred_action);
};

struct xdwl_data_source;
int xdwl_data_source_add_listener(xdwl_proxy *proxy,
                                  struct xdwl_data_source *interface,
                                  void *user_data) {
  return xdwl_add_listener(proxy, "wl_data_source", interface, user_data);
};

int xdwl_data_source_offer(xdwl_proxy *proxy, const char *_mime_type) {
  return xdwl_send_request(proxy, "wl_data_source", 0, 1, _mime_type);
};

int xdwl_data_source_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_data_source", 1, 0);
};

int xdwl_data_source_set_actions(xdwl_proxy *proxy, uint32_t _dnd_actions) {
  return xdwl_send_request(proxy, "wl_data_source", 2, 1, _dnd_actions);
};

struct xdwl_data_device;
int xdwl_data_device_add_listener(xdwl_proxy *proxy,
                                  struct xdwl_data_device *interface,
                                  void *user_data) {
  return xdwl_add_listener(proxy, "wl_data_device", interface, user_data);
};

int xdwl_data_device_start_drag(xdwl_proxy *proxy, uint32_t _source,
                                uint32_t _origin, uint32_t _icon,
                                uint32_t _serial) {
  return xdwl_send_request(proxy, "wl_data_device", 0, 4, _source, _origin,
                           _icon, _serial);
};

int xdwl_data_device_set_selection(xdwl_proxy *proxy, uint32_t _source,
                                   uint32_t _serial) {
  return xdwl_send_request(proxy, "wl_data_device", 1, 2, _source, _serial);
};

int xdwl_data_device_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_data_device", 2, 0);
};

int xdwl_data_device_manager_create_data_source(xdwl_proxy *proxy,
                                                uint32_t _id) {
  return xdwl_send_request(proxy, "wl_data_device_manager", 0, 1, _id);
};

int xdwl_data_device_manager_get_data_device(xdwl_proxy *proxy, uint32_t _id,
                                             uint32_t _seat) {
  return xdwl_send_request(proxy, "wl_data_device_manager", 1, 2, _id, _seat);
};

int xdwl_shell_get_shell_surface(xdwl_proxy *proxy, uint32_t _id,
                                 uint32_t _surface) {
  return xdwl_send_request(proxy, "wl_shell", 0, 2, _id, _surface);
};

struct xdwl_shell_surface;
int xdwl_shell_surface_add_listener(xdwl_proxy *proxy,
                                    struct xdwl_shell_surface *interface,
                                    void *user_data) {
  return xdwl_add_listener(proxy, "wl_shell_surface", interface, user_data);
};

int xdwl_shell_surface_pong(xdwl_proxy *proxy, uint32_t _serial) {
  return xdwl_send_request(proxy, "wl_shell_surface", 0, 1, _serial);
};

int xdwl_shell_surface_move(xdwl_proxy *proxy, uint32_t _seat,
                            uint32_t _serial) {
  return xdwl_send_request(proxy, "wl_shell_surface", 1, 2, _seat, _serial);
};

int xdwl_shell_surface_resize(xdwl_proxy *proxy, uint32_t _seat,
                              uint32_t _serial, uint32_t _edges) {
  return xdwl_send_request(proxy, "wl_shell_surface", 2, 3, _seat, _serial,
                           _edges);
};

int xdwl_shell_surface_set_toplevel(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_shell_surface", 3, 0);
};

int xdwl_shell_surface_set_transient(xdwl_proxy *proxy, uint32_t _parent,
                                     int32_t _x, int32_t _y, uint32_t _flags) {
  return xdwl_send_request(proxy, "wl_shell_surface", 4, 4, _parent, _x, _y,
                           _flags);
};

int xdwl_shell_surface_set_fullscreen(xdwl_proxy *proxy, uint32_t _method,
                                      uint32_t _framerate, uint32_t _output) {
  return xdwl_send_request(proxy, "wl_shell_surface", 5, 3, _method, _framerate,
                           _output);
};

int xdwl_shell_surface_set_popup(xdwl_proxy *proxy, uint32_t _seat,
                                 uint32_t _serial, uint32_t _parent, int32_t _x,
                                 int32_t _y, uint32_t _flags) {
  return xdwl_send_request(proxy, "wl_shell_surface", 6, 6, _seat, _serial,
                           _parent, _x, _y, _flags);
};

int xdwl_shell_surface_set_maximized(xdwl_proxy *proxy, uint32_t _output) {
  return xdwl_send_request(proxy, "wl_shell_surface", 7, 1, _output);
};

int xdwl_shell_surface_set_title(xdwl_proxy *proxy, const char *_title) {
  return xdwl_send_request(proxy, "wl_shell_surface", 8, 1, _title);
};

int xdwl_shell_surface_set_class(xdwl_proxy *proxy, const char *_class_) {
  return xdwl_send_request(proxy, "wl_shell_surface", 9, 1, _class_);
};

struct xdwl_surface;
int xdwl_surface_add_listener(xdwl_proxy *proxy, struct xdwl_surface *interface,
                              void *user_data) {
  return xdwl_add_listener(proxy, "wl_surface", interface, user_data);
};

int xdwl_surface_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_surface", 0, 0);
};

int xdwl_surface_attach(xdwl_proxy *proxy, uint32_t _buffer, int32_t _x,
                        int32_t _y) {
  return xdwl_send_request(proxy, "wl_surface", 1, 3, _buffer, _x, _y);
};

int xdwl_surface_damage(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                        int32_t _width, int32_t _height) {
  return xdwl_send_request(proxy, "wl_surface", 2, 4, _x, _y, _width, _height);
};

int xdwl_surface_frame(xdwl_proxy *proxy, uint32_t _callback) {
  return xdwl_send_request(proxy, "wl_surface", 3, 1, _callback);
};

int xdwl_surface_set_opaque_region(xdwl_proxy *proxy, uint32_t _region) {
  return xdwl_send_request(proxy, "wl_surface", 4, 1, _region);
};

int xdwl_surface_set_input_region(xdwl_proxy *proxy, uint32_t _region) {
  return xdwl_send_request(proxy, "wl_surface", 5, 1, _region);
};

int xdwl_surface_commit(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_surface", 6, 0);
};

int xdwl_surface_set_buffer_transform(xdwl_proxy *proxy, int32_t _transform) {
  return xdwl_send_request(proxy, "wl_surface", 7, 1, _transform);
};

int xdwl_surface_set_buffer_scale(xdwl_proxy *proxy, int32_t _scale) {
  return xdwl_send_request(proxy, "wl_surface", 8, 1, _scale);
};

int xdwl_surface_damage_buffer(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                               int32_t _width, int32_t _height) {
  return xdwl_send_request(proxy, "wl_surface", 9, 4, _x, _y, _width, _height);
};

int xdwl_surface_offset(xdwl_proxy *proxy, int32_t _x, int32_t _y) {
  return xdwl_send_request(proxy, "wl_surface", 10, 2, _x, _y);
};

struct xdwl_seat;
int xdwl_seat_add_listener(xdwl_proxy *proxy, struct xdwl_seat *interface,
                           void *user_data) {
  return xdwl_add_listener(proxy, "wl_seat", interface, user_data);
};

int xdwl_seat_get_pointer(xdwl_proxy *proxy, uint32_t _id) {
  return xdwl_send_request(proxy, "wl_seat", 0, 1, _id);
};

int xdwl_seat_get_keyboard(xdwl_proxy *proxy, uint32_t _id) {
  return xdwl_send_request(proxy, "wl_seat", 1, 1, _id);
};

int xdwl_seat_get_touch(xdwl_proxy *proxy, uint32_t _id) {
  return xdwl_send_request(proxy, "wl_seat", 2, 1, _id);
};

int xdwl_seat_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_seat", 3, 0);
};

struct xdwl_pointer;
int xdwl_pointer_add_listener(xdwl_proxy *proxy, struct xdwl_pointer *interface,
                              void *user_data) {
  return xdwl_add_listener(proxy, "wl_pointer", interface, user_data);
};

int xdwl_pointer_set_cursor(xdwl_proxy *proxy, uint32_t _serial,
                            uint32_t _surface, int32_t _hotspot_x,
                            int32_t _hotspot_y) {
  return xdwl_send_request(proxy, "wl_pointer", 0, 4, _serial, _surface,
                           _hotspot_x, _hotspot_y);
};

int xdwl_pointer_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_pointer", 1, 0);
};

struct xdwl_keyboard;
int xdwl_keyboard_add_listener(xdwl_proxy *proxy,
                               struct xdwl_keyboard *interface,
                               void *user_data) {
  return xdwl_add_listener(proxy, "wl_keyboard", interface, user_data);
};

int xdwl_keyboard_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_keyboard", 0, 0);
};

struct xdwl_touch;
int xdwl_touch_add_listener(xdwl_proxy *proxy, struct xdwl_touch *interface,
                            void *user_data) {
  return xdwl_add_listener(proxy, "wl_touch", interface, user_data);
};

int xdwl_touch_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_touch", 0, 0);
};

struct xdwl_output;
int xdwl_output_add_listener(xdwl_proxy *proxy, struct xdwl_output *interface,
                             void *user_data) {
  return xdwl_add_listener(proxy, "wl_output", interface, user_data);
};

int xdwl_output_release(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_output", 0, 0);
};

int xdwl_region_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_region", 0, 0);
};

int xdwl_region_add(xdwl_proxy *proxy, int32_t _x, int32_t _y, int32_t _width,
                    int32_t _height) {
  return xdwl_send_request(proxy, "wl_region", 1, 4, _x, _y, _width, _height);
};

int xdwl_region_subtract(xdwl_proxy *proxy, int32_t _x, int32_t _y,
                         int32_t _width, int32_t _height) {
  return xdwl_send_request(proxy, "wl_region", 2, 4, _x, _y, _width, _height);
};

int xdwl_subcompositor_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_subcompositor", 0, 0);
};

int xdwl_subcompositor_get_subsurface(xdwl_proxy *proxy, uint32_t _id,
                                      uint32_t _surface, uint32_t _parent) {
  return xdwl_send_request(proxy, "wl_subcompositor", 1, 3, _id, _surface,
                           _parent);
};

int xdwl_subsurface_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_subsurface", 0, 0);
};

int xdwl_subsurface_set_position(xdwl_proxy *proxy, int32_t _x, int32_t _y) {
  return xdwl_send_request(proxy, "wl_subsurface", 1, 2, _x, _y);
};

int xdwl_subsurface_place_above(xdwl_proxy *proxy, uint32_t _sibling) {
  return xdwl_send_request(proxy, "wl_subsurface", 2, 1, _sibling);
};

int xdwl_subsurface_place_below(xdwl_proxy *proxy, uint32_t _sibling) {
  return xdwl_send_request(proxy, "wl_subsurface", 3, 1, _sibling);
};

int xdwl_subsurface_set_sync(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_subsurface", 4, 0);
};

int xdwl_subsurface_set_desync(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_subsurface", 5, 0);
};

int xdwl_fixes_destroy(xdwl_proxy *proxy) {
  return xdwl_send_request(proxy, "wl_fixes", 0, 0);
};

int xdwl_fixes_destroy_registry(xdwl_proxy *proxy, uint32_t _registry) {
  return xdwl_send_request(proxy, "wl_fixes", 1, 1, _registry);
};
