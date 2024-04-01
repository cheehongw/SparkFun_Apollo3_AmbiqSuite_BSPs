// ****************************************************************************
//
//  amdtpc_main.c
//! @file
//!
//! @brief Ambiq Micro AMDTP client.
//!
//! @{
//
// ****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2023, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision release_sdk_3_1_1-10cda4b5e0 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <string.h>
#include <stdbool.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "bstream.h"
#include "app_api.h"
#include "amdtpc_api.h"
#include "svc_amdtp.h"
#include "wsf_trace.h"

static void amdtpcHandleWriteResponse(attEvt_t *pMsg);

//*****************************************************************************
//
// Global variables
//
//*****************************************************************************

uint8_t rxPktBuf[AMDTP_PACKET_SIZE];
uint8_t txPktBuf[AMDTP_PACKET_SIZE];
uint8_t ackPktBuf[20];


/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* UUIDs */
static const uint8_t amdtpSvcUuid[] = {ATT_UUID_AMDTP_SERVICE};    /*! AMDTP service */
static const uint8_t amdtpRxChUuid[] = {ATT_UUID_AMDTP_RX};        /*! AMDTP Rx */
static const uint8_t amdtpTxChUuid[] = {ATT_UUID_AMDTP_TX};        /*! AMDTP Tx */
static const uint8_t amdtpAckChUuid[] = {ATT_UUID_AMDTP_ACK};      /*! AMDTP Ack */

/* Characteristics for discovery */

/*! Proprietary data */
static const attcDiscChar_t amdtpRx =
{
  amdtpRxChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

static const attcDiscChar_t amdtpTx =
{
  amdtpTxChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! AMDTP Tx CCC descriptor */
static const attcDiscChar_t amdtpTxCcc =
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

static const attcDiscChar_t amdtpAck =
{
  amdtpAckChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! AMDTP Tx CCC descriptor */
static const attcDiscChar_t amdtpAckCcc =
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *amdtpDiscCharList[] =
{
  &amdtpRx,                  /*! Rx */
  &amdtpTx,                  /*! Tx */
  &amdtpTxCcc,               /*! Tx CCC descriptor */
  &amdtpAck,                 /*! Ack */
  &amdtpAckCcc               /*! Ack CCC descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
//WSF_ASSERT(AMDTP_HDL_LIST_LEN == ((sizeof(amdtpDiscCharList) / sizeof(attcDiscChar_t *))));


/* Control block */
static struct
{
    bool_t                  txReady;                // TRUE if ready to send notifications
    uint16_t                attRxHdl;
    uint16_t                attAckHdl;
    uint16_t                attTxHdl;
    amdtpCb_t               core;
}
amdtpcCb[DM_CONN_MAX];

/*************************************************************************************************/
/*!
 *  \fn     AmdtpDiscover
 *
 *  \brief  Perform service and characteristic discovery for AMDTP service.
 *          Parameter pHdlList must point to an array of length AMDTP_HDL_LIST_LEN.
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void
AmdtpcDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_128_UUID_LEN, (uint8_t *) amdtpSvcUuid,
                     AMDTP_HDL_LIST_LEN, (attcDiscChar_t **) amdtpDiscCharList, pHdlList);
}

//*****************************************************************************
//
// Send data to server specified in amdtpcCb[connId]
//
//*****************************************************************************

dmConnId_t isConnectionOpen(dmConnId_t connId)
{
    dmConnId_t connIdList[DM_CONN_MAX];

    uint8_t numConn = AppConnOpenList(connIdList);
    for (uint8_t i = 0; i < numConn; i++) {
        if (connId == connIdList[i]) {
            return connId;
        }
    }

    return DM_CONN_ID_NONE;
}



static void
amdtpcSendData(uint8_t *buf, uint16_t len, dmConnId_t connId)
{
    if (isConnectionOpen(connId) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("AmdtpcSendData() no connection\n");
        return;
    }
    if (amdtpcCb[connId - 1].attRxHdl != ATT_HANDLE_NONE)
    {
        APP_TRACE_INFO3("AmdtpcSendData(), connId = %d, attRxHdl = %x, len = %d", connId, amdtpcCb[connId - 1].attRxHdl, len);
        AttcWriteCmd(connId, amdtpcCb[connId - 1].attRxHdl, len, buf);
        amdtpcCb[connId - 1].txReady = false;
    }
    else
    {
        APP_TRACE_WARN1("Invalid attRxHdl = 0x%x\n", amdtpcCb[connId - 1].attRxHdl);
    }
}

// Send ack to server specified in amdtpcCb.connId
static eAmdtpStatus_t
amdtpcSendAck(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len, dmConnId_t connId)
{
    AmdtpBuildPkt(&amdtpcCb[connId - 1].core, type, encrypted, enableACK, buf, len);

    if (isConnectionOpen(connId) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("AmdtpcSendAck() no connection\n");
        return AMDTP_STATUS_TX_NOT_READY;
    }

    if (amdtpcCb[connId - 1].attAckHdl != ATT_HANDLE_NONE)
    {
        APP_TRACE_INFO2("rxHdl = 0x%x, ackHdl = 0x%x\n", amdtpcCb[connId - 1].attRxHdl, amdtpcCb[connId - 1].attAckHdl);
        AttcWriteCmd(connId, amdtpcCb[connId - 1].attAckHdl, amdtpcCb[connId - 1].core.ackPkt.len, amdtpcCb[connId - 1].core.ackPkt.data);
    }
    else
    {
        APP_TRACE_INFO1("Invalid attAckHdl = 0x%x\n", amdtpcCb[connId - 1].attAckHdl);
        return AMDTP_STATUS_TX_NOT_READY;
    }
    return AMDTP_STATUS_SUCCESS;
}

void amdtpc_init_single(amdtpCb_t *core, wsfHandlerId_t handlerId, amdtpRecvCback_t recvCback, amdtpTransCback_t transCback)
{
    APP_TRACE_INFO1("amdtpc_init_single(), core address = %x\n", core);
    core->txState = AMDTP_STATE_TX_IDLE;
    core->rxState = AMDTP_STATE_INIT;
    core->timeoutTimer.handlerId = handlerId;

    core->lastRxPktSn = 0;
    core->txPktSn = 0;

    resetPkt(&(core->rxPkt));
    core->rxPkt.data = rxPktBuf;

    resetPkt(&(core->txPkt));
    core->txPkt.data = txPktBuf;

    resetPkt(&(core->ackPkt));
    core->ackPkt.data = ackPktBuf;

    core->recvCback = recvCback;
    core->transCback = transCback;

    core->txTimeoutMs = TX_TIMEOUT_DEFAULT;

    core->data_sender_func = amdtpcSendData;
    core->ack_sender_func = amdtpcSendAck;
}

void
amdtpc_init(wsfHandlerId_t handlerId, amdtpRecvCback_t recvCback, amdtpTransCback_t transCback)
{
    memset(&amdtpcCb, 0, sizeof(amdtpcCb));
    for (int i = 0; i < DM_CONN_MAX; i++)
    {
        APP_TRACE_INFO1("amdtpc_init(), core address = %x", &amdtpcCb[i].core);
        amdtpcCb[i].txReady = false;
        amdtpc_init_single(&amdtpcCb[i].core, handlerId, recvCback, transCback);
    }

}

static void
amdtpc_conn_close(dmEvt_t *pMsg)
{   

    dmConnId_t connId = pMsg->hdr.param;
    /* clear connection */
    WsfTimerStop(&amdtpcCb[connId - 1].core.timeoutTimer);

    amdtpcCb[connId - 1].txReady = false;
    amdtpcCb[connId - 1].core.txState = AMDTP_STATE_TX_IDLE;
    amdtpcCb[connId - 1].core.rxState = AMDTP_STATE_INIT;
    amdtpcCb[connId - 1].core.lastRxPktSn = 0;
    amdtpcCb[connId - 1].core.txPktSn = 0;
    resetPkt(&amdtpcCb[connId - 1].core.rxPkt);
    resetPkt(&amdtpcCb[connId - 1].core.txPkt);
    resetPkt(&amdtpcCb[connId - 1].core.ackPkt);
}

void
amdtpc_start(dmConnId_t connId, uint16_t rxHdl, uint16_t ackHdl, uint16_t txHdl, uint8_t timerEvt)
{
    amdtpcCb[connId - 1].txReady = true;
    amdtpcCb[connId - 1].attRxHdl = rxHdl;
    amdtpcCb[connId - 1].attAckHdl = ackHdl;
    amdtpcCb[connId - 1].attTxHdl = txHdl;
    amdtpcCb[connId - 1].core.timeoutTimer.msg.event = timerEvt;
    amdtpcCb[connId - 1].core.timeoutTimer.msg.param = connId;
    amdtpcCb[connId - 1].core.connId = connId;

    if (isConnectionOpen(connId) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("amdtpc_start() no connection\n");
        return;
    }

    amdtpcCb[connId - 1].core.attMtuSize = AttGetMtu(connId);
    APP_TRACE_INFO1("MTU size = %d bytes", amdtpcCb[connId - 1].core.attMtuSize);
}

//*****************************************************************************
//
// Timer Expiration handler
//
//*****************************************************************************
void
amdtpc_timeout_timer_expired(wsfMsgHdr_t *pMsg)
{
    uint8_t data[1];
    dmConnId_t connId = pMsg->param;
    data[0] = amdtpcCb[connId - 1].core.txPktSn;
    APP_TRACE_INFO1("amdtpc tx timeout, txPktSn = %d", amdtpcCb[connId - 1].core.txPktSn);
    AmdtpSendControl(&amdtpcCb[connId - 1].core, AMDTP_CONTROL_RESEND_REQ, data, 1);
    // fire a timer for receiving an AMDTP_STATUS_RESEND_REPLY ACK
    WsfTimerStartMs(&amdtpcCb[connId - 1].core.timeoutTimer, amdtpcCb[connId - 1].core.txTimeoutMs);
}

extern bool g_requestServerSendStop ;
/*************************************************************************************************/
/*!
 *  \fn     amdtpcValueNtf
 *
 *  \brief  Process a received ATT notification.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static uint8_t
amdtpcValueNtf(attEvt_t *pMsg)
{
    eAmdtpStatus_t status = AMDTP_STATUS_UNKNOWN_ERROR;
    amdtpPacket_t *pkt = NULL;
    dmConnId_t connId = pMsg->hdr.param;
#if 0
    APP_TRACE_INFO0("receive ntf data\n");
    APP_TRACE_INFO1("handle = 0x%x\n", pMsg->handle);
    for (int i = 0; i < pMsg->valueLen; i++)
    {
        APP_TRACE_INFO1("%02x ", pMsg->pValue[i]);
    }
    APP_TRACE_INFO0("\n");
#endif

    if (pMsg->handle == amdtpcCb[connId - 1].attRxHdl)
    {
        status = AmdtpReceivePkt(&amdtpcCb[connId - 1].core, &amdtpcCb[connId - 1].core.rxPkt, pMsg->valueLen, pMsg->pValue);
    }
    else if ( pMsg->handle == amdtpcCb[connId - 1].attAckHdl )
    {
        status = AmdtpReceivePkt(&amdtpcCb[connId - 1].core, &amdtpcCb[connId - 1].core.ackPkt, pMsg->valueLen, pMsg->pValue);
    }
    else if ( pMsg->handle == amdtpcCb[connId - 1].attTxHdl )
    {
        if ( g_requestServerSendStop ) //double check this
        {
            // if issuing "Request Server to send command" while receiving notification data, ignore the notification data
            amdtpcCb[connId - 1].core.txPkt.header.pktType = AMDTP_PKT_TYPE_DATA;
            status = AMDTP_STATUS_RECEIVE_DONE;
        }
        else
        {
            status = AmdtpReceivePkt(&amdtpcCb[connId - 1].core, &amdtpcCb[connId - 1].core.txPkt, pMsg->valueLen, pMsg->pValue);
        }
    }

    if (status == AMDTP_STATUS_RECEIVE_DONE)
    {
        if (pMsg->handle == amdtpcCb[connId - 1].attRxHdl)
        {
            pkt = &amdtpcCb[connId - 1].core.rxPkt;
        }
        else if (pMsg->handle == amdtpcCb[connId - 1].attAckHdl)
        {
            pkt = &amdtpcCb[connId - 1].core.ackPkt;
        }
        else if ( pMsg->handle == amdtpcCb[connId - 1].attTxHdl )
        {
            pkt = &amdtpcCb[connId - 1].core.txPkt;
        }

        AmdtpPacketHandler(&amdtpcCb[connId - 1].core, (eAmdtpPktType_t)pkt->header.pktType, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
    }

    return ATT_SUCCESS;
}

static void
amdtpcHandleWriteResponse(attEvt_t *pMsg)
{

    dmConnId_t connId = pMsg->hdr.param;

    //APP_TRACE_INFO2("amdtpcHandleWriteResponse, status = %d, hdl = 0x%x\n", pMsg->hdr.status, pMsg->handle);
    if (pMsg->hdr.status == ATT_SUCCESS && pMsg->handle == amdtpcCb[connId - 1].attRxHdl)
    {
        amdtpcCb[connId - 1].txReady = true;
        // process next data
        AmdtpSendPacketHandler(&amdtpcCb[connId - 1].core);
    }
}

void
amdtpc_proc_msg(wsfMsgHdr_t *pMsg)
{
    if (pMsg->event == DM_CONN_OPEN_IND)
    {
    }
    else if (pMsg->event == DM_CONN_CLOSE_IND)
    {
        amdtpc_conn_close((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_UPDATE_IND)
    {
    }
    else if (pMsg->event == amdtpcCb[pMsg->param - 1].core.timeoutTimer.msg.event)
    {
       amdtpc_timeout_timer_expired(pMsg);
    }
    else if (pMsg->event == ATTC_WRITE_CMD_RSP)
    {
        amdtpcHandleWriteResponse((attEvt_t *) pMsg);
    }
    else if (pMsg->event == ATTC_HANDLE_VALUE_NTF)
    {
        amdtpcValueNtf((attEvt_t *) pMsg);
    }
}

//*****************************************************************************
//
//! @brief Send data to Server via write command
//!
//! @param type - packet type
//! @param encrypted - is packet encrypted
//! @param enableACK - does client need to response
//! @param buf - data
//! @param len - data length
//!
//! @return status
//
//*****************************************************************************
eAmdtpStatus_t
AmdtpcSendPacket(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len, dmConnId_t connId)
{
    //
    // Check if the service is idle to send
    //
    if ( amdtpcCb[connId - 1].core.txState != AMDTP_STATE_TX_IDLE )
    {
        APP_TRACE_INFO1("data sending failed, tx state = %d", amdtpcCb[connId - 1].core.txState);
        return AMDTP_STATUS_BUSY;
    }

    //
    // Check if data length is valid
    //
    if ( len > AMDTP_MAX_PAYLOAD_SIZE )
    {
        APP_TRACE_INFO1("data sending failed, exceed maximum payload, len = %d.", len);
        return AMDTP_STATUS_INVALID_PKT_LENGTH;
    }

    //
    // Check if ready to send notification
    //
    if ( !amdtpcCb[connId - 1].txReady )
    {
        //set in callback amdtpsHandleValueCnf
        APP_TRACE_INFO1("data sending failed, not ready for notification.", NULL);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    AmdtpBuildPkt(&amdtpcCb[connId - 1].core, type, encrypted, enableACK, buf, len);
    // send packet
    AmdtpSendPacketHandler(&amdtpcCb[connId - 1].core);

    return AMDTP_STATUS_SUCCESS;
}
