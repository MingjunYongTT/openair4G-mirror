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

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "queue.h"
#include "assertions.h"

#if defined(ENABLE_EVENT_FD)
# include <sys/epoll.h>
# include <sys/eventfd.h>
# include "liblfds611.h"
#endif

#include "intertask_interface.h"
#include "intertask_interface_dump.h"

/* Includes "intertask_interface_init.h" to check prototype coherence, but
 * disable threads and messages information generation.
 */
#define CHECK_PROTOTYPE_ONLY
#include "intertask_interface_init.h"
#undef CHECK_PROTOTYPE_ONLY

#include "signals.h"
#include "timer.h"

int itti_debug = 0;

#define ITTI_DEBUG(x, args...) do { if (itti_debug) fprintf(stdout, "[ITTI][D]"x, ##args); fflush (stdout); } \
    while(0)
#define ITTI_ERROR(x, args...) do { fprintf(stdout, "[ITTI][E]"x, ##args); fflush (stdout); } \
    while(0)

/* Global message size */
#define MESSAGE_SIZE(mESSAGEiD) (sizeof(MessageHeader) + itti_desc.messages_info[mESSAGEiD].size)

typedef enum task_state_s {
    TASK_STATE_NOT_CONFIGURED, TASK_STATE_STARTING, TASK_STATE_READY, TASK_STATE_ENDED, TASK_STATE_MAX,
} task_state_t;

/* This list acts as a FIFO of messages received by tasks (RRC, NAS, ...) */
typedef struct message_list_s {
#if !defined(ENABLE_EVENT_FD)
    STAILQ_ENTRY(message_list_s) next_element;
#endif

    MessageDef *msg; ///< Pointer to the message

    message_number_t message_number; ///< Unique message number
    uint32_t message_priority; ///< Message priority
} message_list_t;

typedef struct thread_desc_s {
    /* pthread associated with the thread */
    pthread_t task_thread;
    /* State of the thread */
    volatile task_state_t task_state;
} thread_desc_t;

typedef struct task_desc_s {
    /* Queue of messages belonging to the task */
#if !defined(ENABLE_EVENT_FD)
    STAILQ_HEAD(message_queue_head, message_list_s) message_queue;

    /* Number of messages in the queue */
    volatile uint32_t message_in_queue;
    /* Mutex for the message queue */
    pthread_mutex_t message_queue_mutex;
    /* Conditional var for message queue and task synchro */
    pthread_cond_t message_queue_cond_var;
#else
    struct lfds611_queue_state *message_queue;

    /* This fd is used internally by ITTI. */
    int epoll_fd;

    /* The task fd */
    int task_event_fd;

    /* Number of events to monitor */
    uint16_t nb_events;

    /* Array of events monitored by the task.
     * By default only one fd is monitored (the one used to received messages
     * from other tasks).
     * More events can be suscribed later by the task itself.
     */
    struct epoll_event *events;

    int epoll_nb_events;
#endif
} task_desc_t;

typedef struct itti_desc_s {
    thread_desc_t *threads;
    task_desc_t *tasks;

    /* Current message number. Incremented every call to send_msg_to_task */
    message_number_t message_number __attribute__((aligned(8)));

    thread_id_t thread_max;
    task_id_t task_max;
    MessagesIds messages_id_max;

    pthread_t thread_handling_signals;

    const task_info_t *tasks_info;
    const message_info_t *messages_info;

    itti_lte_time_t lte_time;
} itti_desc_t;

static itti_desc_t itti_desc;

static inline message_number_t itti_increment_message_number(void) {
    /* Atomic operation supported by GCC: returns the current message number
     * and then increment it by 1.
     * This can be done without mutex.
     */
    return __sync_fetch_and_add (&itti_desc.message_number, 1);
}

static inline uint32_t itti_get_message_priority(MessagesIds message_id) {
    DevCheck(message_id < itti_desc.messages_id_max, message_id, itti_desc.messages_id_max, 0);

    return (itti_desc.messages_info[message_id].priority);
}

const char *itti_get_message_name(MessagesIds message_id) {
    DevCheck(message_id < itti_desc.messages_id_max, message_id, itti_desc.messages_id_max, 0);

    return (itti_desc.messages_info[message_id].name);
}

const char *itti_get_task_name(task_id_t task_id)
{
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);

    return (itti_desc.tasks_info[task_id].name);
}

void itti_update_lte_time(uint32_t frame, uint8_t slot)
{
    itti_desc.lte_time.frame = frame;
    itti_desc.lte_time.slot = slot;
}

int itti_send_broadcast_message(MessageDef *message_p) {
    task_id_t destination_task_id;
    thread_id_t origin_thread_id;
    uint32_t thread_id;
    int ret = 0;
    int result;

    DevAssert(message_p != NULL);

    origin_thread_id = TASK_GET_THREAD_ID(message_p->header.originTaskId);

    destination_task_id = TASK_FIRST;
    for (thread_id = THREAD_FIRST; thread_id < itti_desc.thread_max; thread_id++) {
        MessageDef *new_message_p;

        while (thread_id != TASK_GET_THREAD_ID(destination_task_id))
        {
            destination_task_id++;
        }
        /* Skip task that broadcast the message */
        if (thread_id != origin_thread_id) {
            /* Skip tasks which are not running */
            if (itti_desc.threads[thread_id].task_state == TASK_STATE_READY) {
                new_message_p = malloc (sizeof(MessageDef));
                DevAssert(message_p != NULL);

                memcpy (new_message_p, message_p, sizeof(MessageDef));
                result = itti_send_msg_to_task (destination_task_id, INSTANCE_DEFAULT, new_message_p);
                DevCheck(result >= 0, message_p->header.messageId, thread_id, destination_task_id);
            }
        }
    }
    free (message_p);

    return ret;
}

inline MessageDef *itti_alloc_new_message(task_id_t origin_task_id, MessagesIds message_id) {
    MessageDef *temp = NULL;

    DevCheck(message_id < itti_desc.messages_id_max, message_id, itti_desc.messages_id_max, 0);

    temp = calloc (1, MESSAGE_SIZE(message_id));
    DevAssert(temp != NULL);

    temp->header.messageId = message_id;
    temp->header.originTaskId = origin_task_id;
    temp->header.size = itti_desc.messages_info[message_id].size;

    return temp;
}

int itti_send_msg_to_task(task_id_t task_id, instance_t instance, MessageDef *message) {
    thread_id_t thread_id = TASK_GET_THREAD_ID(task_id);
    message_list_t *new;
    uint32_t priority;
    message_number_t message_number;
    uint32_t message_id;

    DevAssert(message != NULL);
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);

    message->header.destinationTaskId = task_id;
    message->header.instance = instance;
    message->header.lte_time.frame = itti_desc.lte_time.frame;
    message->header.lte_time.slot = itti_desc.lte_time.slot;
    message_id = message->header.messageId;
    DevCheck(message_id < itti_desc.messages_id_max, itti_desc.messages_id_max, message_id, 0);

    priority = itti_get_message_priority (message_id);

    /* Increment the global message number */
    message_number = itti_increment_message_number ();

    itti_dump_queue_message (message_number, message, itti_desc.messages_info[message_id].name,
                             MESSAGE_SIZE(message_id));

    if (task_id != TASK_UNKNOWN)
    {
        /* We cannot send a message if the task is not running */
        DevCheck(itti_desc.threads[thread_id].task_state == TASK_STATE_READY, itti_desc.threads[thread_id].task_state,
                 TASK_STATE_READY, thread_id);

#if !defined(ENABLE_EVENT_FD)
        /* Lock the mutex to get exclusive access to the list */
        pthread_mutex_lock (&itti_desc.tasks[task_id].message_queue_mutex);

        /* Check the number of messages in the queue */
        DevCheck(itti_desc.tasks[task_id].message_in_queue < itti_desc.tasks_info[task_id].queue_size,
                 task_id, itti_desc.tasks[task_id].message_in_queue, itti_desc.tasks_info[task_id].queue_size);
#endif

        /* Allocate new list element */
        new = (message_list_t *) malloc (sizeof(struct message_list_s));
        DevAssert(new != NULL);

        /* Fill in members */
        new->msg = message;
        new->message_number = message_number;
        new->message_priority = priority;

#if defined(ENABLE_EVENT_FD)
        {
            uint64_t sem_counter = 1;

            lfds611_queue_enqueue(itti_desc.tasks[task_id].message_queue, new);

            /* Call to write for an event fd must be of 8 bytes */
            write(itti_desc.tasks[task_id].task_event_fd, &sem_counter, sizeof(sem_counter));
        }
#else
        if (STAILQ_EMPTY (&itti_desc.tasks[task_id].message_queue)) {
            STAILQ_INSERT_HEAD (&itti_desc.tasks[task_id].message_queue, new, next_element);
        }
        else {
//         struct message_list_s *insert_after = NULL;
//         struct message_list_s *temp;
// 
//         /* This method is inefficient... */
//         STAILQ_FOREACH(temp, &itti_desc.tasks[task_id].message_queue, next_element) {
//             struct message_list_s *next;
//             next = STAILQ_NEXT(temp, next_element);
//             /* Increment message priority to create a sort of
//              * priority based scheduler */
// //             if (temp->message_priority < TASK_PRIORITY_MAX) {
// //                 temp->message_priority++;
// //             }
//             if (next && next->message_priority < priority) {
//                 insert_after = temp;
//                 break;
//             }
//         }
//         if (insert_after == NULL) {
        STAILQ_INSERT_TAIL (&itti_desc.tasks[task_id].message_queue, new, next_element);
//         } else {
//             STAILQ_INSERT_AFTER(&itti_desc.tasks[task_id].message_queue, insert_after, new,
//                                 next_element);
//         }
        }

        /* Update the number of messages in the queue */
        itti_desc.tasks[task_id].message_in_queue++;
        if (itti_desc.tasks[task_id].message_in_queue == 1) {
            /* Emit a signal to wake up target task thread */
            pthread_cond_signal (&itti_desc.tasks[task_id].message_queue_cond_var);
        }
        /* Release the mutex */
        pthread_mutex_unlock (&itti_desc.tasks[task_id].message_queue_mutex);
#endif
    }

    ITTI_DEBUG(
            "Message %s, number %lu with priority %d successfully sent to queue (%u:%s)\n",
            itti_desc.messages_info[message_id].name, message_number, priority, task_id, itti_get_task_name(task_id));
    return 0;
}

#if defined(ENABLE_EVENT_FD)
void itti_subscribe_event_fd(task_id_t task_id, int fd)
{
    struct epoll_event event;

    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);
    DevCheck(fd >= 0, fd, 0, 0);

    itti_desc.tasks[task_id].nb_events++;

    /* Reallocate the events */
    itti_desc.tasks[task_id].events = realloc(
        itti_desc.tasks[task_id].events,
        itti_desc.tasks[task_id].nb_events * sizeof(struct epoll_event));

    event.events  = EPOLLIN;
    event.data.fd = fd;

    /* Add the event fd to the list of monitored events */
    if (epoll_ctl(itti_desc.tasks[task_id].epoll_fd, EPOLL_CTL_ADD, fd,
        &event) != 0)
    {
        ITTI_ERROR("epoll_ctl (EPOLL_CTL_ADD) failed for task %s, fd %d: %s\n",
                   itti_get_task_name(task_id), fd, strerror(errno));
        /* Always assert on this condition */
        DevAssert(0 == 1);
    }
}

void itti_unsubscribe_event_fd(task_id_t task_id, int fd)
{
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);
    DevCheck(fd >= 0, fd, 0, 0);

    /* Add the event fd to the list of monitored events */
    if (epoll_ctl(itti_desc.tasks[task_id].epoll_fd, EPOLL_CTL_DEL, fd, NULL) != 0)
    {
        ITTI_ERROR("epoll_ctl (EPOLL_CTL_DEL) failed for task %s and fd %d: %s\n",
                   itti_get_task_name(task_id), fd, strerror(errno));
        /* Always assert on this condition */
        DevAssert(0 == 1);
    }

    itti_desc.tasks[task_id].nb_events--;
    itti_desc.tasks[task_id].events = realloc(
        itti_desc.tasks[task_id].events,
        itti_desc.tasks[task_id].nb_events * sizeof(struct epoll_event));
}

int itti_get_events(task_id_t task_id, struct epoll_event **events)
{
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);

    *events = itti_desc.tasks[task_id].events;

    return itti_desc.tasks[task_id].epoll_nb_events;
}

static inline void itti_receive_msg_internal_event_fd(task_id_t task_id, uint8_t polling, MessageDef **received_msg)
{
    int epoll_ret = 0;
    int epoll_timeout = 0;
    int i;

    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);
    DevAssert(received_msg != NULL);

    *received_msg = NULL;

    if (polling) {
        /* In polling mode we set the timeout to 0 causing epoll_wait to return
         * immediately.
         */
        epoll_timeout = 0;
    } else {
        /* timeout = -1 causes the epoll_wait to wait indefinetely.
         */
        epoll_timeout = -1;
    }

    do {
        epoll_ret = epoll_wait(itti_desc.tasks[task_id].epoll_fd,
                               itti_desc.tasks[task_id].events,
                               itti_desc.tasks[task_id].nb_events,
                               epoll_timeout);
    } while (epoll_ret < 0 && errno == EINTR);

    if (epoll_ret < 0) {
        ITTI_ERROR("epoll_wait failed for task %s: %s\n",
                   itti_get_task_name(task_id), strerror(errno));
        DevAssert(0 == 1);
    }
    if (epoll_ret == 0 && polling) {
        /* No data to read -> return */
        return;
    }

    itti_desc.tasks[task_id].epoll_nb_events = epoll_ret;

    for (i = 0; i < epoll_ret; i++) {
        /* Check if there is an event for ITTI for the event fd */
        if ((itti_desc.tasks[task_id].events[i].events & EPOLLIN) &&
            (itti_desc.tasks[task_id].events[i].data.fd == itti_desc.tasks[task_id].task_event_fd))
        {
            struct message_list_s *message;
            uint64_t sem_counter;

            /* Read will always return 1 */
            read (itti_desc.tasks[task_id].task_event_fd, &sem_counter, sizeof(sem_counter));

            if (lfds611_queue_dequeue (itti_desc.tasks[task_id].message_queue, (void **) &message) == 0) {
                /* No element in list -> this should not happen */
                DevParam(task_id, epoll_ret, 0);
            }
            *received_msg = message->msg;
            free (message);
            return;
        }
    }
}
#endif

void itti_receive_msg(task_id_t task_id, MessageDef **received_msg)
{
#if defined(ENABLE_EVENT_FD)
    itti_receive_msg_internal_event_fd(task_id, 0, received_msg);
#else
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);
    DevAssert(received_msg != NULL);

    // Lock the mutex to get exclusive access to the list
    pthread_mutex_lock (&itti_desc.tasks[task_id].message_queue_mutex);

    if (itti_desc.tasks[task_id].message_in_queue == 0) {
        ITTI_DEBUG("Message in queue[(%u:%s)] == 0, waiting\n", task_id, itti_get_task_name(task_id));
        // Wait while list == 0
        pthread_cond_wait (&itti_desc.tasks[task_id].message_queue_cond_var,
                           &itti_desc.tasks[task_id].message_queue_mutex);
        ITTI_DEBUG("Receiver queue[(%u:%s)] got new message notification\n",
                   task_id, itti_get_task_name(task_id));
    }

    if (!STAILQ_EMPTY (&itti_desc.tasks[task_id].message_queue)) {
        message_list_t *temp = STAILQ_FIRST (&itti_desc.tasks[task_id].message_queue);

        /* Update received_msg reference */
        *received_msg = temp->msg;

        /* Remove message from queue */
        STAILQ_REMOVE_HEAD (&itti_desc.tasks[task_id].message_queue, next_element);
        free (temp);
        itti_desc.tasks[task_id].message_in_queue--;
    }
    // Release the mutex
    pthread_mutex_unlock (&itti_desc.tasks[task_id].message_queue_mutex);
#endif
}

void itti_poll_msg(task_id_t task_id, MessageDef **received_msg) {
    DevCheck(task_id < itti_desc.task_max, task_id, itti_desc.task_max, 0);
    DevAssert(received_msg != NULL);

    *received_msg = NULL;

#if defined(ENABLE_EVENT_FD)
    itti_receive_msg_internal_event_fd(task_id, 1, received_msg);
#else
    if (itti_desc.tasks[task_id].message_in_queue != 0) {
        message_list_t *temp;

        // Lock the mutex to get exclusive access to the list
        pthread_mutex_lock (&itti_desc.tasks[task_id].message_queue_mutex);

        STAILQ_FOREACH (temp, &itti_desc.tasks[task_id].message_queue, next_element)
        {
            /* Update received_msg reference */
            *received_msg = temp->msg;

            /* Remove message from queue */
            STAILQ_REMOVE (&itti_desc.tasks[task_id].message_queue, temp, message_list_s, next_element);
            free (temp);
            itti_desc.tasks[task_id].message_in_queue--;

            ITTI_DEBUG(
                    "Receiver queue[(%u:%s)] got new message %s, number %lu\n",
                    task_id, itti_get_task_name(task_id), itti_desc.messages_info[temp->msg->header.messageId].name, temp->message_number);
            break;
        }

        // Release the mutex
        pthread_mutex_unlock (&itti_desc.tasks[task_id].message_queue_mutex);
    }
#endif

    if (*received_msg == NULL) {
        ITTI_DEBUG("No message in queue[(%u:%s)]\n", task_id, itti_get_task_name(task_id));
    }
}

int itti_create_task(task_id_t task_id, void *(*start_routine)(void *), void *args_p) {
    thread_id_t thread_id = TASK_GET_THREAD_ID(task_id);
    int result;

    DevAssert(start_routine != NULL);
    DevCheck(thread_id < itti_desc.thread_max, thread_id, itti_desc.thread_max, 0);
    DevCheck(itti_desc.threads[thread_id].task_state == TASK_STATE_NOT_CONFIGURED, task_id, thread_id,
             itti_desc.threads[thread_id].task_state);

    itti_desc.threads[thread_id].task_state = TASK_STATE_STARTING;

    result = pthread_create (&itti_desc.threads[thread_id].task_thread, NULL, start_routine, args_p);
    DevCheck(result>= 0, task_id, thread_id, result);

    /* Wait till the thread is completely ready */
    while (itti_desc.threads[thread_id].task_state != TASK_STATE_READY)
        ;
    return 0;
}

void itti_mark_task_ready(task_id_t task_id) {
    thread_id_t thread_id = TASK_GET_THREAD_ID(task_id);

    DevCheck(thread_id < itti_desc.thread_max, thread_id, itti_desc.thread_max, 0);

#if !defined(ENABLE_EVENT_FD)
    // Lock the mutex to get exclusive access to the list
    pthread_mutex_lock (&itti_desc.tasks[task_id].message_queue_mutex);
#endif

    itti_desc.threads[thread_id].task_state = TASK_STATE_READY;

#if !defined(ENABLE_EVENT_FD)
    // Release the mutex
    pthread_mutex_unlock (&itti_desc.tasks[task_id].message_queue_mutex);
#endif
}

void itti_exit_task(void) {
    pthread_exit (NULL);
}

void itti_terminate_tasks(task_id_t task_id) {
    // Sends Terminate signals to all tasks.
    itti_send_terminate_message (task_id);

    if (itti_desc.thread_handling_signals >= 0) {
        pthread_kill (itti_desc.thread_handling_signals, SIGUSR1);
    }

    pthread_exit (NULL);
}

int itti_init(task_id_t task_max, thread_id_t thread_max, MessagesIds messages_id_max, const task_info_t *tasks_info,
              const message_info_t *messages_info, const char * const messages_definition_xml, const char * const dump_file_name) {
    int i;
    itti_desc.message_number = 1;

    ITTI_DEBUG("Init: %d tasks, %d threads, %d messages\n", task_max, thread_max, messages_id_max);

    CHECK_INIT_RETURN(signal_init());

    /* Saves threads and messages max values */
    itti_desc.task_max = task_max;
    itti_desc.thread_max = thread_max;
    itti_desc.messages_id_max = messages_id_max;
    itti_desc.thread_handling_signals = -1;
    itti_desc.tasks_info = tasks_info;
    itti_desc.messages_info = messages_info;

    /* Allocates memory for tasks info */
    itti_desc.tasks = calloc (itti_desc.task_max, sizeof(task_desc_t));

    /* Allocates memory for threads info */
    itti_desc.threads = calloc (itti_desc.thread_max, sizeof(thread_desc_t));

    /* Initializing each queue and related stuff */
    for (i = TASK_FIRST; i < itti_desc.task_max; i++)
    {
#if defined(ENABLE_EVENT_FD)
        ITTI_DEBUG("Creating queue of message of size %u\n", itti_desc.tasks_info[i].queue_size);
        if (lfds611_queue_new(&itti_desc.tasks[i].message_queue, itti_desc.tasks_info[i].queue_size) < 0)
        {
            ITTI_ERROR("lfds611_queue_new failed for task %u\n", i);
            DevAssert(0 == 1);
        }

        itti_desc.tasks[i].epoll_fd = epoll_create1(0);
        if (itti_desc.tasks[i].epoll_fd == -1) {
            ITTI_ERROR("Failed to create new epoll fd: %s\n", strerror(errno));
            /* Always assert on this condition */
            DevAssert(0 == 1);
        }

        itti_desc.tasks[i].task_event_fd = eventfd(0, EFD_SEMAPHORE);
        if (itti_desc.tasks[i].task_event_fd == -1) {
            ITTI_ERROR("eventfd failed: %s\n", strerror(errno));
            /* Always assert on this condition */
            DevAssert(0 == 1);
        }

        itti_desc.tasks[i].nb_events = 1;

        itti_desc.tasks[i].events = malloc(sizeof(struct epoll_event));

        itti_desc.tasks[i].events->events  = EPOLLIN;
        itti_desc.tasks[i].events->data.fd = itti_desc.tasks[i].task_event_fd;

        /* Add the event fd to the list of monitored events */
        if (epoll_ctl(itti_desc.tasks[i].epoll_fd, EPOLL_CTL_ADD,
            itti_desc.tasks[i].task_event_fd, itti_desc.tasks[i].events) != 0)
        {
            ITTI_ERROR("epoll_ctl failed: %s\n", strerror(errno));
            /* Always assert on this condition */
            DevAssert(0 == 1);
        }
#else
        STAILQ_INIT (&itti_desc.tasks[i].message_queue);
        itti_desc.tasks[i].message_in_queue = 0;

        // Initialize mutexes
        pthread_mutex_init (&itti_desc.tasks[i].message_queue_mutex, NULL);

        // Initialize Cond vars
        pthread_cond_init (&itti_desc.tasks[i].message_queue_cond_var, NULL);
#endif
    }

    /* Initializing each thread */
    for (i = THREAD_FIRST; i < itti_desc.thread_max; i++)
    {
        itti_desc.threads[i].task_state = TASK_STATE_NOT_CONFIGURED;
    }

    itti_dump_init (messages_definition_xml, dump_file_name);

    CHECK_INIT_RETURN(timer_init ());

    return 0;
}

void itti_wait_tasks_end(void) {
    int end = 0;
    int thread_id;
    task_id_t task_id;
    int ready_tasks;
    int result;
    int retries = 10;

    itti_desc.thread_handling_signals = pthread_self ();

    /* Handle signals here */
    while (end == 0) {
        signal_handle (&end);
    }

    do {
        ready_tasks = 0;

        task_id = TASK_FIRST;
        for (thread_id = THREAD_FIRST; thread_id < itti_desc.task_max; thread_id++) {
            /* Skip tasks which are not running */
            if (itti_desc.threads[thread_id].task_state == TASK_STATE_READY) {
                while (thread_id != TASK_GET_THREAD_ID(task_id))
                {
                    task_id++;
                }

                result = pthread_tryjoin_np (itti_desc.threads[thread_id].task_thread, NULL);

                ITTI_DEBUG("Thread %s join status %d\n", itti_get_task_name(task_id), result);

                if (result == 0) {
                    /* Thread has terminated */
                    itti_desc.threads[thread_id].task_state = TASK_STATE_ENDED;
                }
                else {
                    /* Thread is still running, count it */
                    ready_tasks++;
                }
            }
        }
        if (ready_tasks > 0) {
            usleep (100 * 1000);
        }
    } while ((ready_tasks > 0) && (retries--));

    if (ready_tasks > 0) {
        ITTI_DEBUG("Some threads are still running, force exit\n");
        exit (0);
    }

    itti_dump_exit();
}

void itti_send_terminate_message(task_id_t task_id) {
    MessageDef *terminate_message_p;

    terminate_message_p = itti_alloc_new_message (task_id, TERMINATE_MESSAGE);

    itti_send_broadcast_message (terminate_message_p);
}
