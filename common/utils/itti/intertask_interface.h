/*******************************************************************************

  Eurecom OpenAirInterface
  Copyright(c) 1999 - 2012 Eurecom

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information
  Openair Admin: openair_admin@eurecom.fr
  Openair Tech : openair_tech@eurecom.fr
  Forums       : http://forums.eurecom.fr/openairinterface
  Address      : EURECOM, Campus SophiaTech, 450 Route des Chappes
                 06410 Biot FRANCE

*******************************************************************************/

/** @defgroup _intertask_interface_impl_ Intertask Interface Mechanisms
 * Implementation
 * @ingroup _ref_implementation_
 * @{
 */

#ifndef INTERTASK_INTERFACE_H_
#define INTERTASK_INTERFACE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "intertask_interface_conf.h"
#include "intertask_interface_types.h"

#define ITTI_MSG_NAME(mSGpTR)               itti_get_message_name((mSGpTR)->header.messageId)
#define ITTI_MSG_ORIGIN_NAME(mSGpTR)        itti_get_task_name((mSGpTR)->header.originTaskId)
#define ITTI_MSG_DESTINATION_NAME(mSGpTR)   itti_get_task_name((mSGpTR)->header.destinationTaskId)
#define ITTI_MSG_INSTANCE(mSGpTR)           (mSGpTR)->header.instance

/* Make the message number platform specific */
typedef unsigned long message_number_t;
#define MESSAGE_NUMBER_SIZE (sizeof(unsigned long))

enum message_priorities {
    MESSAGE_PRIORITY_MAX       = 100,
    MESSAGE_PRIORITY_MAX_LEAST = 85,
    MESSAGE_PRIORITY_MED_PLUS  = 70,
    MESSAGE_PRIORITY_MED       = 55,
    MESSAGE_PRIORITY_MED_LEAST = 40,
    MESSAGE_PRIORITY_MIN_PLUS  = 25,
    MESSAGE_PRIORITY_MIN       = 10,
};

typedef struct message_info_s {
    task_id_t id;
    uint32_t priority;
    /* Message payload size */
    MessageHeaderSize size;
    /* Printable name */
    const char * const name;
} message_info_t;

enum task_priorities {
    TASK_PRIORITY_MAX       = 100,
    TASK_PRIORITY_MAX_LEAST = 85,
    TASK_PRIORITY_MED_PLUS  = 70,
    TASK_PRIORITY_MED       = 55,
    TASK_PRIORITY_MED_LEAST = 40,
    TASK_PRIORITY_MIN_PLUS  = 25,
    TASK_PRIORITY_MIN       = 10,
};

/** \brief Send a broadcast message to every task
 \param message_p Pointer to the message to send
 @returns < 0 on failure, 0 otherwise
 **/
int itti_send_broadcast_message(MessageDef *message_p);

/** \brief Send a message to a task (could be itself)
 \param task_id Task ID
 \param instance Instance of the task used for virtualization
 \param message Pointer to the message to send
 @returns -1 on failure, 0 otherwise
 **/
int itti_send_msg_to_task(task_id_t task_id, instance_t instance, MessageDef *message);

/** \brief Retrieves a message in the queue associated to task_id.
 * If the queue is empty, the thread is blocked till a new message arrives.
 \param task_id Task ID of the receiving task
 \param received_msg Pointer to the allocated message
 **/
void itti_receive_msg(task_id_t task_id, MessageDef **received_msg);

/** \brief Try to retrieves a message in the queue associated to task_id and matching requested instance.
 \param task_id Task ID of the receiving task
 \param instance Instance of the task used for virtualization
 \param received_msg Pointer to the allocated message
 **/
void itti_poll_msg(task_id_t task_id, instance_t instance, MessageDef **received_msg);

/** \brief Start thread associated to the task
 * \param task_id task to start
 * \param start_routine entry point for the task
 * \param args_p Optional argument to pass to the start routine
 * @returns -1 on failure, 0 otherwise
 **/
int itti_create_task(task_id_t task_id,
                     void *(*start_routine) (void *),
                     void *args_p);

/** \brief Mark the task as in ready state
 * \param task_id task to mark as ready
 **/
void itti_mark_task_ready(task_id_t task_id);

/** \brief Exit the current task.
 **/
void itti_exit_task(void);

/** \brief Indicate that the task is completed and initiate termination of all tasks.
 * \param task_id task that is completed
 **/
void itti_terminate_tasks(task_id_t task_id);

/** \brief Return the printable string associated with the message
 * \param message_id Id of the message
 **/
const char *itti_get_message_name(MessagesIds message_id);

/** \brief Return the printable string associated with a task id
 * \param thread_id Id of the task
 **/
const char *itti_get_task_name(task_id_t task_id);

/** \brief Alloc and memset(0) a new itti message.
 * \param origin_task_id Task ID of the sending task
 * \param message_id Message ID
 * @returns NULL in case of failure or newly allocated mesage ref
 **/
inline MessageDef *itti_alloc_new_message(
    task_id_t   origin_task_id,
    MessagesIds message_id);

/** \brief handle signals and wait for all threads to join when the process complete.
 * This function should be called from the main thread after having created all ITTI tasks.
 **/
void itti_wait_tasks_end(void);

/** \brief Send a termination message to all tasks.
 * \param task_id task that is broadcasting the message.
 **/
void itti_send_terminate_message(task_id_t task_id);

#endif /* INTERTASK_INTERFACE_H_ */
/* @} */