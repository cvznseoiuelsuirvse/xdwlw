#ifndef OUTPUTS_H
#define OUTPUTS_H

#include "xdwayland-client.h"
#include "xdwayland-collections.h"

size_t get_outputs(xdwl_proxy *proxy, xdwl_list *globals, xdwl_list *monitors);

#endif
