/**
 ******************************************************************************
 *
 * @file rwnx_fw_trace.c
 *
 * Copyright (C) RivieraWaves 2017-2019
 *
 ******************************************************************************
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include "rwnx_fw_trace.h"

#define RWNX_FW_TRACE_HEADER_LEN 4
#define RWNX_FW_TRACE_HEADER_FMT "ts=%12u ID=%8d"
#define RWNX_FW_TRACE_HEADER_ASCII_LEN (3 + 12 + 4 + 8)
#define RWNX_FW_TRACE_PARAM_FMT ", %5d"
#define RWNX_FW_TRACE_PARAM_ASCII_LEN (7)

#define RWNX_FW_TRACE_NB_PARAM(a) ((*a >> 8) & 0xff)
#define RWNX_FW_TRACE_ID(a) (uint32_t)(((a[0] & 0xff) << 16) + a[1])
#define RWNX_FW_TRACE_ENTRY_SIZE(a) (RWNX_FW_TRACE_NB_PARAM(a) + \
                                     RWNX_FW_TRACE_HEADER_LEN)

#define RWNX_FW_TRACE_READY  0x1234
#define RWNX_FW_TRACE_LOCKED 0xdead
#define RWNX_FW_TRACE_LOCKED_HOST 0x0230
#define RWNX_FW_TRACE_LAST_ENTRY 0xffff

#define RWNX_FW_TRACE_RESET "*** RESET ***\n"
#define RWNX_FW_TRACE_RESET_SIZE sizeof(RWNX_FW_TRACE_RESET) - 1 // don't count '\0'

static int trace_last_reset=0;

static const int startup_max_to = 500;

#define RWNX_FW_TRACE_CHECK_INT_MS 1000


/**
 * rwnx_fw_trace_work() - Work function to check for new traces
 *                        process function for &struct rwnx_fw_trace.work
 *
 * @ws: work structure
 *
 * Check if new traces are available in the shared buffer, by comparing current
 * end index with end index in the last check. If so wake up pending threads,
 * otherwise re-schedule the work is there are still some pending readers.
 *
 * Note: If between two check firmware exactly write one buffer of trace then
 * those traces will be lost. Fortunately this is very unlikely to happen.
 *
 * Note: Even if wake_up doesn't actually wake up threads (because condition
 * failed), calling waitqueue_active just after will still return false.
 * Fortunately this should never happen (new trace should always trigger the
 * waiting condition) otherwise it may be needed to re-schedule the work after
 * wake_up.
 */
static void rwnx_fw_trace_work(struct work_struct *ws)
{
    struct delayed_work *dw = container_of(ws, struct delayed_work, work);
    struct rwnx_fw_trace *trace = container_of(dw, struct rwnx_fw_trace, work);

    if (trace->closing ||
        (!rwnx_fw_trace_empty(&trace->buf) &&
         trace->last_read_index != *trace->buf.end)) {
        trace->last_read_index = *trace->buf.end;
        wake_up_interruptible(&trace->queue);
        return;
    }

    if (waitqueue_active(&trace->queue) && !delayed_work_pending(dw)) {
        schedule_delayed_work(dw, msecs_to_jiffies(RWNX_FW_TRACE_CHECK_INT_MS));
    }
}

/**
 * rwnx_fw_trace_buf_lock() - Lock trace buffer for firmware
 *
 * @shared_buf: Pointer to shared buffer
 *
 * Very basic synchro mechanism so that fw do not update trace buffer while host
 * is reading it. Not safe to race condition if host and fw read lock value at
 * the "same" time.
 */
static void rwnx_fw_trace_buf_lock(struct rwnx_fw_trace_buf *shared_buf)
{
  wait:
    while(*shared_buf->lock == RWNX_FW_TRACE_LOCKED) {}
    *shared_buf->lock &= RWNX_FW_TRACE_LOCKED_HOST;

    /* re-read to reduce race condition window */
    if (*shared_buf->lock == RWNX_FW_TRACE_LOCKED)
        goto wait;
}

/**
 * rwnx_fw_trace_buf_unlock() - Unlock trace buffer for firmware
 *
 * @shared_buf: Pointer to shared buffer
 *
 */
static void rwnx_fw_trace_buf_unlock(struct rwnx_fw_trace_buf *shared_buf)
{
    *shared_buf->lock = RWNX_FW_TRACE_READY;
}

/**
 * rwnx_fw_trace_buf_init() - Initialize rwnx_fw_trace_buf structure
 *
 * @shared_buf: Structure to initialize
 * @ipc: Pointer to IPC shard structure that contains trace buffer info
 *
 *
 * Return: 0 if initialization succeed, <0 otherwise. It can only fail if
 * trace feature is not enabled in the firmware (or buffer is corrupted).
 */
int rwnx_fw_trace_buf_init(struct rwnx_fw_trace_buf *shared_buf,
                           struct rwnx_fw_trace_ipc_desc *ipc)
{
    uint16_t lock_status = ipc->pattern;

    if ((lock_status != RWNX_FW_TRACE_READY &&
         lock_status != RWNX_FW_TRACE_LOCKED)) {
        shared_buf->data = NULL;
        return -ENOENT;
    }

    /* Buffer starts <offset> bytes from the location of ipc->offset */
    shared_buf->data = (uint16_t *)((uint8_t *)(&ipc->offset) + ipc->offset);
    shared_buf->lock = &ipc->pattern;
    shared_buf->size = ipc->size;
    shared_buf->start = &ipc->start;
    shared_buf->end = &ipc->end;
    shared_buf->reset_idx = ++trace_last_reset;

    /* backward compatibilty with firmware without trace activation */
    if ((ipc->nb_compo >> 16) == RWNX_FW_TRACE_READY) {
        shared_buf->nb_compo = ipc->nb_compo & 0xffff;
        shared_buf->compo_table = (uint32_t *)((uint8_t *)(&ipc->offset_compo)
                                               + ipc->offset_compo);
    } else {
        shared_buf->nb_compo = 0;
        shared_buf->compo_table = NULL;
    }

    return 0;
}

/**
 * rwnx_fw_trace_init() - Initialize rwnx_fw_trace structure
 *
 * @trace: Structure to initialize
 * @ipc: Pointer to IPC shard structure that contains trace buffer info
 *
 * Return: 0 if initialization succeed, <0 otherwise. It can only fail if
 * trace feature is not enabled in the firmware (or buffer is corrupted).
 */
int rwnx_fw_trace_init(struct rwnx_fw_trace *trace,
                       struct rwnx_fw_trace_ipc_desc *ipc)
{
    if (rwnx_fw_trace_buf_init(&trace->buf, ipc))
        return -ENOENT;

    INIT_DELAYED_WORK(&trace->work, rwnx_fw_trace_work);
    init_waitqueue_head(&trace->queue);
    mutex_init(&trace->mutex);
    trace->closing = false;
    return 0;
}

/**
 * rwnx_fw_trace_deinit() - De-initialization before releasing rwnx_fw_trace
 *
 * @trace: fw trace control structure
 */
void rwnx_fw_trace_deinit(struct rwnx_fw_trace *trace)
{
    trace->closing = true;
    flush_delayed_work(&trace->work);
    trace->buf.data = NULL;
}

/**
 * rwnx_fw_trace_reset_local() - Reset local buffer pointer/status
 *
 * @local_buf: structure to reset
 */
static void rwnx_fw_trace_reset_local(struct rwnx_fw_trace_local_buf *local_buf)
{
    local_buf->read = local_buf->data;
    local_buf->write = local_buf->data;
    local_buf->nb_entries = 0;
    local_buf->free_space = local_buf->size;
    local_buf->last_read = NULL;
    local_buf->reset_idx = 0;
    local_buf->show_reset = NULL;
}

/**
 * rwnx_fw_trace_alloc_local() - Allocate a local buffer and initialize
 * rwnx_fw_trace_local_buf structure
 *
 * @local_buf: structure to initialize
 * @size: Size of the buffer to allocate
 *
 * @local structure is initialized to use the allocated buffer.
 *
 * Return: 0 if allocation succeed and <0 otherwise.
 */
int rwnx_fw_trace_alloc_local(struct rwnx_fw_trace_local_buf *local_buf,
                              int size)
{
    local_buf->data = kmalloc(size * sizeof(uint16_t), GFP_KERNEL);
    if (!local_buf->data) {
        return -ENOMEM;
    }

    local_buf->data_end = local_buf->data + size;
    local_buf->size = size;
    rwnx_fw_trace_reset_local(local_buf);
    return 0;
}

/**
 * rwnx_fw_trace_free_local() - Free local buffer
 *
 * @local_buf: structure containing buffer pointer to free.
 */
void rwnx_fw_trace_free_local(struct rwnx_fw_trace_local_buf *local_buf)
{
    if (local_buf->data)
        kfree(local_buf->data);
    local_buf->data = NULL;
}

/**
 * rwnx_fw_trace_strlen() - Return buffer size needed convert a trace entry into
 * string
 *
 * @entry: Pointer on trace entry
 *
 */
static inline int rwnx_fw_trace_strlen(uint16_t *entry)
{
    return (RWNX_FW_TRACE_HEADER_ASCII_LEN +
            (RWNX_FW_TRACE_NB_PARAM(entry) * RWNX_FW_TRACE_PARAM_ASCII_LEN) +
            1); /* for \n */
}

/**
 * rwnx_fw_trace_to_str() - Convert one trace entry to a string
 *
 * @trace: Poitner to the trace entry
 * @buf: Buffer for the string
 * @size: Size of the string buffer, updated with the actual string size
 *
 * Return: pointer to the next tag entry.
 */
static uint16_t *rwnx_fw_trace_to_str(uint16_t *trace, char *buf, size_t *size)
{
    uint32_t ts, id;
    int nb_param;
    int res, buf_idx = 0, left = *size;

    id = RWNX_FW_TRACE_ID(trace);
    nb_param = RWNX_FW_TRACE_NB_PARAM(trace);

    trace +=2;
    ts = *trace++;
    ts <<= 16;
    ts += *trace++;

    res = scnprintf(&buf[buf_idx], left, RWNX_FW_TRACE_HEADER_FMT, ts, id);
    buf_idx += res;
    left    -= res;

    while (nb_param > 0) {
        res = scnprintf(&buf[buf_idx], left, RWNX_FW_TRACE_PARAM_FMT, *trace++);
        buf_idx += res;
        left    -= res;
        nb_param--;
    }

    res = scnprintf(&buf[buf_idx], left, "\n");
    left -= res;
    *size = (*size - left);

    return trace;
}

/**
 * rwnx_fw_trace_copy_entry() - Copy one trace entry in a local buffer
 *
 * @local_buf: Local buffer to copy trace into
 * @trace_entry: Pointer to the trace entry (in shared memory) to copy
 * @size: Size, in 16bits words, of the trace entry
 *
 * It is assumed that local has enough contiguous free-space available in
 * local buffer (i.e. from local_buf->write) to copy this trace.
 */
static void rwnx_fw_trace_copy_entry(struct rwnx_fw_trace_local_buf *local_buf,
                                     uint16_t *trace_entry, int size)
{
    uint16_t *write = local_buf->write;
    uint16_t *read = trace_entry;
    int i;

    for (i = 0; i < size; i++) {
        *write++ = *read++;
    }

    if (write >= local_buf->data_end)
        local_buf->write = local_buf->data;
    else
        local_buf->write = write;

    local_buf->free_space -= size;
    local_buf->last_read = trace_entry;
    local_buf->last_read_value = *trace_entry;
    local_buf->nb_entries++;
}

/**
 * rwnx_fw_trace_copy() - Copy trace entries from shared to local buffer
 *
 * @trace_buf: Pointer to shard buffer
 * @local_buf: Pointer to local buffer
 *
 * Copy has many trace entry as possible from shared buffer to local buffer
 * without overwriting traces in local buffer.
 *
 * Return: number of trace entries copied to local buffer
 */
static int rwnx_fw_trace_copy(struct rwnx_fw_trace *trace,
                              struct rwnx_fw_trace_local_buf *local_buf)
{
    struct rwnx_fw_trace_buf *trace_buf = &trace->buf;
    uint16_t *ptr, *ptr_end, *ptr_limit;
    int entry_size, ret = 0;

    if (mutex_lock_interruptible(&trace->mutex))
        return 0;

    /* reset last_read ptr if shared buffer has been reset */
    if (local_buf->reset_idx != trace_buf->reset_idx) {
        local_buf->show_reset = local_buf->write;
        local_buf->reset_idx = trace_buf->reset_idx;
        local_buf->last_read = NULL;
    }

    rwnx_fw_trace_buf_lock(trace_buf);

    ptr_end = trace_buf->data + *trace_buf->end;
    if (rwnx_fw_trace_empty(trace_buf) || (ptr_end == local_buf->last_read))
        goto end;
    ptr_limit = trace_buf->data + trace_buf->size;

    if (local_buf->last_read &&
        (local_buf->last_read_value == *local_buf->last_read)) {
        ptr = local_buf->last_read;
        ptr += RWNX_FW_TRACE_ENTRY_SIZE(ptr);
    } else {
        ptr = trace_buf->data + *trace_buf->start;
    }

    while (1) {

        if ((ptr == ptr_limit) || (*ptr == RWNX_FW_TRACE_LAST_ENTRY))
             ptr = trace_buf->data;

        entry_size = RWNX_FW_TRACE_ENTRY_SIZE(ptr);

        if ((ptr + entry_size) > ptr_limit) {
            pr_err("Corrupted trace buffer\n");
            _rwnx_fw_trace_reset(trace, false);
            break;
        } else if (entry_size > local_buf->size) {
            pr_err("FW_TRACE local buffer too small, trace skipped");
            goto next_entry;
        }

        if (local_buf->free_space >= entry_size) {
            int contiguous = local_buf->data_end - local_buf->write;

            if ((local_buf->write < local_buf->read) || contiguous >= entry_size) {
                /* enough contiguous memory from local_buf->write */
                rwnx_fw_trace_copy_entry(local_buf, ptr, entry_size);
                ret++;
            } else if ((local_buf->free_space - contiguous) >= entry_size) {
                /* not enough contiguous from local_buf->write but enough
                   from local_buf->data */
                *local_buf->write = RWNX_FW_TRACE_LAST_ENTRY;
                if (local_buf->show_reset == local_buf->write)
                    local_buf->show_reset = local_buf->data;
                local_buf->write = local_buf->data;
                local_buf->free_space -= contiguous;
                rwnx_fw_trace_copy_entry(local_buf, ptr, entry_size);
                ret++;
            } else {
                /* not enough contiguous memory */
                goto end;
            }
        } else {
            goto end;
        }

        if (ptr == ptr_end)
            break;

      next_entry:
        ptr += entry_size;
    }

  end:
    rwnx_fw_trace_buf_unlock(trace_buf);
    mutex_unlock(&trace->mutex);
    return ret;
}

/**
 * rwnx_fw_trace_read_local() - Read trace from local buffer and convert it to
 * string in a user buffer
 *
 * @local_buf: Pointer to local buffer
 * @user_buf: Pointer to user buffer
 * @size: Size of the user buffer
 *
 * Read traces from shared buffer to write them in the user buffer after string
 * conversion. Stop when no more space in user buffer or no more trace to read.
 *
 * Return: The size written in the user buffer.
 */
static size_t rwnx_fw_trace_read_local(struct rwnx_fw_trace_local_buf *local_buf,
                                       char __user *user_buf, size_t size)
{
    uint16_t *ptr;
    char str[1824]; // worst case 255 params
    size_t str_size;
    int entry_size;
    size_t res = 0 , remain = size, not_cpy = 0;

    if (!local_buf->nb_entries)
        return res;

    ptr = local_buf->read;
    while(local_buf->nb_entries && !not_cpy) {

        if (local_buf->show_reset == ptr) {
            if (remain < RWNX_FW_TRACE_RESET_SIZE)
                break;

            local_buf->show_reset = NULL;
            not_cpy = copy_to_user(user_buf + res, RWNX_FW_TRACE_RESET,
                                   RWNX_FW_TRACE_RESET_SIZE);
            res += (RWNX_FW_TRACE_RESET_SIZE - not_cpy);
            remain -= (RWNX_FW_TRACE_RESET_SIZE - not_cpy);
        }

        if (remain < rwnx_fw_trace_strlen(ptr))
            break;

        entry_size = RWNX_FW_TRACE_ENTRY_SIZE(ptr);
        str_size = sizeof(str);
        ptr = rwnx_fw_trace_to_str(ptr, str, &str_size);
        not_cpy = copy_to_user(user_buf + res, str, str_size);
        str_size -= not_cpy;
        res += str_size;
        remain -= str_size;

        local_buf->nb_entries--;
        local_buf->free_space += entry_size;
        if (ptr >= local_buf->data_end) {
            ptr = local_buf->data;
        } else if (*ptr == RWNX_FW_TRACE_LAST_ENTRY) {
            local_buf->free_space += local_buf->data_end - ptr;
            ptr = local_buf->data;
        }
        local_buf->read = ptr;
    }

    /* read all entries reset pointer */
    if ( !local_buf->nb_entries) {

        local_buf->write = local_buf->read = local_buf->data;
        local_buf->free_space = local_buf->size;
    }

    return res;
}

/**
 * rwnx_fw_trace_read() - Update local buffer from shared buffer and convert
 * local buffer to string in user buffer
 *
 * @trace: Fw trace control structure
 * @local_buf: Local buffer to update and read from
 * @dont_wait: Indicate whether function should wait or not for traces before
 * returning
 * @user_buf: Pointer to user buffer
 * @size: Size of the user buffer
 *
 * Read traces from shared buffer to write them in the user buffer after string
 * conversion. Stop when no more space in user buffer or no more trace to read.
 *
 * Return: The size written in the user buffer if > 0, -EAGAIN if there is no
 * new traces and dont_wait is set and -ERESTARTSYS if signal has been
 * received while waiting for new traces.
 */
size_t rwnx_fw_trace_read(struct rwnx_fw_trace *trace,
                          struct rwnx_fw_trace_local_buf *local_buf,
                          bool dont_wait, char __user *user_buf, size_t size)
{
    size_t res = 0;

    rwnx_fw_trace_copy(trace, local_buf);

    while(!local_buf->nb_entries) {
        int last_index;

        if (dont_wait)
            return -EAGAIN;

        /* no trace, schedule work to periodically check trace buffer */
        if (!delayed_work_pending(&trace->work)) {
            trace->last_read_index = *trace->buf.end;
            schedule_delayed_work(&trace->work,
                                  msecs_to_jiffies(RWNX_FW_TRACE_CHECK_INT_MS));
        }

        /* and wait for traces */
        last_index = *trace->buf.end;
        if (wait_event_interruptible(trace->queue,
                                     (trace->closing ||
                                      (last_index != *trace->buf.end)))) {
            return -ERESTARTSYS;
        }

        if (trace->closing)
            return 0;

        rwnx_fw_trace_copy(trace, local_buf);
    }

    /* copy as many traces as possible in user buffer */
    while (1) {
        size_t read;
        read = rwnx_fw_trace_read_local(local_buf, user_buf + res, size - res);
        res += read;
        rwnx_fw_trace_copy(trace, local_buf);
        if (!read)
            break;
    }

    return res;
}


/**
 * _rwnx_fw_trace_dump() - Dump shared trace buffer in kernel buffer
 *
 * @trace_buf: Pointer to shared trace buffer;
 *
 * Called when error is detected, output trace on dmesg directly read from
 * shared memory
 */
void _rwnx_fw_trace_dump(struct rwnx_fw_trace_buf *trace_buf)
{
    uint16_t *ptr, *ptr_end, *ptr_limit, *next_ptr;
    char buf[1824]; // worst case 255 params
    size_t size;

    if (!trace_buf->data || rwnx_fw_trace_empty(trace_buf))
        return;

    rwnx_fw_trace_buf_lock(trace_buf);

    ptr = trace_buf->data + *trace_buf->start;
    ptr_end = trace_buf->data + *trace_buf->end;
    ptr_limit = trace_buf->data + trace_buf->size;

    while (1) {
        size = sizeof(buf);
        next_ptr = rwnx_fw_trace_to_str(ptr, buf, &size);
        pr_info("%s", buf);

        if (ptr == ptr_end) {
            break;
        } else if ((next_ptr == ptr_limit) ||
                   (*next_ptr == RWNX_FW_TRACE_LAST_ENTRY)) {
            ptr = trace_buf->data;
        } else if (next_ptr > ptr_limit) {
            pr_err("Corrupted trace buffer\n");
            break;
        } else {
            ptr = next_ptr;
        }
    }

    rwnx_fw_trace_buf_unlock(trace_buf);
}

/**
 * _rwnx_fw_trace_reset() - Reset trace buffer at firmware level
 *
 * @trace: Pointer to shared trace buffer;
 * @bool: Indicate if mutex must be aquired before
 */
int _rwnx_fw_trace_reset(struct rwnx_fw_trace *trace, bool lock)
{
    struct rwnx_fw_trace_buf *trace_buf = &trace->buf;

    if (lock && mutex_lock_interruptible(&trace->mutex))
        return -ERESTARTSYS;

    if (trace->buf.data) {
        rwnx_fw_trace_buf_lock(trace_buf);
        *trace_buf->start = 0;
        *trace_buf->end = trace_buf->size + 1;
        trace_buf->reset_idx = ++trace_last_reset;
        rwnx_fw_trace_buf_unlock(trace_buf);
    }

    if (lock)
        mutex_unlock(&trace->mutex);
    return 0;
}

/**
 * rwnx_fw_trace_get_trace_level() - Get trace level for a given component
 *
 * @trace: Pointer to shared trace buffer;
 * @compo_id: Index of the componetn in the table
 *
 * Return: The trace level set for the given component, 0 if component index
 * is invalid.
 */
static uint32_t rwnx_fw_trace_get_trace_level(struct rwnx_fw_trace_buf *trace_buf,
                                              unsigned int compo_id)
{
    if (compo_id >= trace_buf->nb_compo)
        return 0;
    return trace_buf->compo_table[compo_id];
}

/**
 * rwnx_fw_trace_set_trace_level() - Set trace level for a given component
 *
 * @trace_buf: Pointer to shared trace buffer;
 * @compo_id: Index of the componetn in the table
 * @level: Trace level to set
 *
 * Set all components if compo_id is equals to the number of component and
 * does nothing if it is greater.
 */
static void rwnx_fw_trace_set_trace_level(struct rwnx_fw_trace_buf *trace_buf,
                                          unsigned int compo_id, uint32_t level)
{
    if (compo_id > trace_buf->nb_compo)
        return;

    if (compo_id == trace_buf->nb_compo) {
        int i;
        for (i = 0; i < trace_buf->nb_compo; i++) {
            trace_buf->compo_table[i] = level;
        }
    } else {
        trace_buf->compo_table[compo_id] = level;
    }
}

/**
 * rwnx_fw_trace_level_read() - Write current trace level in a user buffer
 *                              as a string
 *
 * @trace: Fw trace control structure
 * @user_buf: Pointer to user buffer
 * @len: Size of the user buffer
 * @ppos: position offset
 *
 * Return: Number of bytes written in user buffer if > 0, error otherwise
 */
size_t rwnx_fw_trace_level_read(struct rwnx_fw_trace *trace,
                                char __user *user_buf, size_t len, loff_t *ppos)
{
    struct rwnx_fw_trace_buf *trace_buf = &trace->buf;
    size_t res = 0;
    int i, size;
    char *buf;

    size = trace_buf->nb_compo * 16;
    buf = kmalloc(size, GFP_KERNEL);
    if (buf == NULL)
        return 0;

    if (mutex_lock_interruptible(&trace->mutex)) {
        kfree(buf);
        return -ERESTARTSYS;
    }

    for (i = 0 ; i < trace_buf->nb_compo ; i ++) {
        res += scnprintf(&buf[res], size - res, "%3d:0x%08x\n", i,
                         rwnx_fw_trace_get_trace_level(trace_buf, i));
    }
    mutex_unlock(&trace->mutex);

    res = simple_read_from_buffer(user_buf, len, ppos, buf, res);

    kfree(buf);
    return res;
}

/**
 * rwnx_fw_trace_level_write() - Read trace level from  a user buffer provided
 *                               as a string and applyt them.
 *
 * @trace: Fw trace control structure
 * @user_buf: Pointer to user buffer
 * @len: Size of the user buffer
 *
 * trace level must be provided in the following form:
 * <compo_id>:<trace_level> where <compo_id> is in decimal notation and
 * <trace_level> in decical or hexadecimal notation.
 * Several trace level can be provided, separated by space,tab or new line.
 *
 * Return: Number of bytes read form user buffer if > 0, error otherwise
 */
size_t rwnx_fw_trace_level_write(struct rwnx_fw_trace *trace,
                                 const char __user *user_buf, size_t len)
{
    struct rwnx_fw_trace_buf *trace_buf = &trace->buf;
    char *buf, *token, *next;

    buf = kmalloc(len + 1, GFP_KERNEL);
    if (buf == NULL)
        return -ENOMEM;

    if (copy_from_user(buf, user_buf, len)) {
        kfree(buf);
        return -EFAULT;
    }
    buf[len] = '\0';

    if (mutex_lock_interruptible(&trace->mutex)) {
        kfree(buf);
        return -ERESTARTSYS;
    }

    next = buf;
    token = strsep(&next, " \t\n");
    while (token) {
        unsigned int compo, level;
        if ((sscanf(token, "%d:0x%x", &compo, &level) == 2)||
            (sscanf(token, "%d:%d", &compo, &level) == 2)) {
            rwnx_fw_trace_set_trace_level(trace_buf, compo, level);
        }

        token = strsep(&next, " \t");
    }
    mutex_unlock(&trace->mutex);

    kfree(buf);
    return len;
}

/**
 * rwnx_fw_trace_config_filters() - Update FW trace filters
 *
 * @trace_buf: Pointer to shared buffer
 * @ipc: Pointer to IPC shared structure that contains trace buffer info
 * @ftl: Firmware trace level
 *
 * Return: 0 if the trace filters are successfully updated, <0 otherwise.
 */
int rwnx_fw_trace_config_filters(struct rwnx_fw_trace_buf *trace_buf,
                                 struct rwnx_fw_trace_ipc_desc *ipc, char *ftl)
{
    int to;
    char *next, *token;

    to = 0;
    while((ipc->pattern != RWNX_FW_TRACE_READY) && (to < startup_max_to))
    {
        msleep(50);
        to += 50;
    }

    if (rwnx_fw_trace_buf_init(trace_buf, ipc))
        return -ENOENT;

    next = ftl;
    token = strsep(&next, " ");
    while(token)
    {
        unsigned int compo, ret, id, level = 0;
        char action;

        if ((sscanf(token, "%d%c0x%x", &compo, &action, &id) == 3)||
            (sscanf(token, "%d%c%d", &compo, &action, &id) == 3))
        {
            if(action == '=')
            {
                level = id;
            }
            else
            {
                ret = rwnx_fw_trace_get_trace_level(trace_buf, compo);
                if(action == '+')
                    level = (ret | id);
                else if (action == '-')
                    level = (ret & ~id);
            }
            rwnx_fw_trace_set_trace_level(trace_buf, compo, level);
        }

        token = strsep(&next, " ");
    }

    return 0;
}

