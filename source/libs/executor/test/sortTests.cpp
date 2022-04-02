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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <tglobal.h>
#include <tsort.h>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "os.h"

#include "executorimpl.h"
#include "executor.h"
#include "stub.h"
#include "taos.h"
#include "tdatablock.h"
#include "tdef.h"
#include "trpc.h"
#include "tvariant.h"
#include "tcompare.h"

namespace {
typedef struct {
  int32_t startVal;
  int32_t count;
  int32_t pageRows;
} _info;

SSDataBlock* getSingleColDummyBlock(void* param) {
  _info* pInfo = (_info*) param;
  if (--pInfo->count < 0) {
    return NULL;
  }

  SSDataBlock* pBlock = static_cast<SSDataBlock*>(taosMemoryCalloc(1, sizeof(SSDataBlock)));
  pBlock->pDataBlock = taosArrayInit(4, sizeof(SColumnInfoData));

  SColumnInfoData colInfo = {0};
  colInfo.info.type = TSDB_DATA_TYPE_INT;
  colInfo.info.bytes = sizeof(int32_t);
  colInfo.info.colId = 1;
  colInfo.pData = static_cast<char*>(taosMemoryCalloc(pInfo->pageRows, sizeof(int32_t)));
  colInfo.nullbitmap = static_cast<char*>(taosMemoryCalloc(1, (pInfo->pageRows + 7) / 8));

  taosArrayPush(pBlock->pDataBlock, &colInfo);

  for (int32_t i = 0; i < pInfo->pageRows; ++i) {
    SColumnInfoData* pColInfo = static_cast<SColumnInfoData*>(TARRAY_GET_ELEM(pBlock->pDataBlock, 0));

    int32_t v = ++pInfo->startVal;
    colDataAppend(pColInfo, i, reinterpret_cast<const char*>(&v), false);
  }

  pBlock->info.rows = pInfo->pageRows;
  pBlock->info.numOfCols = 1;
  return pBlock;
}

SSDataBlock* getSingleColStrBlock(void* param) {
  _info* pInfo = (_info*) param;
  if (--pInfo->count < 0) {
    return NULL;
  }

  SSDataBlock* pBlock = static_cast<SSDataBlock*>(taosMemoryCalloc(1, sizeof(SSDataBlock)));
  pBlock->pDataBlock = taosArrayInit(4, sizeof(SColumnInfoData));

  SColumnInfoData colInfo = {0};
  colInfo.info.type = TSDB_DATA_TYPE_NCHAR;
  colInfo.info.bytes = TSDB_NCHAR_SIZE * 16;
  colInfo.info.colId = 1;
  colInfo.varmeta.offset = static_cast<int32_t *>(taosMemoryCalloc(pInfo->pageRows, sizeof(int32_t)));

  taosArrayPush(pBlock->pDataBlock, &colInfo);

  for (int32_t i = 0; i < pInfo->pageRows; ++i) {
    SColumnInfoData* pColInfo = static_cast<SColumnInfoData*>(TARRAY_GET_ELEM(pBlock->pDataBlock, 0));

    int32_t size = taosRand() % 16;
    char str[64] = {0};
    taosRandStr(varDataVal(str), size);
    varDataSetLen(str, size);
    colDataAppend(pColInfo, i, reinterpret_cast<const char*>(str), false);
  }

  pBlock->info.rows = pInfo->pageRows;
  pBlock->info.numOfCols = 1;
  pBlock->info.hasVarCol = true;

  return pBlock;
}


int32_t docomp(const void* p1, const void* p2, void* param) {
  int32_t pLeftIdx  = *(int32_t *)p1;
  int32_t pRightIdx = *(int32_t *)p2;

  SMsortComparParam *pParam = (SMsortComparParam *)param;
  SGenericSource** px = reinterpret_cast<SGenericSource**>(pParam->pSources);

  SArray *pInfo = pParam->orderInfo;

  SGenericSource* pLeftSource  = px[pLeftIdx];
  SGenericSource* pRightSource = px[pRightIdx];

  // this input is exhausted, set the special value to denote this
  if (pLeftSource->src.rowIndex == -1) {
    return 1;
  }

  if (pRightSource->src.rowIndex == -1) {
    return -1;
  }

  SSDataBlock* pLeftBlock = pLeftSource->src.pBlock;
  SSDataBlock* pRightBlock = pRightSource->src.pBlock;

  for(int32_t i = 0; i < pInfo->size; ++i) {
    SBlockOrderInfo* pOrder = (SBlockOrderInfo*)TARRAY_GET_ELEM(pInfo, i);

    SColumnInfoData* pLeftColInfoData = (SColumnInfoData*)TARRAY_GET_ELEM(pLeftBlock->pDataBlock, pOrder->slotId);

    bool leftNull  = false;
    if (pLeftColInfoData->hasNull) {
      leftNull = colDataIsNull(pLeftColInfoData, pLeftBlock->info.rows, pLeftSource->src.rowIndex, pLeftBlock->pBlockAgg);
    }

    SColumnInfoData* pRightColInfoData = (SColumnInfoData*) TARRAY_GET_ELEM(pRightBlock->pDataBlock, pOrder->slotId);
    bool rightNull = false;
    if (pRightColInfoData->hasNull) {
      rightNull = colDataIsNull(pRightColInfoData, pRightBlock->info.rows, pRightSource->src.rowIndex, pRightBlock->pBlockAgg);
    }

    if (leftNull && rightNull) {
      continue; // continue to next slot
    }

    if (rightNull) {
      return pParam->nullFirst? 1:-1;
    }

    if (leftNull) {
      return pParam->nullFirst? -1:1;
    }

    void* left1  = colDataGetData(pLeftColInfoData, pLeftSource->src.rowIndex);
    void* right1 = colDataGetData(pRightColInfoData, pRightSource->src.rowIndex);
    __compar_fn_t fn = getKeyComparFunc(pLeftColInfoData->info.type, pOrder->order);

    int ret = fn(left1, right1);
    if (ret == 0) {
      continue;
    } else {
      return ret;
    }
  }

  return 0;
}
}  // namespace

#if 1
TEST(testCase, inMem_sort_Test) {
  SBlockOrderInfo oi = {0};
  oi.order = TSDB_ORDER_ASC;
  oi.slotId = 0;
  SArray* orderInfo = taosArrayInit(1, sizeof(SBlockOrderInfo));
  taosArrayPush(orderInfo, &oi);

  SSchema s = {.type = TSDB_DATA_TYPE_INT, .colId = 1, .bytes = 4, };
  SSortHandle* phandle = tsortCreateSortHandle(orderInfo, SORT_SINGLESOURCE_SORT, 1024, 5, NULL, "test_abc");
  tsortSetFetchRawDataFp(phandle, getSingleColDummyBlock);

  _info* pInfo = (_info*) taosMemoryCalloc(1, sizeof(_info));
  pInfo->startVal = 0;
  pInfo->pageRows = 100;
  pInfo->count = 6;

  SGenericSource* ps = static_cast<SGenericSource*>(taosMemoryCalloc(1, sizeof(SGenericSource)));
  ps->param = pInfo;
  tsortAddSource(phandle, ps);

  int32_t code = tsortOpen(phandle);
  int32_t row = 1;

  while(1) {
    STupleHandle* pTupleHandle = tsortNextTuple(phandle);
    if (pTupleHandle == NULL) {
      break;
    }

    void* v = tsortGetValue(pTupleHandle, 0);
    printf("%d: %d\n", row, *(int32_t*) v);
    ASSERT_EQ(row++, *(int32_t*) v);

  }
  tsortDestroySortHandle(phandle);
}

TEST(testCase, external_mem_sort_Test) {
  SBlockOrderInfo oi = {0};
  oi.order = TSDB_ORDER_ASC;
  oi.slotId = 0;
  SArray* orderInfo = taosArrayInit(1, sizeof(SBlockOrderInfo));
  taosArrayPush(orderInfo, &oi);

  SSortHandle* phandle = tsortCreateSortHandle(orderInfo, SORT_SINGLESOURCE_SORT, 128, 6, NULL, "test_abc");
  tsortSetFetchRawDataFp(phandle, getSingleColDummyBlock);

  _info* pInfo = (_info*) taosMemoryCalloc(1, sizeof(_info));
  pInfo->startVal = 0;
  pInfo->pageRows = 100;
  pInfo->count = 6;

  SGenericSource* ps = static_cast<SGenericSource*>(taosMemoryCalloc(1, sizeof(SGenericSource)));
  ps->param = pInfo;

  tsortAddSource(phandle, ps);

  int32_t code = tsortOpen(phandle);
  int32_t row = 1;

  while(1) {
    STupleHandle* pTupleHandle = tsortNextTuple(phandle);
    if (pTupleHandle == NULL) {
      break;
    }

    void* v = tsortGetValue(pTupleHandle, 0);
    printf("%d: %d\n", row, *(int32_t*) v);
    ASSERT_EQ(row++, *(int32_t*) v);
    //char        buf[64] = {0};
    //snprintf(buf, varDataLen(v), "%s", varDataVal(v));
    //printf("%d: %s\n", row, buf);
  }
  tsortDestroySortHandle(phandle);
}

TEST(testCase, ordered_merge_sort_Test) {
  SBlockOrderInfo oi = {0};
  oi.order = TSDB_ORDER_ASC;
  oi.slotId = 0;
  SArray* orderInfo = taosArrayInit(1, sizeof(SBlockOrderInfo));
  taosArrayPush(orderInfo, &oi);

  SSDataBlock* pBlock = static_cast<SSDataBlock*>(taosMemoryCalloc(1, sizeof(SSDataBlock)));
  pBlock->pDataBlock = taosArrayInit(1, sizeof(SColumnInfoData));
  pBlock->info.numOfCols = 1;
  for (int32_t i = 0; i < pBlock->info.numOfCols; ++i) {
    SColumnInfoData colInfo = {0};
    colInfo.info.type = TSDB_DATA_TYPE_INT;
    colInfo.info.bytes = sizeof(int32_t);
    colInfo.info.colId = 1;
    taosArrayPush(pBlock->pDataBlock, &colInfo);
  }

  SSortHandle* phandle = tsortCreateSortHandle(orderInfo, SORT_MULTISOURCE_MERGE, 1024, 5, pBlock,"test_abc");
  tsortSetFetchRawDataFp(phandle, getSingleColDummyBlock);
  tsortSetComparFp(phandle, docomp);

  for(int32_t i = 0; i < 10; ++i) {
    SGenericSource* p = static_cast<SGenericSource*>(taosMemoryCalloc(1, sizeof(SGenericSource)));
    _info* c = static_cast<_info*>(taosMemoryCalloc(1, sizeof(_info)));
    c->count    = 1;
    c->pageRows = 1000;
    c->startVal = i*1000;

    p->param = c;
    tsortAddSource(phandle, p);
  }

  int32_t code = tsortOpen(phandle);
  int32_t row = 1;

  while(1) {
    STupleHandle* pTupleHandle = tsortNextTuple(phandle);
    if (pTupleHandle == NULL) {
      break;
    }

    void* v = tsortGetValue(pTupleHandle, 0);
    printf("%d: %d\n", row, *(int32_t*) v);
    ASSERT_EQ(row++, *(int32_t*) v);

  }
  tsortDestroySortHandle(phandle);
  taosMemoryFree(pBlock);
}

#endif

#pragma GCC diagnostic pop
