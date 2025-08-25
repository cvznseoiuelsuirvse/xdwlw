#include "xdwayland-collections.h"
#include "xdwayland-structs.h"
#include <assert.h>
#include <libxml/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TO_XMLSTRING(c) (xmlChar *)c

static char *parse_signature(xmlNodePtr cur) {
  char *signature = malloc(1);
  assert(signature);

  size_t i = 0;
  for (xmlNodePtr node = cur->children; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(node->name, (const xmlChar *)"arg") == 0) {

      char *arg_type = (char *)xmlGetProp(node, TO_XMLSTRING("type"));

      if (strcmp(arg_type, "uint") == 0 || strcmp(arg_type, "new_id") == 0 ||
          strcmp(arg_type, "object") == 0) {
        signature[i++] = 'u';
        signature = realloc(signature, i + 1);
      } else if (strcmp(arg_type, "int") == 0 ||
                 strcmp(arg_type, "enum") == 0) {
        signature[i++] = 'i';
        signature = realloc(signature, i + 1);
      } else if (strcmp(arg_type, "fixed") == 0) {
        signature[i++] = 'f';
        signature = realloc(signature, i + 1);
      } else if (strcmp(arg_type, "string") == 0) {
        signature[i++] = 's';
        signature = realloc(signature, i + 1);
      } else if (strcmp(arg_type, "fd") == 0) {
        signature[i++] = 'h';
        signature = realloc(signature, i + 1);
      }
    }
  }

  signature[i] = '\0';
  return signature;
};

static void parse_method(const char *interface_name, xmlNodePtr cur,
                         xdwl_list *l, const char *type) {
  char *method_name = (char *)xmlGetProp(cur, TO_XMLSTRING("name"));
  char *method_signature;

  if (strcmp(method_name, "bind") == 0 &&
      strcmp(interface_name, "wl_registry") == 0) {
    method_signature = strdup("usuu");

  } else {
    method_signature = parse_signature(cur);
  }

  struct xdwl_method method = {.name = method_name,
                               .signature = method_signature,
                               .arg_count = strlen(method_signature)};
  xdwl_list_push(l, &method, sizeof(struct xdwl_method));
};

static void parse_interface(xmlNodePtr cur, xdwl_map *m) {
  char *interface_name = (char *)xmlGetProp(cur, TO_XMLSTRING("name"));
  struct xdwl_interface interface = {
      .name = interface_name,
      .events = xdwl_list_new(),
      .requests = xdwl_list_new(),
  };

  xdwl_map_set_str(m, interface_name, &interface,
                   sizeof(struct xdwl_interface));

  for (xmlNodePtr c = cur->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE &&
        xmlStrEqual(c->name, TO_XMLSTRING("request"))) {
      parse_method(interface_name, c, interface.requests, "request");
    } else if (c->type == XML_ELEMENT_NODE &&
               xmlStrEqual(c->name, TO_XMLSTRING("event"))) {
      parse_method(interface_name, c, interface.events, "event");
    }
  };
};

void parse(const char *xml_path, xdwl_map *m) {
  xmlDocPtr doc = xmlReadFile(xml_path, NULL, 0);
  if (doc == NULL) {
    return;
  }

  xmlNodePtr cur = xmlDocGetRootElement(doc);

  for (xmlNodePtr c = cur->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE &&
        xmlStrEqual(c->name, TO_XMLSTRING("interface"))) {
      parse_interface(c, m);
    }
  }
};
