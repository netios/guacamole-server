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

#ifndef GUAC_RDP_USER_H
#define GUAC_RDP_USER_H

#include <guacamole/user.h>

/**
 * The time to allow between sync responses in milliseconds. If a sync
 * instruction is sent to a user and no response is received within this
 * timeframe, the user will be suspended until a response is received.
 */
#define GUAC_RDP_LAG_THRESHOLD 500

/**
 * Handler for joining users.
 *
 * @param user The user joining the connection.
 *
 * @param argc
 *     The number of arguments provided by the user during the connection
 *     handshake.
 *
 * @param argv
 *     Array of values of each argument provided by the user during the
 *     connection handshake.
 *
 * @return Zero if the join operation succeeded, non-zero otherwise.
 */
int guac_rdp_user_join_handler(guac_user* user, int argc, char** argv);

/**
 * Handler for leaving users.
 *
 * @param user The user leaving the connection.
 * @return Zero if the leave operation succeeded, non-zero otherwise.
 */
int guac_rdp_user_leave_handler(guac_user* user);

/**
 * Handler for resuming users.
 *
 * @param user
 *     The user being resumed
 *
 * @return
 *     Zero if the resume operation succeeded, non-zero otherwise.
 */
int guac_rdp_user_resume_handler(guac_user* user);

/**
 * Handler called for every user sync response.
 *
 * @param user
 *     The user responding to the sync.
 *
 * @param timestamp
 *     The timestamp of the sync instruction received.
 *
 * @return
 *     Zero if the sync was handled successfully, non-zero otherwise.
 */
int guac_rdp_user_sync_handler(guac_user* user, guac_timestamp timestamp);

/**
 * Handler called for every frame sent to a user.
 *
 * @param user
 *     The user receiving the frame.
 *
 * @param timestamp
 *     The timestamp of the end of the frame.
 *
 * @return
 *     Zero if the frame was handled successfully, non-zero otherwise.
 */
int guac_rdp_user_frame_handler(guac_user* user, guac_timestamp timestamp);

#endif
