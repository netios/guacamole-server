/*
 * Copyright (C) 2014 Glyptodon LLC
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

#include "clipboard.h"
#include "input.h"
#include "guac_dot_cursor.h"
#include "guac_pointer_cursor.h"
#include "guac_surface.h"
#include "user.h"
#include "vnc.h"

#include <guacamole/client.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

#include <pthread.h>

/**
 * Synchronizes the remote display of the given user such that it matches the
 * server-side display state.
 *
 * @param user
 *     The user to synchronize.
 */
static void guac_vnc_synchronize_user(guac_user* user) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;

    /* Synchronize user with display state */
    guac_common_surface_dup(vnc_client->default_surface, user->socket);
    guac_common_cursor_dup(vnc_client->cursor, user->socket);
    guac_socket_flush(user->socket);

}

int guac_vnc_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;
    guac_vnc_settings* vnc_settings = &(vnc_client->settings);

    /* Connect via VNC if owner */
    if (user->owner) {

        /* Parse arguments into client */
        if (guac_vnc_parse_args(&(vnc_client->settings), argc, (const char**) argv)) {
            guac_user_log(user, GUAC_LOG_INFO, "Badly formatted client arguments.");
            return 1;
        }

        /* Start client thread */
        if (pthread_create(&vnc_client->client_thread, NULL, guac_vnc_client_thread, user->client)) {
            guac_user_log(user, GUAC_LOG_ERROR, "Unable to start VNC client thread.");
            return 1;
        }

    }

    /* If not owner, synchronize with current display */
    else
        guac_vnc_synchronize_user(user);

    /* Only handle mouse/keyboard/clipboard if not read-only */
    if (vnc_settings->read_only == 0) {
        user->mouse_handler = guac_vnc_user_mouse_handler;
        user->key_handler = guac_vnc_user_key_handler;
        user->clipboard_handler = guac_vnc_clipboard_handler;
    }

    /* Add user management handlers */
    user->leave_handler  = guac_vnc_user_leave_handler;
    user->resume_handler = guac_vnc_user_resume_handler;

    /* Frame and lag control handlers */
    user->frame_handler = guac_vnc_user_frame_handler;
    user->sync_handler  = guac_vnc_user_sync_handler;

    return 0;

}

int guac_vnc_user_leave_handler(guac_user* user) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;

    guac_common_cursor_remove_user(vnc_client->cursor, user);

    return 0;
}

int guac_vnc_user_resume_handler(guac_user* user) {

    /* Re-synchronize user with display state */
    guac_vnc_synchronize_user(user);

    return 0;

}

int guac_vnc_user_sync_handler(guac_user* user, guac_timestamp timestamp) {

    /* Resume user if they are back in sync */
    if (user->state == GUAC_USER_SUSPENDED
            && user->last_sent_timestamp == timestamp)
        guac_client_resume_user(user->client, user);

    return 0;

}

int guac_vnc_user_frame_handler(guac_user* user, guac_timestamp timestamp) {

    /* Calculate time difference between client and server as of last frame */
    int lag = user->last_sent_timestamp - user->last_received_timestamp;

    /* Suspend user if they have fallend out of sync */
    if (user->state == GUAC_USER_RUNNING && lag >= GUAC_VNC_LAG_THRESHOLD)
        guac_client_suspend_user(user->client, user);

    return 0;

}

