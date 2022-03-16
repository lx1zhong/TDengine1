/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http:www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "mmMsg.h"
#include "dmInt.h"
#include "mmWorker.h"

int32_t mmProcessCreateReq(SMnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SDnode  *pDnode = pMgmt->pDnode;
  SRpcMsg *pReq = &pMsg->rpcMsg;

  SDCreateMnodeReq createReq = {0};
  if (tDeserializeSDCreateMnodeReq(pReq->pCont, pReq->contLen, &createReq) != 0) {
    terrno = TSDB_CODE_INVALID_MSG;
    return -1;
  }

  if (createReq.replica <= 1 || createReq.dnodeId != pDnode->dnodeId) {
    terrno = TSDB_CODE_DND_MNODE_INVALID_OPTION;
    dError("failed to create mnode since %s", terrstr());
    return -1;
  }

  SMnodeOpt option = {0};
  if (mmBuildOptionFromReq(pMgmt, &option, &createReq) != 0) {
    terrno = TSDB_CODE_DND_MNODE_INVALID_OPTION;
    dError("failed to create mnode since %s", terrstr());
    return -1;
  }

  SMnode *pMnode = mmAcquire(pMgmt);
  if (pMnode != NULL) {
    mmRelease(pMgmt, pMnode);
    terrno = TSDB_CODE_DND_MNODE_ALREADY_DEPLOYED;
    dError("failed to create mnode since %s", terrstr());
    return -1;
  }

  dDebug("start to create mnode");
  return mmOpen(pMgmt, &option);
}

int32_t mmProcessAlterReq(SMnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SDnode *pDnode = pMgmt->pDnode;
  SRpcMsg *pReq = &pMsg->rpcMsg;

  SDAlterMnodeReq alterReq = {0};
  if (tDeserializeSDCreateMnodeReq(pReq->pCont, pReq->contLen, &alterReq) != 0) {
    terrno = TSDB_CODE_INVALID_MSG;
    return -1;
  }

  if (alterReq.dnodeId != pDnode->dnodeId) {
    terrno = TSDB_CODE_DND_MNODE_INVALID_OPTION;
    dError("failed to alter mnode since %s", terrstr());
    return -1;
  }

  SMnodeOpt option = {0};
  if (mmBuildOptionFromReq(pMgmt, &option, &alterReq) != 0) {
    terrno = TSDB_CODE_DND_MNODE_INVALID_OPTION;
    dError("failed to alter mnode since %s", terrstr());
    return -1;
  }

  SMnode *pMnode = mmAcquire(pMgmt);
  if (pMnode == NULL) {
    terrno = TSDB_CODE_DND_MNODE_NOT_DEPLOYED;
    dError("failed to alter mnode since %s", terrstr());
    return -1;
  }

  dDebug("start to alter mnode");
  int32_t code = mmAlter(pMgmt, &option);
  mmRelease(pMgmt, pMnode);

  return code;
}

int32_t mmProcessDropReq(SMnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SDnode *pDnode = pMgmt->pDnode;
  SRpcMsg *pReq = &pMsg->rpcMsg;

  SDDropMnodeReq dropReq = {0};
  if (tDeserializeSMCreateDropMnodeReq(pReq->pCont, pReq->contLen, &dropReq) != 0) {
    terrno = TSDB_CODE_INVALID_MSG;
    return -1;
  }

  if (dropReq.dnodeId != pDnode->dnodeId) {
    terrno = TSDB_CODE_DND_MNODE_INVALID_OPTION;
    dError("failed to drop mnode since %s", terrstr());
    return -1;
  }

  SMnode *pMnode = mmAcquire(pMgmt);
  if (pMnode == NULL) {
    terrno = TSDB_CODE_DND_MNODE_NOT_DEPLOYED;
    dError("failed to drop mnode since %s", terrstr());
    return -1;
  }

  dDebug("start to drop mnode");
  int32_t code = mmDrop(pMgmt);
  mmRelease(pMgmt, pMnode);

  return code;
}

void mmInitMsgHandles(SMgmtWrapper *pWrapper) {
  // Requests handled by DNODE
  dndSetMsgHandle(pWrapper, TDMT_DND_CREATE_MNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_ALTER_MNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_DROP_MNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_CREATE_QNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_DROP_QNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_CREATE_SNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_DROP_SNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_CREATE_BNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_DROP_BNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_CREATE_VNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_ALTER_VNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_DROP_VNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_SYNC_VNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_COMPACT_VNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_DND_CONFIG_DNODE_RSP, (NodeMsgFp)mmProcessWriteMsg);

  // Requests handled by MNODE
  dndSetMsgHandle(pWrapper, TDMT_MND_CONNECT, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_ACCT, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_ALTER_ACCT, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_ACCT, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_USER, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_ALTER_USER, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_USER, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_GET_USER_AUTH, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_DNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CONFIG_DNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_DNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_MNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_MNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_QNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_QNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_SNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_SNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_BNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_BNODE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_USE_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_ALTER_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_SYNC_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_COMPACT_DB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_FUNC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_RETRIEVE_FUNC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_FUNC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_STB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_ALTER_STB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_STB, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_TABLE_META, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_VGROUP_LIST, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_KILL_QUERY, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_KILL_CONN, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_HEARTBEAT, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_SHOW, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_SHOW_RETRIEVE, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_STATUS, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_KILL_TRANS, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_GRANT, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_AUTH, (NodeMsgFp)mmProcessReadMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_CREATE_TOPIC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_ALTER_TOPIC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_DROP_TOPIC, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_SUBSCRIBE, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_MQ_COMMIT_OFFSET, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_MND_GET_SUB_EP, (NodeMsgFp)mmProcessReadMsg);

  // Requests handled by VNODE
  dndSetMsgHandle(pWrapper, TDMT_VND_MQ_SET_CONN_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_VND_MQ_REB_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_VND_CREATE_STB_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_VND_ALTER_STB_RSP, (NodeMsgFp)mmProcessWriteMsg);
  dndSetMsgHandle(pWrapper, TDMT_VND_DROP_STB_RSP, (NodeMsgFp)mmProcessWriteMsg);
}
