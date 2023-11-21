/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/******************************************************************************
*
*	Module Name:
*	    bt_skbuff.h
*
*	Abstract:
*	    Data buffer managerment through whole bluetooth stack.
*
*	Notes:
*	      To reduce memory copy when pass data buffer to other layers,
*       AIC_BUFFER is designed referring to linux socket buffer.
*       But I still wonder its effect, since AIC_BUFFER is much bigger
*       than original data buffer.AIC_BUFFER will reduce its member if
*       it would not reach what i had expected.
*
******************************************************************************/


#ifndef BT_SKBUFF_H
#define BT_SKBUFF_H
#include "bt_list.h"
#include <stdbool.h>

#ifndef EXTERN
#define EXTERN
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif
/*----------------------------------------------------------------------------------
    CONSTANT DEFINITION
----------------------------------------------------------------------------------*/
#define AIC_CONTEXT_SIZE 12

#define RTB_QUEUE_ID_LENGTH          64

/*----------------------------------------------------------------------------------
    STRUCTURE DEFINITION
----------------------------------------------------------------------------------*/
/**
    Aic buffer definition
      Head -->|<---Data--->|<-----Length------>| <---End
                     _________________________________
                    |_____________|___________________|
                    |<-headroom->|<--RealDataBuffer-->|

    Compared to socket buffer, there exists no tail and end pointer and tailroom as tail is rarely used in bluetooth stack
    \param List             : List structure used to list same type aic buffer and manipulate aic buffer like list.
    \param Head           : Pointer to truely allocated data buffer. It point to the headroom
    \param Data           : Pointer to real data buffer.
    \param Length        : currently data length
    \param HeadRoom  : Record initialize headroom size.
    \param RefCount    : Reference count. zero means able to be freed, otherwise somebody is handling it.
    \param Priv            : Reserved for multi-device support. Record Hci pointer which will handles this packet
    \param Contest      : Control buffer, put private variables here.
*/
typedef struct _AIC_BUFFER
{
    RT_LIST_ENTRY List;
    uint8_t *Head;
    uint8_t *Data;
    uint8_t *Tail;
    uint8_t *End;
    uint32_t Length;
    uint32_t HeadRoom;
//    RT_U16 TailRoom;
    signed char   RefCount;

    void* Priv;
    uint8_t Context[AIC_CONTEXT_SIZE];
}AIC_BUFFER, *PAIC_BUFFER;

/**
    AIC_BUFFER Control Buffer Context
    \param  PacketType      : HCI data types, Command/Acl/...
    \param  LastFrag          : Is Current Acl buffer the last fragment.(0 for no, 1 for yes)
    \param  TxSeq             : Current packet tx sequence
    \param  Retries            : Current packet retransmission times
    \param  Sar                 : L2cap control field segmentation and reassembly bits
*/
struct BT_RTB_CONTEXT{
    uint8_t   PacketType;
    uint16_t Handle;
};

///definition to get aic_buffer's control buffer context pointer
#define BT_CONTEXT(_Rtb) ((struct BT_RTB_CONTEXT *)((_Rtb)->Context))

/**
    Since RTBs are always used into/from list, so abstract this struct and provide APIs to easy process on RTBs
*/
typedef struct _RTB_QUEUE_HEAD  RTB_QUEUE_HEAD;
/*----------------------------------------------------------------------------------
    EXTERNAL FUNCTION
----------------------------------------------------------------------------------*/
/**
    Allocate a AIC_BUFFER with specified data length and reserved headroom.
    If caller does not know actual headroom to reserve for further usage, specify it to zero to use default value.
    \param [IN]     Length            <uint32_t>        : current data buffer length to allcated
    \param [IN]     HeadRoom     <uint32_t>         : if caller knows reserved head space, set it; otherwise set 0 to use default value
    \return pointer to AIC_BUFFER if succeed, null otherwise
*/
AIC_BUFFER*
RtbAllocate(
    IN uint32_t Length,
    IN uint32_t HeadRoom
    );

/**
    Free specified Aic_buffer
    \param [IN]     AicBuffer            <AIC_BUFFER*>        : buffer to free
*/
void
RtbFree(
    IN AIC_BUFFER* AicBuffer
    );

/**
    increament reference count
*/
void
RtbIncreaseRefCount(
    IN AIC_BUFFER* AicBuffer
);

/**
    Recycle a aic_buffer after its usage if specified rtb could
    if rtb total length is not smaller than specified rtbsize to be recycled for, it will succeeded recycling
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : buffer to recycle
    \param [IN]             RtbSize              <uint32_t>                 : size of buffer to be recycled for
*/
/*
BOOLEAN
RtbCheckRecycle(
    IN OUT AIC_BUFFER* AicBuffer,
    IN uint32_t   RtbSize
    );
*/
/**
    Add a specified length protocal header to the start of data buffer hold by specified aic_buffer.
    This function extends used data area of the buffer at the buffer start.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to add
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the first byte of the extra data is returned
*/
uint8_t*
RtbAddHead(
    IN OUT AIC_BUFFER* AicBuffer,
    IN uint32_t                 Length
    );

/**
    Remove a specified length data from the start of data buffer hold by specified aic_buffer.
    This function returns the memory to the headroom.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to remove
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the next data in the buffer is returned, usually useless
*/
unsigned char
RtbRemoveHead(
    IN OUT AIC_BUFFER* AicBuffer,
    IN uint32_t                 Length
    );

/**
    Add a specified length protocal header to the end of data buffer hold by specified aic_buffer.
    This function extends used data area of the buffer at the buffer end.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to add
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the first byte of the extra data is returned
*/
EXTERN uint8_t*
RtbAddTail(
    IN OUT AIC_BUFFER* AicBuffer,
    IN uint32_t                 Length
    );

/**
    Remove a specified length data from the end of data buffer hold by specified aic_buffer.
*/
EXTERN unsigned char
RtbRemoveTail(
    IN OUT AIC_BUFFER * AicBuffer,
    IN     uint32_t       Length
);

/**
    Initialize a rtb queue.
    \return  Initilized rtb queue if succeed, otherwise NULL
*/
EXTERN RTB_QUEUE_HEAD*
RtbQueueInit(
    );

/**
    Free a rtb queue.
    \param [IN]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
*/
EXTERN void
RtbQueueFree(
    RTB_QUEUE_HEAD* AicQueueHead
    );
/**
    Queue specified AicBuffer into a AicQueue at list tail.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            AicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
EXTERN void
RtbQueueTail(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 AicBuffer
    );

/**
    Queue specified AicBuffer into a AicQueue at list Head.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            AicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
EXTERN void
RtbQueueHead(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 AicBuffer
    );

/**
    Remove a AicBuffer from specified aicqueue at list tail.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    removed aicbuffer if succeed, otherwise NULL
*/
EXTERN AIC_BUFFER*
RtbDequeueTail(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
    );

/**
    Remove a AicBuffer from specified aicqueue at list head.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    removed aicbuffer if succeed, otherwise NULL
*/
EXTERN AIC_BUFFER*
RtbDequeueHead(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
    );

/**
    Get current rtb queue's length.
    \param [IN]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    current queue's length
*/
EXTERN signed long
RtbGetQueueLen(
    IN RTB_QUEUE_HEAD* AicQueueHead
    );

/**
    Empty the aicqueue.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
*/
EXTERN void
RtbEmptyQueue(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
    );

/**
    Get the AicBuffer which is the head of a AicQueue
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return head of the AicQueue , otherwise NULL
*/
EXTERN AIC_BUFFER*
RtbTopQueue(
    IN RTB_QUEUE_HEAD* AicQueueHead
);

/**
    Insert new Aicbuffer in the old buffer
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            OldAicBuffer                <AIC_BUFFER*>                 : old aic buffer
    \param [IN]            NewAicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
EXTERN void
RtbInsertBefore(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER* pOldAicBuffer,
    IN AIC_BUFFER* pNewAicBuffer
);

/**
    check whether the buffer is the last node in the queue
*/
EXTERN unsigned char
RtbNodeIsLast(
    IN RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 pAicBuffer
);

/**
    get the next buffer node after the specified buffer in the queue
    if the specified buffer is the last node in the queue , return NULL
    \param [IN]     AicBuffer        <AIC_BUFFER*>        : Aic Queue
    \param [IN]     AicBuffer        <AIC_BUFFER*>        : Aic buffer
    \return node after the specified buffer
*/
EXTERN AIC_BUFFER*
RtbQueueNextNode(
    IN RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 pAicBuffer
);

/**
    check whether queue is empty
*/
EXTERN bool
RtbQueueIsEmpty(
   IN RTB_QUEUE_HEAD* AicQueueHead
);

//annie_tmp
EXTERN unsigned char
RtbCheckQueueLen(
   IN RTB_QUEUE_HEAD* AicQueueHead,
   IN uint8_t Len
);

EXTERN void
RtbRemoveNode(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*         AicBuffer
);

EXTERN AIC_BUFFER*
    RtbCloneBuffer(
    IN AIC_BUFFER* pDataBuffer
    );

#endif /*BT_SKBUFF_H*/
