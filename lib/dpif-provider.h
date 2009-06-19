/*
 * Copyright (c) 2009 Nicira Networks.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DPIF_PROVIDER_H
#define DPIF_PROVIDER_H 1

/* Provider interface to dpifs, which provide an interface to an Open vSwitch
 * datapath. */

#include <assert.h>
#include "dpif.h"

/* Open vSwitch datapath interface.
 *
 * This structure should be treated as opaque by dpif implementations. */
struct dpif {
    const struct dpif_class *class;
    char *name;
    uint8_t netflow_engine_type;
    uint8_t netflow_engine_id;
};

void dpif_init(struct dpif *, const struct dpif_class *, const char *name,
               uint8_t netflow_engine_type, uint8_t netflow_engine_id);
static inline void dpif_assert_class(const struct dpif *dpif,
                                     const struct dpif_class *class)
{
    assert(dpif->class == class);
}

/* Datapath interface class structure, to be defined by each implementation of
 * a datapath interface
 *
 * These functions return 0 if successful or a positive errno value on failure,
 * except where otherwise noted.
 *
 * These functions are expected to execute synchronously, that is, to block as
 * necessary to obtain a result.  Thus, they may not return EAGAIN or
 * EWOULDBLOCK or EINPROGRESS.  We may relax this requirement in the future if
 * and when we encounter performance problems. */
struct dpif_class {
    /* Prefix for names of dpifs in this class, e.g. "udatapath:".
     *
     * One dpif class may have the empty string "" as its prefix, in which case
     * that dpif class is associated with dpif names that don't match any other
     * class name. */
    const char *prefix;

    /* Class name, for use in error messages. */
    const char *name;

    /* Performs periodic work needed by dpifs of this class, if any is
     * necessary. */
    void (*run)(void);

    /* Arranges for poll_block() to wake up if the "run" member function needs
     * to be called. */
    void (*wait)(void);

    /* Attempts to open an existing dpif, if 'create' is false, or to open an
     * existing dpif or create a new one, if 'create' is true.  'name' is the
     * full dpif name provided by the user, e.g. "udatapath:/var/run/mypath".
     * This name is useful for error messages but must not be modified.
     *
     * 'suffix' is a copy of 'name' following the dpif's 'prefix'.
     *
     * If successful, stores a pointer to the new dpif in '*dpifp'.  On failure
     * there are no requirements on what is stored in '*dpifp'. */
    int (*open)(const char *name, char *suffix, bool create,
                struct dpif **dpifp);

    /* Closes 'dpif' and frees associated memory. */
    void (*close)(struct dpif *dpif);

    /* Attempts to destroy the dpif underlying 'dpif'.
     *
     * If successful, 'dpif' will not be used again except as an argument for
     * the 'close' member function. */
    int (*delete)(struct dpif *dpif);

    /* Retrieves statistics for 'dpif' into 'stats'. */
    int (*get_stats)(const struct dpif *dpif, struct odp_stats *stats);

    /* Retrieves 'dpif''s current treatment of IP fragments into '*drop_frags':
     * true indicates that fragments are dropped, false indicates that
     * fragments are treated in the same way as other IP packets (except that
     * the L4 header cannot be read). */
    int (*get_drop_frags)(const struct dpif *dpif, bool *drop_frags);

    /* Changes 'dpif''s treatment of IP fragments to 'drop_frags', whose
     * meaning is the same as for the get_drop_frags member function. */
    int (*set_drop_frags)(struct dpif *dpif, bool drop_frags);

    /* Creates a new port in 'dpif' connected to network device 'devname'.
     * 'flags' is a set of ODP_PORT_* flags.  If successful, sets '*port_no'
     * to the new port's port number. */
    int (*port_add)(struct dpif *dpif, const char *devname, uint16_t flags,
                    uint16_t *port_no);

    /* Removes port numbered 'port_no' from 'dpif'. */
    int (*port_del)(struct dpif *dpif, uint16_t port_no);

    /* Queries 'dpif' for a port with the given 'port_no' or 'devname'.  Stores
     * information about the port into '*port' if successful. */
    int (*port_query_by_number)(const struct dpif *dpif, uint16_t port_no,
                                struct odp_port *port);
    int (*port_query_by_name)(const struct dpif *dpif, const char *devname,
                              struct odp_port *port);

    /* Stores in 'ports' information about up to 'n' ports attached to 'dpif',
     * in no particular order.  Returns the number of ports attached to 'dpif'
     * (not the number stored), if successful, otherwise a negative errno
     * value. */
    int (*port_list)(const struct dpif *dpif, struct odp_port *ports, int n);

    /* Polls for changes in the set of ports in 'dpif'.  If the set of ports in
     * 'dpif' has changed, then this function should do one of the
     * following:
     *
     * - Preferably: store the name of the device that was added to or deleted
     *   from 'dpif' in '*devnamep' and return 0.  The caller is responsible
     *   for freeing '*devnamep' (with free()) when it no longer needs it.
     *
     * - Alternatively: return ENOBUFS, without indicating the device that was
     *   added or deleted.
     *
     * Occasional 'false positives', in which the function returns 0 while
     * indicating a device that was not actually added or deleted or returns
     * ENOBUFS without any change, are acceptable.
     *
     * If the set of ports in 'dpif' has not changed, returns EAGAIN.  May also
     * return other positive errno values to indicate that something has gone
     * wrong. */
    int (*port_poll)(const struct dpif *dpif, char **devnamep);

    /* Arranges for the poll loop to wake up when 'port_poll' will return a
     * value other than EAGAIN. */
    void (*port_poll_wait)(const struct dpif *dpif);

    /* Stores in 'ports' the port numbers of up to 'n' ports that belong to
     * 'group' in 'dpif'.  Returns the number of ports in 'group' (not the
     * number stored), if successful, otherwise a negative errno value. */
    int (*port_group_get)(const struct dpif *dpif, int group,
                          uint16_t ports[], int n);

    /* Changes port group 'group' in 'dpif' to consist of the 'n' ports whose
     * numbers are given in 'ports'.
     *
     * Use the get_stats member function to obtain the number of supported port
     * groups. */
    int (*port_group_set)(struct dpif *dpif, int group,
                          const uint16_t ports[], int n);

    /* For each flow 'flow' in the 'n' flows in 'flows':
     *
     * - If a flow matching 'flow->key' exists in 'dpif':
     *
     *     Stores 0 into 'flow->stats.error' and stores statistics for the flow
     *     into 'flow->stats'.
     *
     *     If 'flow->n_actions' is zero, then 'flow->actions' is ignored.  If
     *     'flow->n_actions' is nonzero, then 'flow->actions' should point to
     *     an array of the specified number of actions.  At most that many of
     *     the flow's actions will be copied into that array.
     *     'flow->n_actions' will be updated to the number of actions actually
     *     present in the flow, which may be greater than the number stored if
     *     the flow has more actions than space available in the array.
     *
     * - Flow-specific errors are indicated by a positive errno value in
     *   'flow->stats.error'.  In particular, ENOENT indicates that no flow
     *   matching 'flow->key' exists in 'dpif'.  When an error value is stored,
     *   the contents of 'flow->key' are preserved but other members of 'flow'
     *   should be treated as indeterminate.
     *
     * Returns 0 if all 'n' flows in 'flows' were updated (whether they were
     * individually successful or not is indicated by 'flow->stats.error',
     * however).  Returns a positive errno value if an error that prevented
     * this update occurred, in which the caller must not depend on any
     * elements in 'flows' being updated or not updated.
     */
    int (*flow_get)(const struct dpif *dpif, struct odp_flow flows[], int n);

    /* Adds or modifies a flow in 'dpif' as specified in 'put':
     *
     * - If the flow specified in 'put->flow' does not exist in 'dpif', then
     *   behavior depends on whether ODPPF_CREATE is specified in 'put->flags':
     *   if it is, the flow will be added, otherwise the operation will fail
     *   with ENOENT.
     *
     * - Otherwise, the flow specified in 'put->flow' does exist in 'dpif'.
     *   Behavior in this case depends on whether ODPPF_MODIFY is specified in
     *   'put->flags': if it is, the flow's actions will be updated, otherwise
     *   the operation will fail with EEXIST.  If the flow's actions are
     *   updated, then its statistics will be zeroed if ODPPF_ZERO_STATS is set
     *   in 'put->flags', left as-is otherwise.
     */
    int (*flow_put)(struct dpif *dpif, struct odp_flow_put *put);

    /* Deletes a flow matching 'flow->key' from 'dpif' or returns ENOENT if
     * 'dpif' does not contain such a flow.
     *
     * If successful, updates 'flow->stats', 'flow->n_actions', and
     * 'flow->actions' as described in more detail under the flow_get member
     * function below. */
    int (*flow_del)(struct dpif *dpif, struct odp_flow *flow);

    /* Deletes all flows from 'dpif' and clears all of its queues of received
     * packets. */
    int (*flow_flush)(struct dpif *dpif);

    /* Stores up to 'n' flows in 'dpif' into 'flows', updating their statistics
     * and actions as described under the flow_get member function.  If
     * successful, returns the number of flows actually present in 'dpif',
     * which might be greater than the number stored (if 'dpif' has more than
     * 'n' flows).  On failure, returns a negative errno value. */
    int (*flow_list)(const struct dpif *dpif, struct odp_flow flows[], int n);

    /* Performs the 'n_actions' actions in 'actions' on the Ethernet frame
     * specified in 'packet'.
     *
     * Pretends that the frame was originally received on the port numbered
     * 'in_port'.  This affects only ODPAT_OUTPUT_GROUP actions, which will not
     * send a packet out their input port.  Specify the number of an unused
     * port (e.g. UINT16_MAX is currently always unused) to avoid this
     * behavior. */
    int (*execute)(struct dpif *dpif, uint16_t in_port,
                   const union odp_action actions[], int n_actions,
                   const struct ofpbuf *packet);

    /* Retrieves 'dpif''s "listen mask" into '*listen_mask'.  Each ODPL_* bit
     * set in '*listen_mask' indicates the 'dpif' will receive messages of the
     * corresponding type when it calls the recv member function. */
    int (*recv_get_mask)(const struct dpif *dpif, int *listen_mask);

    /* Sets 'dpif''s "listen mask" to 'listen_mask'.  Each ODPL_* bit set in
     * 'listen_mask' indicates the 'dpif' will receive messages of the
     * corresponding type when it calls the recv member function. */
    int (*recv_set_mask)(struct dpif *dpif, int listen_mask);

    /* Attempts to receive a message from 'dpif'.  If successful, stores the
     * message into '*packetp'.  The message, if one is received, must begin
     * with 'struct odp_msg' as a header.  Only messages of the types selected
     * with the set_listen_mask member function should be received.
     *
     * This function must not block.  If no message is ready to be received
     * when it is called, it should return EAGAIN without blocking. */
    int (*recv)(struct dpif *dpif, struct ofpbuf **packetp);

    /* Arranges for the poll loop to wake up when 'dpif' has a message queued
     * to be received with the recv member function. */
    void (*recv_wait)(struct dpif *dpif);
};

extern const struct dpif_class dpif_linux_class;
extern const struct dpif_class dpif_netdev_class;

#endif /* dpif-provider.h */
