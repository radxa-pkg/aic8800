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
*	    bt_skbuff.c
*
*	Abstract:
*	    Data buffer managerment through whole bluetooth stack.
*
*	Notes:
*		  To reduce memory copy when pass data buffer to other layers,
*      	AIC_BUFFER is designed referring to linux socket buffer.
*       But I still wonder its effect, since AIC_BUFFER is much bigger
*       than original data buffer.AIC_BUFFER will reduce its member if
*       it would not reach what i had expected.
*
******************************************************************************/


#define LOG_TAG "bt_h5"
#undef NDEBUG
#include <utils/Log.h>
#include <stdlib.h>
#include <fcntl.h>

#include <termios.h>
#include <errno.h>
#include <pthread.h>


#include "bt_list.h"
#include "bt_skbuff.h"
#include "string.h"
#include "hci_h5_int.h"
#define IN
#define OUT

//****************************************************************************
// CONSTANT DEFINITION
//****************************************************************************
///default header size
///l2cap header(8)+hci acl(4)
#define DEFAULT_HEADER_SIZE    (8+4)

//AIC_BUFFER data buffer alignment
#define RTB_ALIGN   4

//do alignment with RTB_ALIGN
#define RTB_DATA_ALIGN(_Length)     ((_Length + (RTB_ALIGN - 1)) & (~(RTB_ALIGN - 1)))

//****************************************************************************
// STRUCTURE DEFINITION
//****************************************************************************
typedef struct _RTB_QUEUE_HEAD{
    RT_LIST_HEAD List;
    uint32_t  QueueLen;
    pthread_mutex_t Lock;
    uint8_t   Id[RTB_QUEUE_ID_LENGTH];
}*PRTB_QUEUE_HEAD;

//****************************************************************************
// FUNCTION
//****************************************************************************
/**
    check whether queue is empty
    \return :   FALSE   Queue is not empty
        TRU Queue is empty
*/
bool
RtbQueueIsEmpty(
   IN RTB_QUEUE_HEAD* AicQueueHead
)
{
    //return ListIsEmpty(&AicQueueHead->List);
    return  AicQueueHead->QueueLen > 0 ? false : true;
}

/**
    Allocate a AIC_BUFFER with specified data length and reserved headroom.
    If caller does not know actual headroom to reserve for further usage, specify it to zero to use default value.
    \param [IN]     Length            <uint32_t>        : current data buffer length to allcated
    \param [IN]     HeadRoom     <uint32_t>         : if caller knows reserved head space, set it; otherwise set 0 to use default value
    \return pointer to AIC_BUFFER if succeed, null otherwise
*/
AIC_BUFFER*
RtbAllocate(
    uint32_t Length,
    uint32_t HeadRoom
    )
{
    AIC_BUFFER* Rtb = NULL;
    ///Rtb buffer length:
    ///     AIC_BUFFER   48
    ///     HeadRoom      HeadRomm or 12
    ///     Length
    ///memory size: 48 + Length + 12(default) + 8*2(header for each memory) ---> a multiple of 8
    ///example:       (48 + 8)+ (300 + 12 + 8) = 372
    Rtb = malloc( sizeof(AIC_BUFFER) );
    if(Rtb)
    {
        uint32_t BufferLen = HeadRoom ? (Length + HeadRoom) : (Length + DEFAULT_HEADER_SIZE);
        BufferLen = RTB_DATA_ALIGN(BufferLen);
        Rtb->Head = malloc(BufferLen);
        if(Rtb->Head)
        {
            Rtb->HeadRoom = HeadRoom ? HeadRoom : DEFAULT_HEADER_SIZE;
            Rtb->Data = Rtb->Head + Rtb->HeadRoom;
            Rtb->End = Rtb->Data;
            Rtb->Tail = Rtb->End + Length;
            Rtb->Length = 0;
            ListInitializeHeader(&Rtb->List);
            Rtb->RefCount = 1;
            return Rtb;
        }
    }

    if (Rtb)
    {
        if (Rtb->Head)
        {
            free(Rtb->Head);
        }

        free(Rtb);
    }
    return NULL;
}


/**
    Free specified Aic_buffer
    \param [IN]     AicBuffer            <AIC_BUFFER*>        : buffer to free
*/
void
RtbFree(
    AIC_BUFFER* AicBuffer
)
{
    if(AicBuffer)
    {
        free(AicBuffer->Head);
        free(AicBuffer);
    }
    return;
}

/**
    Add a specified length protocal header to the start of data buffer hold by specified aic_buffer.
    This function extends used data area of the buffer at the buffer start.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to add
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the first byte of the extra data is returned
*/
uint8_t*
RtbAddHead(
    AIC_BUFFER* AicBuffer,
    uint32_t                 Length
    )
{

    if ((uint32_t)(AicBuffer->Data - AicBuffer->Head) >= Length)
    {
        AicBuffer->Data -= Length;
        AicBuffer->Length += Length;
        AicBuffer->HeadRoom -= Length;
        return AicBuffer->Data;
    }

    return NULL;
}
/**
    Remove a specified length data from the start of data buffer hold by specified aic_buffer.
    This function returns the memory to the headroom.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to remove
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the next data in the buffer is returned, usually useless
*/
unsigned char
RtbRemoveHead(
    AIC_BUFFER* AicBuffer,
    uint32_t                 Length
    )
{

    if (AicBuffer->Length >= Length)
    {
        AicBuffer->Data += Length;
        AicBuffer->Length -= Length;
        AicBuffer->HeadRoom += Length;
        return  true;
    }

    return false;
}

/**
    Add a specified length protocal header to the end of data buffer hold by specified aic_buffer.
    This function extends used data area of the buffer at the buffer end.
    \param [IN OUT]     AicBuffer            <AIC_BUFFER*>        : data buffer to add
    \param [IN]            Length                <uint32_t>                 : header length
    \return  Pointer to the first byte of the extra data is returned
*/
uint8_t*
RtbAddTail(
    AIC_BUFFER* AicBuffer,
    uint32_t                 Length
    )
{

    if ((uint32_t)(AicBuffer->Tail - AicBuffer->End) >= Length)
    {
        uint8_t* Tmp = AicBuffer->End;
        AicBuffer->End += Length;
        AicBuffer->Length += Length;
        return Tmp;
    }

    return NULL;
}

unsigned char
RtbRemoveTail(
    IN OUT AIC_BUFFER * AicBuffer,
    IN     uint32_t       Length
)
{

    if ((uint32_t)(AicBuffer->End - AicBuffer->Data) >= Length)
    {
        AicBuffer->End -= Length;
        AicBuffer->Length -= Length;
        return true;
    }

    return false;
}
//****************************************************************************
// RTB list manipulation
//****************************************************************************
/**
    Initialize a rtb queue.
    \return  Initilized rtb queue if succeed, otherwise NULL
*/
RTB_QUEUE_HEAD*
RtbQueueInit(
)
{
    RTB_QUEUE_HEAD* RtbQueue = NULL;
    int ret = 0;
    RtbQueue = malloc(sizeof(RTB_QUEUE_HEAD));
    if(RtbQueue)
    {
        ret = pthread_mutex_init(&RtbQueue->Lock, NULL);
        if(!ret) {
          ListInitializeHeader(&RtbQueue->List);
          RtbQueue->QueueLen = 0;
          return RtbQueue;
        }
    }

    //error code comes here
    if (RtbQueue)
    {
        free(RtbQueue);
    }
    return NULL;

}

/**
    Free a rtb queue.
    \param [IN]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
*/
void
RtbQueueFree(
    RTB_QUEUE_HEAD* AicQueueHead
    )
{
    if (AicQueueHead)
    {


        RtbEmptyQueue(AicQueueHead);
        pthread_mutex_destroy(&AicQueueHead->Lock);
        free(AicQueueHead);
    }
}

/**
    Queue specified AicBuffer into a AicQueue at list tail.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            AicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
void
RtbQueueTail(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 AicBuffer
    )
{
    pthread_mutex_lock(&AicQueueHead->Lock);
    ListAddToTail(&AicBuffer->List, &AicQueueHead->List);
    AicQueueHead->QueueLen++;
    pthread_mutex_unlock(&AicQueueHead->Lock);
}

/**
    Queue specified AicBuffer into a AicQueue at list Head.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            AicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
void
RtbQueueHead(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 AicBuffer
    )
{
    pthread_mutex_lock(&AicQueueHead->Lock);
    ListAddToHead(&AicBuffer->List, &AicQueueHead->List);
    AicQueueHead->QueueLen++;
    pthread_mutex_unlock(&AicQueueHead->Lock);
}


/**
    Insert new Aicbuffer in the old buffer
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            OldAicBuffer                <AIC_BUFFER*>                 : old aic buffer
    \param [IN]            NewAicBuffer                <AIC_BUFFER*>                 : Aic buffer to add
*/
void
RtbInsertBefore(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*  pOldAicBuffer,
    IN AIC_BUFFER*  pNewAicBuffer
)
{
    pthread_mutex_lock(&AicQueueHead->Lock);
    ListAdd(&pNewAicBuffer->List, pOldAicBuffer->List.Prev, &pOldAicBuffer->List);
    AicQueueHead->QueueLen++;
    pthread_mutex_unlock(&AicQueueHead->Lock);
}

/**
    check whether the buffer is the last node in the queue
*/
unsigned char
RtbNodeIsLast(
    IN RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 pAicBuffer
)
{
    AIC_BUFFER* pBuf;
    pthread_mutex_lock(&AicQueueHead->Lock);

    pBuf = (AIC_BUFFER*)AicQueueHead->List.Prev;
    if(pBuf == pAicBuffer)
    {
        pthread_mutex_unlock(&AicQueueHead->Lock);
        return true;
    }
    pthread_mutex_unlock(&AicQueueHead->Lock);
    return false;
}

/**
    get the next buffer node after the specified buffer in the queue
    if the specified buffer is the last node in the queue , return NULL
    \param [IN]     AicBuffer        <AIC_BUFFER*>        : Aic Queue
    \param [IN]     AicBuffer        <AIC_BUFFER*>        : Aic buffer
    \return node after the specified buffer
*/
AIC_BUFFER*
RtbQueueNextNode(
    IN RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 pAicBuffer
)
{
    AIC_BUFFER* pBuf;
    pthread_mutex_lock(&AicQueueHead->Lock);
    pBuf = (AIC_BUFFER*)AicQueueHead->List.Prev;
    if(pBuf == pAicBuffer)
    {
        pthread_mutex_unlock(&AicQueueHead->Lock);
        return NULL;    ///< if it is already the last node in the queue , return NULL
    }
    pBuf = (AIC_BUFFER*)pAicBuffer->List.Next;
    pthread_mutex_unlock(&AicQueueHead->Lock);
    return pBuf;    ///< return next node after this node
}

/**
    Delete specified AicBuffer from a AicQueue.
    It don't hold spinlock itself, so caller must hold it at someplace.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \param [IN]            AicBuffer                <AIC_BUFFER*>                 : Aic buffer to Remove
*/
void
RtbRemoveNode(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead,
    IN AIC_BUFFER*                 AicBuffer
)
{
    AicQueueHead->QueueLen--;
    ListDeleteNode(&AicBuffer->List);
}


/**
    Get the AicBuffer which is the head of a AicQueue
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return head of the AicQueue , otherwise NULL
*/
AIC_BUFFER*
RtbTopQueue(
    IN RTB_QUEUE_HEAD* AicQueueHead
)
{
    AIC_BUFFER* Rtb = NULL;
    pthread_mutex_lock(&AicQueueHead->Lock);

    if (RtbQueueIsEmpty(AicQueueHead))
    {
        pthread_mutex_unlock(&AicQueueHead->Lock);
        return NULL;
    }

    Rtb = (AIC_BUFFER*)AicQueueHead->List.Next;
    pthread_mutex_unlock(&AicQueueHead->Lock);

    return Rtb;
}

/**
    Remove a AicBuffer from specified aicqueue at list tail.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    removed aicbuffer if succeed, otherwise NULL
*/
AIC_BUFFER*
RtbDequeueTail(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
)
{
    AIC_BUFFER* Rtb = NULL;

    pthread_mutex_lock(&AicQueueHead->Lock);
    if (RtbQueueIsEmpty(AicQueueHead))
    {
         pthread_mutex_unlock(&AicQueueHead->Lock);
         return NULL;
    }
    Rtb = (AIC_BUFFER*)AicQueueHead->List.Prev;
    RtbRemoveNode(AicQueueHead, Rtb);
    pthread_mutex_unlock(&AicQueueHead->Lock);

    return Rtb;
}

/**
    Remove a AicBuffer from specified aicqueue at list head.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    removed aicbuffer if succeed, otherwise NULL
*/
AIC_BUFFER*
RtbDequeueHead(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
    )
{
    AIC_BUFFER* Rtb = NULL;
    pthread_mutex_lock(&AicQueueHead->Lock);

     if (RtbQueueIsEmpty(AicQueueHead))
     {
         pthread_mutex_unlock(&AicQueueHead->Lock);
         return NULL;
     }
    Rtb = (AIC_BUFFER*)AicQueueHead->List.Next;
    RtbRemoveNode(AicQueueHead, Rtb);
    pthread_mutex_unlock(&AicQueueHead->Lock);
    return Rtb;
}

/**
    Get current rtb queue's length.
    \param [IN]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
    \return    current queue's length
*/
signed long RtbGetQueueLen(
    IN RTB_QUEUE_HEAD* AicQueueHead
    )
{
    return AicQueueHead->QueueLen;
}

/**
    Empty the aicqueue.
    \param [IN OUT]     AicQueueHead        <RTB_QUEUE_HEAD*>        : Aic Queue
*/
void
RtbEmptyQueue(
    IN OUT RTB_QUEUE_HEAD* AicQueueHead
    )
{
    AIC_BUFFER* Rtb = NULL;
    pthread_mutex_lock(&AicQueueHead->Lock);

    while( !RtbQueueIsEmpty(AicQueueHead))
    {
        Rtb = (AIC_BUFFER*)AicQueueHead->List.Next;
        RtbRemoveNode(AicQueueHead, Rtb);
        RtbFree(Rtb);
    }

    pthread_mutex_unlock(&AicQueueHead->Lock);
    return;
}


///Annie_tmp
unsigned char
RtbCheckQueueLen(IN RTB_QUEUE_HEAD* AicQueueHead, IN uint8_t Len)
{
    return AicQueueHead->QueueLen < Len ? true : false;
}

/**
    clone buffer for upper or lower layer, because original buffer should be stored in l2cap
    \param <AIC_BUFFER* pDataBuffer: original buffer
    \return cloned buffer
*/
AIC_BUFFER*
RtbCloneBuffer(
    IN AIC_BUFFER* pDataBuffer
)
{
    AIC_BUFFER* pNewBuffer = NULL;
    if(pDataBuffer)
    {
        pNewBuffer = RtbAllocate(pDataBuffer->Length,0);
        if(!pNewBuffer)
        {
            return NULL;
        }
        if(pDataBuffer && pDataBuffer->Data)
            memcpy(pNewBuffer->Data, pDataBuffer->Data, pDataBuffer->Length);
        else
        {
            RtbFree(pNewBuffer);
            return NULL;
        }

        pNewBuffer->Length = pDataBuffer->Length;
    }
    return pNewBuffer;
}
