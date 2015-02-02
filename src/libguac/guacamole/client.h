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

#ifndef _GUAC_CLIENT_H
#define _GUAC_CLIENT_H

/**
 * Functions and structure contents for the Guacamole proxy client.
 *
 * @file client.h
 */

#include "client-fntypes.h"
#include "client-types.h"
#include "client-constants.h"
#include "layer-types.h"
#include "pool-types.h"
#include "socket-types.h"
#include "timestamp-types.h"
#include "user-fntypes.h"
#include "user-types.h"

#include <pthread.h>
#include <stdarg.h>

struct guac_client {

    /**
     * The guac_socket structure to be used to communicate with the web-client.
     * It is expected that the implementor of any Guacamole proxy client will
     * provide their own mechanism of I/O for their protocol. The guac_socket
     * structure is used only to communicate conveniently with the Guacamole
     * web-client.
     */
    guac_socket* socket;

    /**
     * The current state of the client. When the client is first allocated,
     * this will be initialized to GUAC_CLIENT_RUNNING. It will remain at
     * GUAC_CLIENT_RUNNING until an event occurs which requires the client to
     * shutdown, at which point the state becomes GUAC_CLIENT_STOPPING.
     */
    guac_client_state state;

    /**
     * Arbitrary reference to proxy client-specific data. Implementors of a
     * Guacamole proxy client can store any data they want here, which can then
     * be retrieved as necessary in the message handlers.
     */
    void* data;

    /**
     * The time (in milliseconds) that the last sync message was sent to the
     * client.
     */
    guac_timestamp last_sent_timestamp;

    /**
     * Handler for freeing data when the client is being unloaded.
     *
     * This handler will be called when the client needs to be unloaded
     * by the proxy, and any data allocated by the proxy client should be
     * freed.
     *
     * Note that this handler will NOT be called if the client's
     * guac_client_init() function fails.
     *
     * Implement this handler if you store data inside the client.
     *
     * Example:
     * @code
     *     int free_handler(guac_client* client);
     *
     *     int guac_client_init(guac_client* client) {
     *         client->free_handler = free_handler;
     *     }
     * @endcode
     */
    guac_client_free_handler* free_handler;

    /**
     * Logging handler. This handler will be called via guac_client_log() when
     * the client needs to log messages of any type.
     *
     * In general, only programs loading the client should implement this
     * handler, as those are the programs that would provide the logging
     * facilities.
     *
     * Client implementations should expect these handlers to already be
     * set.
     *
     * Example:
     * @code
     *     void log_handler(guac_client* client, guac_client_log_level level, const char* format, va_list args);
     *
     *     void function_of_daemon() {
     *
     *         guac_client* client = [pass log_handler to guac_client_plugin_get_client()];
     *
     *     }
     * @endcode
     */
    guac_client_log_handler* log_handler;

    /**
     * Pool of buffer indices. Buffers are simply layers with negative indices.
     * Note that because guac_pool always gives non-negative indices starting
     * at 0, the output of this guac_pool will be adjusted.
     */
    guac_pool* __buffer_pool;

    /**
     * Pool of layer indices. Note that because guac_pool always gives
     * non-negative indices starting at 0, the output of this guac_pool will
     * be adjusted.
     */
    guac_pool* __layer_pool;

    /**
     * The unique identifier allocated for the connection, which may
     * be used within the Guacamole protocol to refer to this connection.
     * This identifier is guaranteed to be unique from all existing
     * connections and will not collide with any available protocol
     * names.
     */
    char* connection_id;

    /**
     * Lock which is acquired when the users list is being manipulated.
     */
    pthread_mutex_t __users_lock;

    /**
     * The first user within the list of all connected users, or NULL if no
     * users are currently connected.
     */
    guac_user* __users;

    /**
     * The number of currently-connected users. This value may include inactive
     * users if cleanup of those users has not yet finished.
     */
    int connected_users;

    /**
     * Handler for join events, called whenever a new user is joining an
     * active connection.
     *
     * The handler is given a pointer to a newly-allocated guac_user which
     * must then be initialized, if needed.
     *
     * Example:
     * @code
     *     int join_handler(guac_user* user, int argc, char** argv);
     *
     *     int guac_client_init(guac_client* client) {
     *         client->join_handler = join_handler;
     *     }
     * @endcode
     */
    guac_user_join_handler* join_handler;

    /**
     * Handler for leave events, called whenever a new user is leaving an
     * active connection.
     *
     * The handler is given a pointer to the leaving guac_user whose custom
     * data and associated resources must now be freed, if any.
     *
     * Example:
     * @code
     *     int leave_handler(guac_user* user);
     *
     *     int guac_client_init(guac_client* client) {
     *         client->leave_handler = leave_handler;
     *     }
     * @endcode
     */
    guac_user_leave_handler* leave_handler;

    /**
     * Handler for suspend events fired by the guac_client when a guac_user
     * is suspended.
     *
     * The handler takes only a guac_user which will be the guac_user that
     * was suspended.
     *
     * Example:
     * @code
     *     int suspend_handler(guac_user* user);
     *
     *     int my_suspend_handler(guac_user* user, int argv, char** argv) {
     *         user->suspend_handler = suspend_handler;
     *     }
     * @endcode
     */
    guac_user_suspend_handler* suspend_handler;

    /**
     * Handler for resume events fired by the guac_client when a suspended
     * guac_user is resumed.
     *
     * The handler takes only a guac_user which will be the guac_user that
     * was resumed.
     *
     * Example:
     * @code
     *     int resume_handler(guac_user* user);
     *
     *     int my_resume_handler(guac_user* user, int argv, char** argv) {
     *         user->resume_handler = resume_handler;
     *     }
     * @endcode
     */
    guac_user_resume_handler* resume_handler;

    /**
     * NULL-terminated array of all arguments accepted by this client , in
     * order. New users will specify these arguments when they join the
     * connection, and the values of those arguments will be made available to
     * the function initializing newly-joined users.
     *
     * The guac_client_init entry point is expected to initialize this, if
     * arguments are expected.
     *
     * Example:
     * @code
     *     const char* __my_args[] = {
     *         "hostname",
     *         "port",
     *         "username",
     *         "password",
     *         NULL
     *     };
     *
     *     int guac_client_init(guac_client* client) {
     *         client->args = __my_args;
     *     }
     * @endcode
     */
    const char** args;

    /**
     * Handle to the dlopen()'d plugin, which should be given to dlclose() when
     * this client is freed. This is only assigned if guac_client_load_plugin()
     * is used.
     */
    void* __plugin_handle;

};

/**
 * Returns a new, barebones guac_client. This new guac_client has no handlers
 * set, but is otherwise usable.
 *
 * @return A pointer to the new client.
 */
guac_client* guac_client_alloc();

/**
 * Free all resources associated with the given client.
 *
 * @param client The proxy client to free all reasources of.
 */
void guac_client_free(guac_client* client);

/**
 * Writes a message in the log used by the given client. The logger used will
 * normally be defined by guacd (or whichever program loads the proxy client)
 * by setting the logging handlers of the client when it is loaded.
 *
 * @param client The proxy client logging this message.
 * @param level The level at which to log this message.
 * @param format A printf-style format string to log.
 * @param ... Arguments to use when filling the format string for printing.
 */
void guac_client_log(guac_client* client, guac_client_log_level level,
        const char* format, ...);

/**
 * Writes a message in the log used by the given client. The logger used will
 * normally be defined by guacd (or whichever program loads the proxy client)
 * by setting the logging handlers of the client when it is loaded.
 *
 * @param client The proxy client logging this message.
 * @param level The level at which to log this message.
 * @param format A printf-style format string to log.
 * @param ap The va_list containing the arguments to be used when filling the
 *           format string for printing.
 */
void vguac_client_log(guac_client* client, guac_client_log_level level,
        const char* format, va_list ap);

/**
 * Signals the given client to stop gracefully. This is a completely
 * cooperative signal, and can be ignored by the client or the hosting
 * daemon.
 *
 * @param client The proxy client to signal to stop.
 */
void guac_client_stop(guac_client* client);

/**
 * Signals the given client to stop gracefully, while also signalling via the
 * Guacamole protocol that an error has occurred. Note that this is a completely
 * cooperative signal, and can be ignored by the client or the hosting
 * daemon. The message given will be logged to the system logs.
 *
 * @param client The proxy client to signal to stop.
 * @param status The status to send over the Guacamole protocol.
 * @param format A printf-style format string to log.
 * @param ... Arguments to use when filling the format string for printing.
 */
void guac_client_abort(guac_client* client, guac_protocol_status status,
        const char* format, ...);

/**
 * Signals the given client to stop gracefully, while also signalling via the
 * Guacamole protocol that an error has occurred. Note that this is a completely
 * cooperative signal, and can be ignored by the client or the hosting
 * daemon. The message given will be logged to the system logs.
 *
 * @param client The proxy client to signal to stop.
 * @param status The status to send over the Guacamole protocol.
 * @param format A printf-style format string to log.
 * @param ap The va_list containing the arguments to be used when filling the
 *           format string for printing.
 */
void vguac_client_abort(guac_client* client, guac_protocol_status status,
        const char* format, va_list ap);

/**
 * Allocates a new buffer (invisible layer). An arbitrary index is
 * automatically assigned if no existing buffer is available for use.
 *
 * @param client The proxy client to allocate the buffer for.
 * @return The next available buffer, or a newly allocated buffer.
 */
guac_layer* guac_client_alloc_buffer(guac_client* client);

/**
 * Allocates a new layer. An arbitrary index is automatically assigned
 * if no existing layer is available for use.
 *
 * @param client The proxy client to allocate the layer buffer for.
 * @return The next available layer, or a newly allocated layer.
 */
guac_layer* guac_client_alloc_layer(guac_client* client);

/**
 * Returns the given buffer to the pool of available buffers, such that it
 * can be reused by any subsequent call to guac_client_allow_buffer().
 *
 * @param client The proxy client to return the buffer to.
 * @param layer The buffer to return to the pool of available buffers.
 */
void guac_client_free_buffer(guac_client* client, guac_layer* layer);

/**
 * Returns the given layer to the pool of available layers, such that it
 * can be reused by any subsequent call to guac_client_allow_layer().
 *
 * @param client The proxy client to return the layer to.
 * @param layer The buffer to return to the pool of available layer.
 */
void guac_client_free_layer(guac_client* client, guac_layer* layer);

/**
 * Adds the given user to the internal list of connected users. Future writes
 * to the broadcast socket stored within guac_client will also write to this
 * user. The join handler of this guac_client will be called.
 *
 * @param client The proxy client to add the user to.
 * @param user The user to add.
 * @param argc The number of arguments to pass to the new user.
 * @param argv An array of strings containing the argument values being passed.
 * @return Zero if the user was added successfully, non-zero if the user could
 *         not join the connection.
 */
int guac_client_add_user(guac_client* client, guac_user* user, int argc, char** argv);

/**
 * Removes the given user, removing the user from the internally-tracked list
 * of connected users, and calling any appropriate leave handler.
 *
 * @param client The proxy client associated with the user being removed.
 * @param user The user to remove.
 */
void guac_client_remove_user(guac_client* client, guac_user* user);

/**
 * Temporarily suspends the given user, preventing them from receiving any
 * data along the associated guac_client's broadcast socket. If the user has a
 * defined suspend  handler, it will be called, otherwise the resume handler of
 * the associated guac_client will be called.
 *
 * @param client
 *     The proxy client associated with the user being suspended.
 *
 * @param user
 *     The user to suspend.
 */
void guac_client_suspend_user(guac_client* client, guac_user* user);

/**
 * Resumes the given user, allowing them to again receive data along the
 * associated guac_client's broadcast socket. If the user has a defined resume
 * handler, it will be called, otherwise the resume handler of the associated
 * guac_client will be called.
 *
 * @param client
 *     The proxy client associated with the user being resumed.
 *
 * @param user
 *     The user to resume.
 */
void guac_client_resume_user(guac_client* client, guac_user* user);

/**
 * Calls the given function on all currently-connected and running users of the
 * given client. The function will be given a reference to a guac_user and the
 * specified arbitrary data, and will not be invoked for any suspended users.
 *
 * This function is NOT reentrant. The user list MUST NOT be manipulated
 * within the same thread as a callback to this function, and the callback
 * MUST NOT invoke guac_client_foreach_user() within its own thread.
 *
 * @param client The client whose users should be iterated.
 * @param callback The function to call for each user.
 * @param data Arbitrary data to pass to each function call.
 */
void guac_client_foreach_user(guac_client* client, guac_user_callback* callback, void* data);

/**
 * Marks the end of the current frame by sending a "sync" instruction to
 * all connected users. This instruction will contain the current timestamp.
 * The last_sent_timestamp member of guac_client will be updated accordingly.
 *
 * If an error occurs sending the instruction, a non-zero value is
 * returned, and guac_error is set appropriately.
 *
 * @param client The guac_client which has finished a frame.
 * @return Zero on success, non-zero on error.
 */
int guac_client_end_frame(guac_client* client);

/**
 * Initializes the given guac_client using the initialization routine provided
 * by the plugin corresponding to the named protocol. This will automatically
 * invoke guac_client_init within the plugin for the given protocol.
 *
 * Note that the connection will likely not be established until the first
 * user (the "owner") is added to the client.
 *
 * @param client The guac_client to initialize.
 * @param protocol The name of the protocol to use.
 * @return Zero if initialization was successful, non-zero otherwise.
 */
int guac_client_load_plugin(guac_client* client, const char* protocol);

/**
 * The default Guacamole client layer, layer 0.
 */
extern const guac_layer* GUAC_DEFAULT_LAYER;

#endif

