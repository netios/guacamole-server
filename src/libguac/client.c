/*
 * Copyright (C) 2013 Glyptodon LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include "client.h"
#include "error.h"
#include "id.h"
#include "layer.h"
#include "pool.h"
#include "plugin.h"
#include "protocol.h"
#include "socket.h"
#include "timestamp.h"
#include "user.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

guac_layer __GUAC_DEFAULT_LAYER = {
    .index = 0
};

const guac_layer* GUAC_DEFAULT_LAYER = &__GUAC_DEFAULT_LAYER;

/**
 * The broadcast socket cannot be read from.
 */
static ssize_t __guac_socket_broadcast_read_handler(guac_socket* socket,
        void* buf, size_t count) {

    /* Broadcast socket reads are not allowed */
    return -1;

}

/**
 * Single chunk of data, to be broadcast to all users.
 */
typedef struct __write_chunk {

    /**
     * The buffer to write.
     */
    const void* buffer;

    /**
     * The number of bytes in the buffer.
     */
    size_t length;

} __write_chunk;

/**
 * Writes a chunk of data to a given user.
 */
static void __write_chunk_callback(guac_user* user, void* data) {

    __write_chunk* chunk = (__write_chunk*) data;

    /* Attempt write, disconnect on failure */
    if (guac_socket_write(user->socket, chunk->buffer, chunk->length))
        guac_user_stop(user);

}

/**
 * Socket write handler which operates on each of the sockets of all connected
 * users, unifying the results.
 */
static ssize_t __guac_socket_broadcast_write_handler(guac_socket* socket,
        const void* buf, size_t count) {

    guac_client* client = (guac_client*) socket->data;

    /* Build chunk */
    __write_chunk chunk;
    chunk.buffer = buf;
    chunk.length = count;

    /* Broadcast chunk to all users */
    guac_client_foreach_user(client, __write_chunk_callback, &chunk);

    return count;

}

/**
 * The broadcast socket cannot be read from (nor selected).
 */
static int __guac_socket_broadcast_select_handler(guac_socket* socket, int usec_timeout) {

    /* Selecting the broadcast socket is not possible */
    return -1;

}

guac_layer* guac_client_alloc_layer(guac_client* client) {

    /* Init new layer */
    guac_layer* allocd_layer = malloc(sizeof(guac_layer));
    allocd_layer->index = guac_pool_next_int(client->__layer_pool)+1;

    return allocd_layer;

}

guac_layer* guac_client_alloc_buffer(guac_client* client) {

    /* Init new layer */
    guac_layer* allocd_layer = malloc(sizeof(guac_layer));
    allocd_layer->index = -guac_pool_next_int(client->__buffer_pool) - 1;

    return allocd_layer;

}

void guac_client_free_buffer(guac_client* client, guac_layer* layer) {

    /* Release index to pool */
    guac_pool_free_int(client->__buffer_pool, -layer->index - 1);

    /* Free layer */
    free(layer);

}

void guac_client_free_layer(guac_client* client, guac_layer* layer) {

    /* Release index to pool */
    guac_pool_free_int(client->__layer_pool, layer->index);

    /* Free layer */
    free(layer);

}

guac_client* guac_client_alloc() {

    pthread_mutexattr_t lock_attributes;

    /* Allocate new client */
    guac_client* client = malloc(sizeof(guac_client));
    if (client == NULL) {
        guac_error = GUAC_STATUS_NO_MEMORY;
        guac_error_message = "Could not allocate memory for client";
        return NULL;
    }

    /* Init new client */
    memset(client, 0, sizeof(guac_client));

    client->state = GUAC_CLIENT_RUNNING;
    client->last_sent_timestamp = guac_timestamp_current();

    /* Generate ID */
    client->connection_id = guac_generate_id(GUAC_CLIENT_ID_PREFIX);
    if (client->connection_id == NULL) {
        free(client);
        return NULL;
    }

    /* Allocate buffer and layer pools */
    client->__buffer_pool = guac_pool_alloc(GUAC_BUFFER_POOL_INITIAL_SIZE);
    client->__layer_pool = guac_pool_alloc(GUAC_BUFFER_POOL_INITIAL_SIZE);

    /* Init locks */
    pthread_mutexattr_init(&lock_attributes);
    pthread_mutexattr_setpshared(&lock_attributes, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&(client->__users_lock), &lock_attributes);

    /* Set up socket to broadcast to all users */
    guac_socket* socket = guac_socket_alloc();
    client->socket = socket;
    socket->data   = client;

    socket->read_handler   = __guac_socket_broadcast_read_handler;
    socket->write_handler  = __guac_socket_broadcast_write_handler;
    socket->select_handler = __guac_socket_broadcast_select_handler;

    return client;

}

void guac_client_free(guac_client* client) {

    /* Remove all users */
    while (client->__users != NULL)
        guac_client_remove_user(client, client->__users);

    if (client->free_handler) {

        /* FIXME: Errors currently ignored... */
        client->free_handler(client);

    }

    /* Free layer pools */
    guac_pool_free(client->__buffer_pool);
    guac_pool_free(client->__layer_pool);

    /* Close associated plugin */
    if (client->__plugin_handle != NULL) {
        if (dlclose(client->__plugin_handle))
            guac_client_log(client, GUAC_LOG_ERROR, "Unable to close plugin: %s", dlerror());
    }

    pthread_mutex_destroy(&(client->__users_lock));
    free(client);
}

void vguac_client_log(guac_client* client, guac_client_log_level level,
        const char* format, va_list ap) {

    /* Call handler if defined */
    if (client->log_handler != NULL)
        client->log_handler(client, level, format, ap);

}

void guac_client_log(guac_client* client, guac_client_log_level level,
        const char* format, ...) {

    va_list args;
    va_start(args, format);

    vguac_client_log(client, level, format, args);

    va_end(args);

}

void guac_client_stop(guac_client* client) {
    client->state = GUAC_CLIENT_STOPPING;
}

void vguac_client_abort(guac_client* client, guac_protocol_status status,
        const char* format, va_list ap) {

    /* Only relevant if client is running */
    if (client->state == GUAC_CLIENT_RUNNING) {

        /* Log detail of error */
        vguac_client_log(client, GUAC_LOG_ERROR, format, ap);

        /* Send error immediately, limit information given */
        guac_protocol_send_error(client->socket, "Aborted. See logs.", status);
        guac_socket_flush(client->socket);

        /* Stop client */
        guac_client_stop(client);

    }

}

void guac_client_abort(guac_client* client, guac_protocol_status status,
        const char* format, ...) {

    va_list args;
    va_start(args, format);

    vguac_client_abort(client, status, format, args);

    va_end(args);

}

int guac_client_add_user(guac_client* client, guac_user* user, int argc, char** argv) {

    int retval = 0;

    pthread_mutex_lock(&(client->__users_lock));

    /* Call handler, if defined */
    if (client->join_handler)
        retval = client->join_handler(user, argc, argv);

    /* Add to list if join was successful */
    if (retval == 0) {

        user->__prev = NULL;
        user->__next = client->__users;

        if (client->__users != NULL)
            client->__users->__prev = user;

        client->__users = user;
        client->connected_users++;

    }

    pthread_mutex_unlock(&(client->__users_lock));

    return retval;

}

void guac_client_remove_user(guac_client* client, guac_user* user) {

    pthread_mutex_lock(&(client->__users_lock));

    /* Call handler, if defined */
    if (user->leave_handler)
        user->leave_handler(user);
    else if (client->leave_handler)
        client->leave_handler(user);

    /* Update prev / head */
    if (user->__prev != NULL)
        user->__prev->__next = user->__next;
    else
        client->__users = user->__next;

    /* Update next */
    if (user->__next != NULL)
        user->__next->__prev = user->__prev;

    client->connected_users--;

    pthread_mutex_unlock(&(client->__users_lock));

}

void guac_client_suspend_user(guac_client* client, guac_user* user) {

    guac_user_log(user, GUAC_LOG_DEBUG, "Suspending user");

    /* Ensure suspend occurs at an instruction boundary */
    guac_socket_instruction_begin(client->socket);
    user->state = GUAC_USER_SUSPENDED;
    guac_socket_instruction_end(client->socket);

    /* Call handler, if defined */
    if (user->suspend_handler)
        user->suspend_handler(user);
    else if (client->suspend_handler)
        client->suspend_handler(user);

}

void guac_client_resume_user(guac_client* client, guac_user* user) {

    guac_user_log(user, GUAC_LOG_DEBUG, "Resuming user");

    /* Ensure resume occurs at an instruction boundary */
    guac_socket_instruction_begin(client->socket);
    user->state = GUAC_USER_RUNNING;
    guac_socket_instruction_end(client->socket);

    /* Call handler, if defined */
    if (user->resume_handler)
        user->resume_handler(user);
    else if (client->resume_handler)
        client->resume_handler(user);

}

void guac_client_foreach_user(guac_client* client, guac_user_callback* callback, void* data) {

    guac_user* current;

    pthread_mutex_lock(&(client->__users_lock));

    /* Call function on each active user */
    current = client->__users;
    while (current != NULL) {

        /* Only call callback if user is running */
        if (current->state == GUAC_USER_RUNNING)
            callback(current, data);

        current = current->__next;
    }

    pthread_mutex_unlock(&(client->__users_lock));

}

int guac_client_end_frame(guac_client* client) {

    /* Update and send timestamp */
    client->last_sent_timestamp = guac_timestamp_current();
    return guac_protocol_send_sync(client->socket, client->last_sent_timestamp);

}

/**
 * Empty NULL-terminated array of argument names.
 */
const char* __GUAC_CLIENT_NO_ARGS[] = { NULL };

int guac_client_load_plugin(guac_client* client, const char* protocol) {

    /* Reference to dlopen()'d plugin */
    void* client_plugin_handle;

    /* Pluggable client */
    char protocol_lib[GUAC_PROTOCOL_LIBRARY_LIMIT] =
        GUAC_PROTOCOL_LIBRARY_PREFIX;
 
    union {
        guac_client_init_handler* client_init;
        void* obj;
    } alias;

    /* Add protocol and .so suffix to protocol_lib */
    strncat(protocol_lib, protocol, GUAC_PROTOCOL_NAME_LIMIT-1);
    strcat(protocol_lib, GUAC_PROTOCOL_LIBRARY_SUFFIX);

    /* Load client plugin */
    client_plugin_handle = dlopen(protocol_lib, RTLD_LAZY);
    if (!client_plugin_handle) {
        guac_error = GUAC_STATUS_NOT_FOUND;
        guac_error_message = dlerror();
        return -1;
    }

    dlerror(); /* Clear errors */

    /* Get init function */
    alias.obj = dlsym(client_plugin_handle, "guac_client_init");

    /* Fail if cannot find guac_client_init */
    if (dlerror() != NULL) {
        guac_error = GUAC_STATUS_INTERNAL_ERROR;
        guac_error_message = dlerror();
        return -1;
    }

    /* Init client */
    client->args = __GUAC_CLIENT_NO_ARGS;
    client->__plugin_handle = client_plugin_handle;

    return alias.client_init(client);

}

