/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/printk.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
   int byteIndex = 0;
   int bufferOffset = 0;
   
    if(buffer->full)
    {
        bufferOffset = buffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else
    {
        bufferOffset = buffer->in_offs;
    }

    for(int i = buffer->out_offs; i < bufferOffset; i++)
    {
        if(byteIndex + buffer->entry[(i % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)].size > char_offset)
        {
            size_t byteOffset = (char_offset - byteIndex);
            *entry_offset_byte_rtn = (byteOffset);
            for(int j = 0; j < buffer->entry[i%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size; j++)
            {
                printk(KERN_INFO "%c\n", buffer->entry[i%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].buffptr[j]);
            }
            return &(buffer->entry[(i % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)]);
        }
        else
        {
            byteIndex += buffer->entry[(i % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)].size;
        }
    }

   *entry_offset_byte_rtn = 0;
    struct aesd_buffer_entry* bufferPtr = NULL;
    return bufferPtr;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    if(add_entry->buffptr == NULL)
    {
        return;
    }

    if(buffer->full)
    {
        kfree(buffer->entry[(buffer->out_offs)].buffptr);
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;

    for(int i = 0; i < buffer->entry[buffer->in_offs].size; i++)
    {
        printk(KERN_INFO "%c\n", buffer->entry[buffer->in_offs].buffptr[i]);
    }
    
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if(buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
    buffer->out_offs = 0;
    buffer->in_offs = 0;
}