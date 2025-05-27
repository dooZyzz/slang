#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include "runtime/module.h"
#include "vm/vm.h"
#include "vm/object.h"

// Socket handle type
typedef struct {
    int fd;
    bool is_server;
    bool is_connected;
} SocketHandle;

// Create a socket object
static Object* create_socket_object(int fd, bool is_server) {
    Object* obj = object_create();
    
    // Store socket info
    object_set_property(obj, "fd", NUMBER_VAL(fd));
    object_set_property(obj, "is_server", BOOL_VAL(is_server));
    object_set_property(obj, "is_connected", BOOL_VAL(false));
    
    return obj;
}

// Get socket fd from object
static int get_socket_fd(TaggedValue socket_val) {
    if (!IS_OBJECT(socket_val)) return -1;
    
    Object* obj = AS_OBJECT(socket_val);
    TaggedValue* fd_val = object_get_property(obj, "fd");
    if (!fd_val || !IS_NUMBER(*fd_val)) return -1;
    
    return (int)AS_NUMBER(*fd_val);
}

// Native function: createSocket() -> Socket
static TaggedValue native_create_socket(int arg_count, TaggedValue* args) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        return NIL_VAL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    Object* socket_obj = create_socket_object(sock_fd, false);
    return OBJECT_VAL(socket_obj);
}

// Native function: bind(socket: Socket, host: String, port: Int) -> Bool
static TaggedValue native_bind(int arg_count, TaggedValue* args) {
    if (arg_count != 3 || !IS_OBJECT(args[0]) || !IS_STRING(args[1]) || !IS_NUMBER(args[2])) {
        return BOOL_VAL(false);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return BOOL_VAL(false);
    
    const char* host = AS_STRING(args[1]);
    int port = (int)AS_NUMBER(args[2]);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
            return BOOL_VAL(false);
        }
    }
    
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return BOOL_VAL(false);
    }
    
    // Mark as server socket
    Object* obj = AS_OBJECT(args[0]);
    object_set_property(obj, "is_server", BOOL_VAL(true));
    
    return BOOL_VAL(true);
}

// Native function: listen(socket: Socket, backlog: Int) -> Bool
static TaggedValue native_listen(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_OBJECT(args[0]) || !IS_NUMBER(args[1])) {
        return BOOL_VAL(false);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return BOOL_VAL(false);
    
    int backlog = (int)AS_NUMBER(args[1]);
    
    if (listen(sock_fd, backlog) < 0) {
        return BOOL_VAL(false);
    }
    
    return BOOL_VAL(true);
}

// Native function: accept(socket: Socket) -> Socket?
static TaggedValue native_accept(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_OBJECT(args[0])) {
        return NIL_VAL;
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return NIL_VAL;
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return NIL_VAL;
    }
    
    Object* client_socket = create_socket_object(client_fd, false);
    object_set_property(client_socket, "is_connected", BOOL_VAL(true));
    
    // Store client info
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    object_set_property(client_socket, "remote_host", STRING_VAL(strdup(client_ip)));
    object_set_property(client_socket, "remote_port", NUMBER_VAL(ntohs(client_addr.sin_port)));
    
    return OBJECT_VAL(client_socket);
}

// Native function: connect(socket: Socket, host: String, port: Int) -> Bool
static TaggedValue native_connect(int arg_count, TaggedValue* args) {
    if (arg_count != 3 || !IS_OBJECT(args[0]) || !IS_STRING(args[1]) || !IS_NUMBER(args[2])) {
        return BOOL_VAL(false);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return BOOL_VAL(false);
    
    const char* host = AS_STRING(args[1]);
    int port = (int)AS_NUMBER(args[2]);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    // Try to parse as IP address first
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        // If not IP, try hostname resolution
        struct hostent* he = gethostbyname(host);
        if (he == NULL) {
            return BOOL_VAL(false);
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return BOOL_VAL(false);
    }
    
    Object* obj = AS_OBJECT(args[0]);
    object_set_property(obj, "is_connected", BOOL_VAL(true));
    object_set_property(obj, "remote_host", STRING_VAL(strdup(host)));
    object_set_property(obj, "remote_port", NUMBER_VAL(port));
    
    return BOOL_VAL(true);
}

// Native function: send(socket: Socket, data: String) -> Int
static TaggedValue native_send(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_OBJECT(args[0]) || !IS_STRING(args[1])) {
        return NUMBER_VAL(-1);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return NUMBER_VAL(-1);
    
    const char* data = AS_STRING(args[1]);
    size_t len = strlen(data);
    
    ssize_t sent = send(sock_fd, data, len, 0);
    return NUMBER_VAL(sent);
}

// Native function: recv(socket: Socket, maxBytes: Int) -> String?
static TaggedValue native_recv(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_OBJECT(args[0]) || !IS_NUMBER(args[1])) {
        return NIL_VAL;
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return NIL_VAL;
    
    int max_bytes = (int)AS_NUMBER(args[1]);
    if (max_bytes <= 0 || max_bytes > 65536) {
        max_bytes = 4096; // Default buffer size
    }
    
    char* buffer = malloc(max_bytes + 1);
    if (!buffer) return NIL_VAL;
    
    ssize_t received = recv(sock_fd, buffer, max_bytes, 0);
    if (received <= 0) {
        free(buffer);
        return NIL_VAL;
    }
    
    buffer[received] = '\0';
    return STRING_VAL(buffer);
}

// Native function: close(socket: Socket) -> Bool
static TaggedValue native_close(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_OBJECT(args[0])) {
        return BOOL_VAL(false);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return BOOL_VAL(false);
    
    close(sock_fd);
    
    Object* obj = AS_OBJECT(args[0]);
    object_set_property(obj, "fd", NUMBER_VAL(-1));
    object_set_property(obj, "is_connected", BOOL_VAL(false));
    
    return BOOL_VAL(true);
}

// Native function: setNonBlocking(socket: Socket, nonBlocking: Bool) -> Bool
static TaggedValue native_set_non_blocking(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_OBJECT(args[0]) || !IS_BOOL(args[1])) {
        return BOOL_VAL(false);
    }
    
    int sock_fd = get_socket_fd(args[0]);
    if (sock_fd < 0) return BOOL_VAL(false);
    
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0) return BOOL_VAL(false);
    
    if (AS_BOOL(args[1])) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(sock_fd, F_SETFL, flags) < 0) {
        return BOOL_VAL(false);
    }
    
    return BOOL_VAL(true);
}

// Native function: getLastError() -> String
static TaggedValue native_get_last_error(int arg_count, TaggedValue* args) {
    return STRING_VAL(strdup(strerror(errno)));
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    // Register socket functions
    module_register_native_function(module, "createSocket", native_create_socket);
    module_register_native_function(module, "bind", native_bind);
    module_register_native_function(module, "listen", native_listen);
    module_register_native_function(module, "accept", native_accept);
    module_register_native_function(module, "connect", native_connect);
    module_register_native_function(module, "send", native_send);
    module_register_native_function(module, "recv", native_recv);
    module_register_native_function(module, "close", native_close);
    module_register_native_function(module, "setNonBlocking", native_set_non_blocking);
    module_register_native_function(module, "getLastError", native_get_last_error);
    
    // Export constants
    module_export(module, "AF_INET", NUMBER_VAL(AF_INET));
    module_export(module, "SOCK_STREAM", NUMBER_VAL(SOCK_STREAM));
    module_export(module, "SOCK_DGRAM", NUMBER_VAL(SOCK_DGRAM));
    
    return true;
}