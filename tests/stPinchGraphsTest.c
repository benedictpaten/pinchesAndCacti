/*
 * stPinchGraphsTest.c
 *
 *  Created on: 11 Apr 2012
 *      Author: benedictpaten
 *
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "CuTest.h"
#include "sonLib.h"
#include "stPinchGraphs.h"

static stPinchThreadSet *threadSet = NULL;
static int64_t name1 = 0, start1 = 1, length1 = INT64_MAX - 1;
static int64_t leftSplitPoint1 = 10, leftSplitPoint2 = 11;
static stPinchThread *thread1;
static int64_t name2 = INT64_MAX, start2 = 4, length2 = 10;
static stPinchThread *thread2;

static void teardown() {
    if (threadSet != NULL) {
        stPinchThreadSet_destruct(threadSet);
        threadSet = NULL;
    }
}

static void setup() {
    teardown();
    threadSet = stPinchThreadSet_construct();
    thread1 = stPinchThreadSet_addThread(threadSet, name1, start1, length1);
    thread2 = stPinchThreadSet_addThread(threadSet, name2, start2, length2);
    stPinchThread_split(thread1, leftSplitPoint1);
    stPinchThread_split(thread1, leftSplitPoint2);
}

static void testStPinchThreadSet(CuTest *testCase) {
    setup();
    CuAssertIntEquals(testCase, 2, stPinchThreadSet_getSize(threadSet));
    CuAssertPtrEquals(testCase, thread1, stPinchThreadSet_getThread(threadSet, name1));
    CuAssertPtrEquals(testCase, thread2, stPinchThreadSet_getThread(threadSet, name2));
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSet_getThread(threadSet, 5));
    stPinchThreadSetIt threadIt = stPinchThreadSet_getIt(threadSet);
    CuAssertPtrEquals(testCase, thread1, stPinchThreadSetIt_getNext(&threadIt));
    CuAssertPtrEquals(testCase, thread2, stPinchThreadSetIt_getNext(&threadIt));
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetIt_getNext(&threadIt));
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetIt_getNext(&threadIt));

    //Test segment iterators
    stSortedSet *segmentSet = stSortedSet_construct();
    stPinchThreadSetSegmentIt segmentIt = stPinchThreadSet_getSegmentIt(threadSet);
    stPinchSegment *segment;
    while ((segment = stPinchThreadSetSegmentIt_getNext(&segmentIt)) != NULL) {
        CuAssertTrue(testCase, stSortedSet_search(segmentSet, segment) == NULL);
        stSortedSet_insert(segmentSet, segment);
    }
    int64_t segmentCount = 0;
    stPinchThread *threads[] = { thread1, thread2 };
    for (int64_t i = 0; i < 2; i++) {
        stPinchThread *thread = threads[i];
        stPinchSegment *segment = stPinchThread_getFirst(thread);
        while (segment != NULL) {
            segmentCount++;
            CuAssertTrue(testCase, stSortedSet_search(segmentSet, segment) != NULL);
            segment = stPinchSegment_get3Prime(segment);
        }
    }
    CuAssertIntEquals(testCase, segmentCount, stSortedSet_size(segmentSet));
    stSortedSet_destruct(segmentSet);

    //Now test block iterator
    stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetBlockIt_getNext(&blockIt));
    stPinchBlock *block1 = stPinchBlock_construct2(stPinchThread_getFirst(thread1));
    stPinchBlock *block2 = stPinchBlock_construct2(stPinchThread_getFirst(thread2));
    blockIt = stPinchThreadSet_getBlockIt(threadSet);
    CuAssertPtrEquals(testCase, block1, stPinchThreadSetBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, block2, stPinchThreadSetBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetBlockIt_getNext(&blockIt));

    teardown();
}

static void testStPinchThreadAndSegment(CuTest *testCase) {
    setup();
    CuAssertIntEquals(testCase, name1, stPinchThread_getName(thread1));
    CuAssertIntEquals(testCase, start1, stPinchThread_getStart(thread1));
    CuAssertIntEquals(testCase, length1, stPinchThread_getLength(thread1));
    CuAssertIntEquals(testCase, name2, stPinchThread_getName(thread2));
    CuAssertIntEquals(testCase, start2, stPinchThread_getStart(thread2));
    CuAssertIntEquals(testCase, length2, stPinchThread_getLength(thread2));

    //Test the first thread.
    CuAssertPtrEquals(testCase, NULL, stPinchThread_getSegment(thread2, start2 - 1));
    CuAssertPtrEquals(testCase, stPinchThread_getFirst(thread2), stPinchThread_getSegment(thread2, start2));
    stPinchSegment *segment = stPinchThread_getFirst(thread2);
    CuAssertTrue(testCase, segment != NULL);
    CuAssertIntEquals(testCase, start2, stPinchSegment_getStart(segment));
    CuAssertIntEquals(testCase, length2, stPinchSegment_getLength(segment));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment));

    //Test the second thread.
    CuAssertPtrEquals(testCase, NULL, stPinchThread_getSegment(thread1, start1 - 1));
    CuAssertPtrEquals(testCase, stPinchThread_getFirst(thread1), stPinchThread_getSegment(thread1, start1));
    segment = stPinchThread_getFirst(thread1);
    CuAssertTrue(testCase, segment != NULL);
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get5Prime(segment));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment));
    CuAssertIntEquals(testCase, start1, stPinchSegment_getStart(segment));
    CuAssertIntEquals(testCase, leftSplitPoint1 - start1 + 1, stPinchSegment_getLength(segment));

    stPinchSegment *segment2 = stPinchSegment_get3Prime(segment);
    CuAssertTrue(testCase, segment2 != NULL);
    CuAssertPtrEquals(testCase, segment, stPinchSegment_get5Prime(segment2));
    CuAssertIntEquals(testCase, leftSplitPoint1 + 1, stPinchSegment_getStart(segment2));
    CuAssertIntEquals(testCase, leftSplitPoint2 - leftSplitPoint1, stPinchSegment_getLength(segment2));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment));

    stPinchSegment *segment3 = stPinchSegment_get3Prime(segment2);
    CuAssertTrue(testCase, segment3 != NULL);
    CuAssertPtrEquals(testCase, segment2, stPinchSegment_get5Prime(segment3));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get3Prime(segment3));
    CuAssertIntEquals(testCase, leftSplitPoint2 + 1, stPinchSegment_getStart(segment3));
    CuAssertIntEquals(testCase, length1 + start1 - leftSplitPoint2 - 1, stPinchSegment_getLength(segment3));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment));

    //Test stPinchThread_getSegment
    CuAssertPtrEquals(testCase, segment, stPinchThread_getSegment(thread1, start1));
    CuAssertPtrEquals(testCase, segment, stPinchThread_getSegment(thread1, leftSplitPoint1));
    CuAssertPtrEquals(testCase, segment2, stPinchThread_getSegment(thread1, leftSplitPoint1 + 1));
    CuAssertPtrEquals(testCase, segment2, stPinchThread_getSegment(thread1, leftSplitPoint2));
    CuAssertPtrEquals(testCase, segment3, stPinchThread_getSegment(thread1, leftSplitPoint2 + 1));
    CuAssertPtrEquals(testCase, segment3, stPinchThread_getSegment(thread1, start1 + length1 - 1));
    CuAssertPtrEquals(testCase, NULL, stPinchThread_getSegment(thread1, start1 + length1));

    //Test stPinchThread_joinTrivialBoundaries
    stPinchThread_joinTrivialBoundaries(thread1);
    //Now should be just one segment
    segment = stPinchThread_getFirst(thread1);
    CuAssertTrue(testCase, segment != NULL);
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get5Prime(segment));
    CuAssertIntEquals(testCase, start1, stPinchSegment_getStart(segment));
    CuAssertIntEquals(testCase, length1, stPinchSegment_getLength(segment));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get3Prime(segment));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment));
    CuAssertPtrEquals(testCase, segment, stPinchThread_getSegment(thread1, start1));
    CuAssertPtrEquals(testCase, segment, stPinchThread_getSegment(thread1, start1 + length1 - 1));
    CuAssertPtrEquals(testCase, NULL, stPinchThread_getSegment(thread1, start1 - 1));
    CuAssertPtrEquals(testCase, NULL, stPinchThread_getSegment(thread1, start1 + length1));

    //Test stPinchThreadSet_getTotalBlockNumber
    CuAssertIntEquals(testCase, 0, stPinchThreadSet_getTotalBlockNumber(threadSet));

    teardown();
}

static void testStPinchBlock_NoSplits(CuTest *testCase) {
    setup();
    static int64_t name3 = 5, start3 = 0, length3 = 20;
    stPinchThread *thread3 = stPinchThreadSet_addThread(threadSet, name3, start3, length3);
    stPinchThread_split(thread3, 4);
    stPinchThread_split(thread3, 9);
    stPinchThread_split(thread3, 14);
    stPinchSegment *segment1 = stPinchThread_getFirst(thread3);
    stPinchSegment *segment2 = stPinchSegment_get3Prime(segment1);
    stPinchSegment *segment3 = stPinchSegment_get3Prime(segment2);
    stPinchSegment *segment4 = stPinchSegment_get3Prime(segment3);
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment2));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment3));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment4));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get3Prime(segment4));
    stPinchBlock *block = stPinchBlock_construct(segment1, 1, segment2, 0);
    CuAssertIntEquals(testCase, 2, stPinchBlock_getDegree(block));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchBlock_getLength(block));
    //get block
    CuAssertPtrEquals(testCase, segment1, stPinchBlock_getFirst(block));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment2));
    //get block orientation
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment1));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment2));
    //test iterator
    stPinchBlockIt blockIt = stPinchBlock_getSegmentIterator(block);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment2, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));

    //Now try pinching in a segment
    stPinchBlock_pinch2(block, segment3, 1);
    CuAssertPtrEquals(testCase, segment1, stPinchBlock_getFirst(block));
    CuAssertIntEquals(testCase, 3, stPinchBlock_getDegree(block));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchBlock_getLength(block));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment3));
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment3));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment2));
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment1));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment2));
    blockIt = stPinchBlock_getSegmentIterator(block);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment2, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment3, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));

    //Now try removing from the block
    stPinchBlock_destruct(stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment2));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment3));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment1));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment2));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment3));

    //Now try merging two blocks and undoing them
    block = stPinchBlock_pinch(
            stPinchBlock_pinch(stPinchBlock_construct2(segment1), stPinchBlock_construct2(segment2), 0),
            stPinchBlock_construct(segment3, 0, segment4, 1), 0);
    CuAssertIntEquals(testCase, 4, stPinchBlock_getDegree(block));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchBlock_getLength(block));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment1));
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment1));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment2));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment2));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment3));
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment3));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment4));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment4));

    CuAssertPtrEquals(testCase, segment1, stPinchBlock_getFirst(block));
    blockIt = stPinchBlock_getSegmentIterator(block);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment2, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment3, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment4, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));

    //Test block destruct
    stPinchBlock_destruct(block);
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment1));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment1));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment2));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment2));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment3));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment3));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment4));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment4));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), 5);
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment2));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment3));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchSegment_getLength(segment4));

    stPinchThread_split(thread3, 0);
    segment1 = stPinchThread_getFirst(thread3);
    //Now make a block with a single element
    block = stPinchBlock_construct2(segment1);
    CuAssertIntEquals(testCase, 1, stPinchBlock_getDegree(block));
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchBlock_getLength(block));
    //get block
    CuAssertPtrEquals(testCase, segment1, stPinchBlock_getFirst(block));
    CuAssertPtrEquals(testCase, block, stPinchSegment_getBlock(segment1));
    //get block orientation
    CuAssertIntEquals(testCase, 1, stPinchSegment_getBlockOrientation(segment1));
    //test iterator
    blockIt = stPinchBlock_getSegmentIterator(block);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));
    stPinchBlock_destruct(block);
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment1));
    CuAssertIntEquals(testCase, 0, stPinchSegment_getBlockOrientation(segment1));

    //Test stPinchThreadSet_getTotalBlockNumber
    CuAssertIntEquals(testCase, 0, stPinchThreadSet_getTotalBlockNumber(threadSet));

    teardown();

}

static void testStPinchBlock_Splits(CuTest *testCase) {
    /*
     * Tests splitting of segments that are aligned. Put in seperate function as draws together previous code.
     */
    setup();
    static int64_t name3 = 5, start3 = 0, length3 = 15;
    stPinchThread *thread3 = stPinchThreadSet_addThread(threadSet, name3, start3, length3);
    stPinchThread_split(thread3, 4);
    stPinchThread_split(thread3, 9);
    stPinchSegment *segment1 = stPinchThread_getFirst(thread3);
    stPinchSegment *segment2 = stPinchSegment_get3Prime(segment1);
    stPinchSegment *segment3 = stPinchSegment_get3Prime(segment2);
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get3Prime(segment3));
    stPinchBlock_pinch2(stPinchBlock_construct(segment1, 1, segment2, 1), segment3, 1);

    stPinchThread_split(thread3, 0);

    //Now traverse through thread and check all is okay
    segment1 = stPinchThread_getFirst(thread3);
    CuAssertTrue(testCase, segment1 != NULL);
    CuAssertIntEquals(testCase, 1, stPinchSegment_getLength(segment1));
    segment2 = stPinchSegment_get3Prime(segment1);
    CuAssertTrue(testCase, segment2 != NULL);
    CuAssertIntEquals(testCase, 4, stPinchSegment_getLength(segment2));
    segment3 = stPinchSegment_get3Prime(segment2);
    CuAssertTrue(testCase, segment3 != NULL);
    CuAssertIntEquals(testCase, 1, stPinchSegment_getLength(segment3));
    stPinchSegment *segment4 = stPinchSegment_get3Prime(segment3);
    CuAssertTrue(testCase, segment4 != NULL);
    CuAssertIntEquals(testCase, 4, stPinchSegment_getLength(segment4));
    stPinchSegment *segment5 = stPinchSegment_get3Prime(segment4);
    CuAssertTrue(testCase, segment5 != NULL);
    CuAssertIntEquals(testCase, 1, stPinchSegment_getLength(segment5));
    stPinchSegment *segment6 = stPinchSegment_get3Prime(segment5);
    CuAssertTrue(testCase, segment6 != NULL);
    CuAssertIntEquals(testCase, 4, stPinchSegment_getLength(segment6));
    CuAssertPtrEquals(testCase, NULL, stPinchSegment_get3Prime(segment6));
    //Check the thread.
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment1));
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment2));
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment3));
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment4));
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment5));
    CuAssertPtrEquals(testCase, thread3, stPinchSegment_getThread(segment6));
    //Check the name
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment1));
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment2));
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment3));
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment4));
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment5));
    CuAssertIntEquals(testCase, name3, stPinchSegment_getName(segment6));

    //Now check blocks
    stPinchBlock *block1 = stPinchSegment_getBlock(segment1);
    CuAssertTrue(testCase, block1 != NULL);
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment1), stPinchBlock_getLength(block1));
    CuAssertIntEquals(testCase, 3, stPinchBlock_getDegree(block1));
    stPinchBlock *block2 = stPinchSegment_getBlock(segment2);
    CuAssertTrue(testCase, block2 != NULL);
    CuAssertIntEquals(testCase, stPinchSegment_getLength(segment2), stPinchBlock_getLength(block2));
    CuAssertIntEquals(testCase, 3, stPinchBlock_getDegree(block2));

    stPinchBlockIt blockIt = stPinchBlock_getSegmentIterator(block1);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment3, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment5, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment3));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment5));

    blockIt = stPinchBlock_getSegmentIterator(block2);
    CuAssertPtrEquals(testCase, segment2, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment4, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment6, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, block2, stPinchSegment_getBlock(segment2));
    CuAssertPtrEquals(testCase, block2, stPinchSegment_getBlock(segment4));
    CuAssertPtrEquals(testCase, block2, stPinchSegment_getBlock(segment6));

    //Test block iterator
    stPinchThreadSetBlockIt threadBlockIt = stPinchThreadSet_getBlockIt(threadSet);
    CuAssertTrue(testCase, stPinchThreadSetBlockIt_getNext(&threadBlockIt) != NULL);
    CuAssertTrue(testCase, stPinchThreadSetBlockIt_getNext(&threadBlockIt) != NULL);
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetBlockIt_getNext(&threadBlockIt));

    //Test stPinchThreadSet_getTotalBlockNumber
    CuAssertIntEquals(testCase, 2, stPinchThreadSet_getTotalBlockNumber(threadSet));

    //Now do merge
    stPinchThreadSet_joinTrivialBoundaries(threadSet);
    threadBlockIt = stPinchThreadSet_getBlockIt(threadSet);
    block1 = stPinchThreadSetBlockIt_getNext(&threadBlockIt);
    CuAssertTrue(testCase, block1 != NULL);
    CuAssertPtrEquals(testCase, NULL, stPinchThreadSetBlockIt_getNext(&threadBlockIt));
    blockIt = stPinchBlock_getSegmentIterator(block1);
    CuAssertPtrEquals(testCase, segment1, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment3, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, segment5, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, NULL, stPinchBlockIt_getNext(&blockIt));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment1));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment3));
    CuAssertPtrEquals(testCase, block1, stPinchSegment_getBlock(segment5));
    CuAssertIntEquals(testCase, 5, stPinchBlock_getLength(block1));

    //Test stPinchThreadSet_getTotalBlockNumber
    CuAssertIntEquals(testCase, 1, stPinchThreadSet_getTotalBlockNumber(threadSet));

    teardown();
}

static bool areAligned(stPinchThread *thread1, int64_t base1, stPinchThread *thread2, int64_t base2, bool orientation) {
    stPinchSegment *segment1 = stPinchThread_getSegment(thread1, base1);
    stPinchSegment *segment2 = stPinchThread_getSegment(thread2, base2);
    assert(segment1 != NULL && segment2 != NULL);
    stPinchBlock *block1 = stPinchSegment_getBlock(segment1);
    stPinchBlock *block2 = stPinchSegment_getBlock(segment2);
    if (block1 == NULL) {
        return 0;
    }
    if (block1 != block2) {
        return 0;
    }
    int64_t offset1 = base1 - stPinchSegment_getStart(segment1);
    int64_t offset2 = base2 - stPinchSegment_getStart(segment2);
    assert(base1 >= 0 && base2 >= 0);
    if (orientation) {
        return offset1 == offset2;
    }
    return stPinchBlock_getLength(block1) - 1 - offset2 == offset1;
}

static void testStPinchThread_pinchP(CuTest *testCase, int64_t segmentNumber, int64_t start, int64_t lengths[],
        int64_t blockDegrees[], stPinchThread *thread) {
    stPinchSegment *segment = stPinchThread_getFirst(thread);
    int64_t i = start;
    for (int64_t j = 0; j < segmentNumber; j++) {
        CuAssertTrue(testCase, segment != NULL);
        CuAssertIntEquals(testCase, stPinchThread_getName(thread), stPinchSegment_getName(segment));
        CuAssertIntEquals(testCase, i, stPinchSegment_getStart(segment));
        CuAssertIntEquals(testCase, lengths[j], stPinchSegment_getLength(segment));
        //Block
        if (blockDegrees[j] == 1) {
            CuAssertPtrEquals(testCase, NULL, stPinchSegment_getBlock(segment));
        } else {
            stPinchBlock *block = stPinchSegment_getBlock(segment);
            CuAssertTrue(testCase, block != NULL);
            CuAssertIntEquals(testCase, blockDegrees[j], stPinchBlock_getDegree(block));
        }

        i += stPinchSegment_getLength(segment);
        if (j + 1 == segmentNumber) {
            CuAssertTrue(testCase, stPinchSegment_get3Prime(segment) == NULL);
        } else {
            stPinchSegment *segment2 = stPinchSegment_get3Prime(segment);
            CuAssertTrue(testCase, segment2 != NULL);
            CuAssertPtrEquals(testCase, segment, stPinchSegment_get5Prime(segment2));
            segment = segment2;
        }
    }
}

static void testStPinchThread_pinch(CuTest *testCase) {
    setup();
    stPinchThread_pinch(thread1, thread2, 5, 5, 8, 1);
    int64_t lengths1[] = { 4, 6, 1, 1, length1 - 12 };
    int64_t blockDegrees1[] = { 1, 2, 2, 2, 1 };
    testStPinchThread_pinchP(testCase, 5, start1, lengths1, blockDegrees1, thread1);
    st_logInfo("First thread, first pinch okay\n");
    int64_t lengths2[] = { 1, 6, 1, 1, 1 };
    int64_t blockDegrees2[] = { 1, 2, 2, 2, 1 };
    testStPinchThread_pinchP(testCase, 5, start2, lengths2, blockDegrees2, thread2);
    st_logInfo("Second thread, first pinch okay\n");

    stPinchThread_pinch(thread1, thread2, 4, 10, 4, 0);
    int64_t lengths1b[] = { 3, 1, 1, 1, 1, 2, 1, 1, 1, length1 - 12 };
    int64_t blockDegrees1b[] = { 1, 2, 4, 4, 4, 2, 4, 4, 4, 1 };
    testStPinchThread_pinchP(testCase, 10, start1, lengths1b, blockDegrees1b, thread1);
    st_logInfo("First thread, second pinch okay\n");
    int64_t lengths2b[] = { 1, 1, 1, 1, 2, 1, 1, 1, 1 };
    int64_t blockDegrees2b[] = { 1, 4, 4, 4, 2, 4, 4, 4, 2 };
    testStPinchThread_pinchP(testCase, 9, start2, lengths2b, blockDegrees2b, thread2);
    st_logInfo("Second thread, second pinch okay\n");

    stPinchThread_pinch(thread1, thread2, 4, 10, 4, 0); //Doing the same thing again should not affect the result
    testStPinchThread_pinchP(testCase, 10, start1, lengths1b, blockDegrees1b, thread1);
    testStPinchThread_pinchP(testCase, 9, start2, lengths2b, blockDegrees2b, thread2);
    st_logInfo("Third pinch okay\n");
    //nor should a zero length pinch
    stPinchThread_pinch(thread1, thread2, 5, 10, 0, 0);
    testStPinchThread_pinchP(testCase, 10, start1, lengths1b, blockDegrees1b, thread1);
    testStPinchThread_pinchP(testCase, 9, start2, lengths2b, blockDegrees2b, thread2);
    st_logInfo("Fourth pinch okay\n");

    //Check a subset of the homology groups
    CuAssertTrue(testCase, areAligned(thread1, 4, thread2, 13, 0));

    CuAssertTrue(testCase, areAligned(thread1, 5, thread2, 5, 1));
    CuAssertTrue(testCase, areAligned(thread1, 5, thread2, 12, 0));
    CuAssertTrue(testCase, areAligned(thread1, 5, thread1, 12, 0));

    CuAssertTrue(testCase, areAligned(thread1, 6, thread2, 6, 1));
    CuAssertTrue(testCase, areAligned(thread1, 6, thread2, 11, 0));
    CuAssertTrue(testCase, areAligned(thread1, 6, thread1, 11, 0));

    CuAssertTrue(testCase, areAligned(thread1, 7, thread2, 7, 1));
    CuAssertTrue(testCase, areAligned(thread1, 7, thread2, 10, 0));
    CuAssertTrue(testCase, areAligned(thread1, 7, thread1, 10, 0));

    CuAssertTrue(testCase, areAligned(thread1, 8, thread2, 8, 1));

    CuAssertTrue(testCase, areAligned(thread1, 9, thread2, 9, 1));

    teardown();
}

//Functions that implement a very simple merging of alignment positions

static void addColumn(stHash *columns, int64_t name, int64_t position, int64_t strand) {
    assert(position > 0);
    stIntTuple *p = stIntTuple_construct2(name, (strand ? position : -position));
    stSortedSet *column = stSortedSet_construct3((int(*)(const void *, const void *)) stIntTuple_cmpFn, NULL);
    stSortedSet_insert(column, p);
    stHash_insert(columns, p, column);
}

static stSortedSet *getColumn(stHash *columns, int64_t name, int64_t position, int64_t strand) {
    assert(position != 0);
    stIntTuple *p = stIntTuple_construct2(name, strand ? position : -position);
    stSortedSet *column = stHash_search(columns, p);
    assert(column != NULL);
    stIntTuple_destruct(p);
    return column;
}

static void mergePositions(stHash *columns, int64_t name1, int64_t start1, bool strand1, int64_t name2, int64_t start2,
        bool strand2) {
    stSortedSet *column1 = getColumn(columns, name1, start1, strand1);
    stSortedSet *column2 = getColumn(columns, name2, start2, strand2);
    stSortedSet *column2R = getColumn(columns, name2, start2, !strand2);
    if (column1 != column2 && column1 != column2R) {
        stSortedSet *mergedColumn = stSortedSet_getUnion(column1, column2);
        stSortedSetIterator *it = stSortedSet_getIterator(mergedColumn);
        stIntTuple *position;
        while ((position = stSortedSet_getNext(it)) != NULL) {
            assert(stHash_remove(columns, position) != NULL);
            stHash_insert(columns, position, mergedColumn);
        }
        stSortedSet_destructIterator(it);
        stSortedSet_destruct(column1);
        stSortedSet_destruct(column2);
    }
}

static void mergePositionsSymmetric(stHash *columns, int64_t name1, int64_t start1, bool strand1, int64_t name2,
        int64_t start2, bool strand2) {
    mergePositions(columns, name1, start1, strand1, name2, start2, strand2);
    mergePositions(columns, name1, start1, !strand1, name2, start2, !strand2);
}

stHash *getUnalignedColumns(stPinchThreadSet *threadSet) {
    stHash *columns = stHash_construct3((uint64_t(*)(const void *)) stIntTuple_hashKey,
            (int(*)(const void *, const void *)) stIntTuple_equalsFn, (void(*)(void *)) stIntTuple_destruct, NULL);
    stPinchThreadSetIt it = stPinchThreadSet_getIt(threadSet);
    stPinchThread *thread;
    while ((thread = stPinchThreadSetIt_getNext(&it))) {
        for (int64_t i = 0; i < stPinchThread_getLength(thread); i++) {
            addColumn(columns, stPinchThread_getName(thread), stPinchThread_getStart(thread) + i, 1);
            addColumn(columns, stPinchThread_getName(thread), stPinchThread_getStart(thread) + i, 0);
        }
    }
    return columns;
}

static void decodePosition(stPinchThreadSet *threadSet, stIntTuple *alignedPosition, stPinchThread **thread,
        int64_t *position, bool *strand) {
    *thread = stPinchThreadSet_getThread(threadSet, stIntTuple_get(alignedPosition, 0));
    assert(*thread != NULL);
    *position = stIntTuple_get(alignedPosition, 1);
    assert(*position != 0);
    *strand = 1;
    if (*position < 0) {
        *strand = 0;
        *position *= -1;
    }
}

static void checkPinchSetsAreEquivalentAndCleanup(CuTest *testCase, stPinchThreadSet *threadSet, stHash *columns) {
    //Check they are equivalent
    stHashIterator *hashIt = stHash_getIterator(columns);
    stIntTuple *key;
    while ((key = stHash_getNext(hashIt)) != NULL) {
        stPinchThread *thread1, *thread2;
        int64_t position1, position2;
        bool strand1, strand2;
        stSortedSet *column = stHash_search(columns, key);
        stList *columnList = stSortedSet_getList(column);

        // Check that there's no false positives
        decodePosition(threadSet, stList_get(columnList, 0), &thread1, &position1, &strand1);
        stPinchBlock *block = stPinchSegment_getBlock(stPinchThread_getSegment(thread1, position1));
        int64_t degree = 1;
        if (block != NULL) {
            degree = stPinchBlock_getDegree(block);
        }
        CuAssertIntEquals(testCase, stList_length(columnList), degree);

        for (int64_t i = 0; i < stList_length(columnList); i++) {
            stIntTuple *alignedPosition1 = stList_get(columnList, i);
            decodePosition(threadSet, alignedPosition1, &thread1, &position1, &strand1);

            for (int64_t j = i + 1; j < stList_length(columnList); j++) {
                stIntTuple *alignedPosition2 = stList_get(columnList, j);
                decodePosition(threadSet, alignedPosition2, &thread2, &position2, &strand2);
                // Check that there's no false negatives
                CuAssertTrue(testCase, areAligned(thread1, position1, thread2, position2, strand1 == strand2));
            }
        }
        stList_destruct(columnList);
    }
    stHash_destructIterator(hashIt);
    //cleanup
    stPinchThreadSet_destruct(threadSet);
    stList *columnList = stHash_getValues(columns);
    stSortedSet *columnSet = stList_getSortedSet(columnList, NULL);
    stSortedSet_setDestructor(columnSet, (void(*)(void *)) stSortedSet_destruct);
    stList_destruct(columnList);
    stHash_destruct(columns);
    stSortedSet_destruct(columnSet);
}

static void testStPinchThread_pinch_randomTests(CuTest *testCase) {
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random pinch test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomEmptyGraph();
        stHash *columns = getUnalignedColumns(threadSet);

        //Randomly push them together, updating both sets, and checking that set of alignments is what we expect
        double threshold = st_random();
        while (st_random() > threshold) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);
            stPinchThread_pinch(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand);
            //now do all the pushing together of the equivalence classes
            for (int64_t i = 0; i < pinch.length; i++) {
                mergePositionsSymmetric(columns, pinch.name1, pinch.start1 + i, 1, pinch.name2,
                        pinch.strand ? pinch.start2 + i : pinch.start2 + pinch.length - 1 - i, pinch.strand);
            }
        }
        if (st_random() > 0.5) {
            stPinchThreadSet_joinTrivialBoundaries(threadSet); //Checks this function does not affect result
        }
        checkPinchSetsAreEquivalentAndCleanup(testCase, threadSet, columns);
    }
}

static bool checkIntersection(stSortedSet *names1, stSortedSet *names2) {
    stSortedSet *n12 = stSortedSet_getIntersection(names1, names2);
    bool b = stSortedSet_size(n12) > 0;
    stSortedSet_destruct(names1);
    stSortedSet_destruct(names2);
    stSortedSet_destruct(n12);
    return b;
}

static stSortedSet *getNames(stPinchSegment *segment) {
    stSortedSet *names = stSortedSet_construct3((int(*)(const void *, const void *)) stIntTuple_cmpFn,
            (void(*)(void *)) stIntTuple_destruct);
    if (stPinchSegment_getBlock(segment) != NULL) {
        stPinchBlock *block = stPinchSegment_getBlock(segment);
        stPinchBlockIt it = stPinchBlock_getSegmentIterator(block);
        while ((segment = stPinchBlockIt_getNext(&it)) != NULL) {
            stSortedSet_insert(names, stIntTuple_construct1(stPinchSegment_getName(segment)));
        }
    } else {
        stSortedSet_insert(names, stIntTuple_construct1(stPinchSegment_getName(segment)));
    }
    return names;
}

static bool testStPinchThread_filterPinch_randomTests_filterFn(stPinchSegment *segment1, stPinchSegment *segment2) {
    return checkIntersection(getNames(segment1), getNames(segment2));
}

static stSortedSet *getNames2(stSortedSet *column) {
    stSortedSet *names = stSortedSet_construct3((int(*)(const void *, const void *)) stIntTuple_cmpFn,
            (void(*)(void *)) stIntTuple_destruct);
    stSortedSetIterator *it = stSortedSet_getIterator(column);
    stIntTuple *i;
    while ((i = stSortedSet_getNext(it)) != NULL) {
        stSortedSet_insert(names, stIntTuple_construct1(stIntTuple_get(i, 0)));
    }
    stSortedSet_destructIterator(it);
    return names;
}

static bool canMerge(stSortedSet *column1, stSortedSet *column2) {
    return !checkIntersection(getNames2(column1), getNames2(column2));
}

static void testStPinchThread_filterPinch_randomTests(CuTest *testCase) {
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random pinch test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomEmptyGraph();
        stHash *columns = getUnalignedColumns(threadSet);

        //Randomly push them together, updating both sets, and checking that set of alignments is what we expect
        double threshold = st_random();
        while (st_random() > threshold) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);
            stPinchThread_filterPinch(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand, testStPinchThread_filterPinch_randomTests_filterFn);
            //now do all the pushing together of the equivalence classes
            for (int64_t i = 0; i < pinch.length; i++) {
                stSortedSet *column1 = getColumn(columns, pinch.name1, pinch.start1 + i, 1);
                stSortedSet *column2 = getColumn(columns, pinch.name2,
                        pinch.strand ? pinch.start2 + i : pinch.start2 + pinch.length - 1 - i, pinch.strand);
                if (canMerge(column1, column2)) {
                    mergePositionsSymmetric(columns, pinch.name1, pinch.start1 + i, 1, pinch.name2,
                            pinch.strand ? pinch.start2 + i : pinch.start2 + pinch.length - 1 - i, pinch.strand);
                }
            }
        }
        if (st_random() > 0.5) {
            stPinchThreadSet_joinTrivialBoundaries(threadSet); //Checks this function does not affect result
        }
        checkPinchSetsAreEquivalentAndCleanup(testCase, threadSet, columns);
    }
}

static void testStPinchThreadSet_joinTrivialBoundaries_randomTests(CuTest *testCase) {
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random trivial boundaries test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stPinchThreadSet_joinTrivialBoundaries(threadSet);
        stPinchThreadSetSegmentIt segmentIt = stPinchThreadSet_getSegmentIt(threadSet);
        stPinchSegment *segment;
        while ((segment = stPinchThreadSetSegmentIt_getNext(&segmentIt))) {
            if (stPinchSegment_getBlock(segment) == NULL) {
                stPinchSegment *segment2 = stPinchSegment_get5Prime(segment);
                CuAssertTrue(testCase, segment2 == NULL || stPinchSegment_getBlock(segment2) != NULL);
                segment2 = stPinchSegment_get3Prime(segment);
                CuAssertTrue(testCase, segment2 == NULL || stPinchSegment_getBlock(segment2) != NULL);
            }
        }
        stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
        stPinchBlock *block;
        while ((block = stPinchThreadSetBlockIt_getNext(&blockIt)) != NULL) {
            stPinchEnd end;
            end.block = block;
            end.orientation = 0;
            CuAssertTrue(testCase, !stPinchEnd_boundaryIsTrivial(end));
            end.orientation = 1;
            CuAssertTrue(testCase, !stPinchEnd_boundaryIsTrivial(end));
        }
        stPinchThreadSet_destruct(threadSet);
    }
}

static void testStPinchThreadSet_getAdjacencyComponents(CuTest *testCase) {
    //return;
    setup();
    //Quick check that it returns what we expect
    stPinchThread_pinch(thread1, thread2, 5, 5, 8, 1);
    stList *adjacencyComponents = stPinchThreadSet_getAdjacencyComponents(threadSet);
    CuAssertIntEquals(testCase, 4, stList_length(adjacencyComponents));
    stList_destruct(adjacencyComponents);
    teardown();
}

static void testStPinchThreadSet_getAdjacencyComponents_randomTests(CuTest *testCase) {
    //return;
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random adjacency component test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stList *adjacencyComponents = stPinchThreadSet_getAdjacencyComponents(threadSet);
        //Check all ends in one adjacency component
        stHash *ends = stHash_construct3(stPinchEnd_hashFn, stPinchEnd_equalsFn, NULL, NULL);
        for (int64_t i = 0; i < stList_length(adjacencyComponents); i++) {
            stList *adjacencyComponent = stList_get(adjacencyComponents, i);
            for (int64_t j = 0; j < stList_length(adjacencyComponent); j++) {
                stPinchEnd *end = stList_get(adjacencyComponent, j);
                CuAssertPtrEquals(testCase, NULL, stHash_search(ends, end));
                stHash_insert(ends, end, end);
            }
        }
        stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
        int64_t blockNumber = 0;
        while ((stPinchThreadSetBlockIt_getNext(&blockIt)) != NULL) {
            blockNumber++;
        }
        CuAssertIntEquals(testCase, 2 * blockNumber, stHash_size(ends));
        //Check all connected nodes in same adjacency component
        stPinchThreadSet_destruct(threadSet);
        stHash_destruct(ends);
        stList_destruct(adjacencyComponents);
    }
}

static bool hasSelfLoopWithRespectToOtherBlock(stPinchEnd *end1, stPinchBlock *block2) {
    stPinchBlockIt sIt = stPinchBlock_getSegmentIterator(stPinchEnd_getBlock(end1));
    stPinchSegment *segment;
    while((segment = stPinchBlockIt_getNext(&sIt)) != NULL) {
        bool traverse5Prime = stPinchEnd_traverse5Prime(stPinchEnd_getOrientation(end1), segment);
        while(1) {
            segment = traverse5Prime ? stPinchSegment_get5Prime(segment) : stPinchSegment_get3Prime(segment);
            if(segment == NULL) {
                break;
            }
            if(block2 == stPinchSegment_getBlock(segment) && stPinchEnd_getBlock(end1) != block2) {
                break;
            }
            if(stPinchEnd_getBlock(end1) == stPinchSegment_getBlock(segment)) {
                if(stPinchEnd_endOrientation(traverse5Prime, segment) == stPinchEnd_getOrientation(end1)) {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}

static stList *getListOfBlocks(stPinchThreadSet *threadSet) {
    //Get set of blocks.
    stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
    stList *blocks = stList_construct();
    stPinchBlock *block;
    while((block = stPinchThreadSetBlockIt_getNext(&blockIt)) != NULL) {
        stList_append(blocks, block);
    }
    return blocks;
}

static void testStPinchEnd_hasSelfLoopWithRespectToOtherBlock_randomTests(CuTest *testCase) {
    for (int64_t test = 0; test < 10000; test++) {
        st_logInfo("Starting random has self loop with respect to other end test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stList *blocks = getListOfBlocks(threadSet);
        stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
        //Walk through blocks and randomly pick pairs of sides to test.
        stPinchBlock *block;
        while((block = stPinchThreadSetBlockIt_getNext(&blockIt)) != NULL) {
            stPinchEnd end1 = stPinchEnd_constructStatic(block, st_random() > 0.5);
            stPinchBlock *block2 = st_randomChoice(blocks);
            CuAssertTrue(testCase, stPinchEnd_hasSelfLoopWithRespectToOtherBlock(&end1, block2) == \
                    hasSelfLoopWithRespectToOtherBlock(&end1, block2));
        }
        stPinchThreadSet_destruct(threadSet);
        stList_destruct(blocks);
    }
}

static stList *getSubSequenceLengthsConnectingEnds(stPinchEnd *end1, stPinchEnd *end2) {
    stPinchBlockIt sIt = stPinchBlock_getSegmentIterator(stPinchEnd_getBlock(end1));
    stPinchSegment *segment;
    stList *lengths = stList_construct3(0, (void (*)(void *))stIntTuple_destruct);
    while((segment = stPinchBlockIt_getNext(&sIt)) != NULL) {
        stPinchSegment *startSegment = segment;
        bool traverse5Prime = stPinchEnd_traverse5Prime(stPinchEnd_getOrientation(end1), segment);
        while(1) {
            segment = traverse5Prime ? stPinchSegment_get5Prime(segment) : stPinchSegment_get3Prime(segment);
            if(segment == NULL) {
                break;
            }
            if(stPinchEnd_getBlock(end1) == stPinchSegment_getBlock(segment) && stPinchEnd_getBlock(end1) != stPinchEnd_getBlock(end2)) {
                break;
            }
            if(stPinchEnd_getBlock(end2) == stPinchSegment_getBlock(segment)) {
                if(stPinchEnd_endOrientation(traverse5Prime, segment) == stPinchEnd_getOrientation(end2)) {
                    if(traverse5Prime) {
                        assert(stPinchSegment_getStart(startSegment) >= stPinchSegment_getStart(segment) + stPinchSegment_getLength(segment));
                        stList_append(lengths, stIntTuple_construct1(stPinchSegment_getStart(startSegment) - (stPinchSegment_getStart(segment) + stPinchSegment_getLength(segment))));
                    }
                    else if(!stPinchEnd_equalsFn(end1, end2)) { //if statement ensures we don't double count end end1 equals end2
                        assert(stPinchSegment_getStart(startSegment) + stPinchSegment_getLength(startSegment) <= stPinchSegment_getStart(segment));
                        stList_append(lengths, stIntTuple_construct1(stPinchSegment_getStart(segment) - (stPinchSegment_getStart(startSegment) + stPinchSegment_getLength(startSegment))));
                    }
                }
                break;
            }
        }
    }
    return lengths;
}

static void testStPinchEnd_getSubSequenceLengthsConnectingEnds_randomTests(CuTest *testCase) {
    for (int64_t test = 0; test < 10000; test++) {
        st_logInfo("Starting random get total incident sequence connecting ends test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stList *blocks = getListOfBlocks(threadSet);
        stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
        //Walk through blocks and randomly pick pairs of sides to test.
        stPinchBlock *block;
        while((block = stPinchThreadSetBlockIt_getNext(&blockIt)) != NULL) {
            stPinchEnd end1 = stPinchEnd_constructStatic(block, st_random() > 0.5);
            stPinchEnd end2 = stPinchEnd_constructStatic(st_randomChoice(blocks), st_random() > 0.5);
            stList *lengths1 = getSubSequenceLengthsConnectingEnds(&end1, &end2);
            stList *lengths2 = stPinchEnd_getSubSequenceLengthsConnectingEnds(&end1, &end2);
            stSortedSet *lengths1Set = stList_getSortedSet(lengths1, (int (*)(const void *, const void *))stIntTuple_cmpFn);
            stSortedSet *lengths2Set = stList_getSortedSet(lengths2, (int (*)(const void *, const void *))stIntTuple_cmpFn);
            CuAssertTrue(testCase, stSortedSet_equals(lengths1Set, lengths2Set));
            stSortedSet_destruct(lengths1Set);
            stSortedSet_destruct(lengths2Set);
            stList_destruct(lengths1);
            stList_destruct(lengths2);
        }
        stPinchThreadSet_destruct(threadSet);
        stList_destruct(blocks);
    }
}

static void testStPinchThreadSet_getThreadComponents(CuTest *testCase) {
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random thread component test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stSortedSet *threadComponents = stPinchThreadSet_getThreadComponents(threadSet);
        stSortedSetIterator *it = stSortedSet_getIterator(threadComponents);
        stList *threadComponent;
        stSortedSet *threads = stSortedSet_construct();
        while ((threadComponent = stSortedSet_getNext(it)) != NULL) {
            stSortedSet *threadComponentSet = stList_getSortedSet(threadComponent, NULL);
            for (int64_t i = 0; i < stList_length(threadComponent); i++) {
                stPinchThread *thread = stList_get(threadComponent, i);
                CuAssertTrue(testCase, stSortedSet_search(threads, thread) == NULL); //Check is unique;
                stSortedSet_insert(threads, thread);
                stPinchSegment *segment = stPinchThread_getFirst(thread);
                while (segment != NULL) {
                    stPinchBlock *block;
                    if ((block = stPinchSegment_getBlock(segment)) != NULL) {
                        stPinchBlockIt segmentIt = stPinchBlock_getSegmentIterator(block);
                        stPinchSegment *segment2;
                        while ((segment2 = stPinchBlockIt_getNext(&segmentIt)) != NULL) {
                            CuAssertTrue(testCase,
                                    stSortedSet_search(threadComponentSet, stPinchSegment_getThread(segment2)) != NULL);
                        }
                    }
                    segment = stPinchSegment_get3Prime(segment);
                }
            }
            stSortedSet_destruct(threadComponentSet);
        } st_logInfo("There were %" PRIi64 " threads in %" PRIi64 " components\n", stPinchThreadSet_getSize(threadSet), stSortedSet_size(threadComponents));
        CuAssertIntEquals(testCase, stPinchThreadSet_getSize(threadSet), stSortedSet_size(threads));
        stSortedSet_destructIterator(it);
        stSortedSet_destruct(threadComponents);
        stSortedSet_destruct(threads);
        stPinchThreadSet_destruct(threadSet);
    }
}

static void testStPinchThreadSet_trimAlignments_randomTests(CuTest *testCase) {
    //return;
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random block trim test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        int64_t trim = st_randomInt(0, 10);
        stPinchThreadSetBlockIt blockIt = stPinchThreadSet_getBlockIt(threadSet);
        stPinchBlock *block = stPinchThreadSetBlockIt_getNext(&blockIt);
        while (block != NULL) {
            stPinchBlock *block2 = stPinchThreadSetBlockIt_getNext(&blockIt);
            stPinchBlock_trim(block, trim);
            block = block2;
        }
        stPinchThreadSet_destruct(threadSet);
    }
}

static void testStPinchInterval(CuTest *testCase) {
    setup();
    stPinchInterval *interval = stPinchInterval_construct(name1, start1, length1, testCase);
    CuAssertIntEquals(testCase, name1, stPinchInterval_getName(interval));
    CuAssertIntEquals(testCase, start1, stPinchInterval_getStart(interval));
    CuAssertIntEquals(testCase, length1, stPinchInterval_getLength(interval));
    CuAssertPtrEquals(testCase, testCase, stPinchInterval_getLabel(interval));
    stPinchInterval interval2 = stPinchInterval_constructStatic(name2, start2, length2, testCase);
    CuAssertIntEquals(testCase, name2, stPinchInterval_getName(&interval2));
    CuAssertIntEquals(testCase, start2, stPinchInterval_getStart(&interval2));
    CuAssertIntEquals(testCase, length2, stPinchInterval_getLength(&interval2));
    CuAssertPtrEquals(testCase, testCase, stPinchInterval_getLabel(&interval2));
    CuAssertIntEquals(testCase, 0, stPinchInterval_compareFunction(interval, interval));
    CuAssertIntEquals(testCase, 0, stPinchInterval_compareFunction(&interval2, &interval2));
    CuAssertIntEquals(testCase, -1, stPinchInterval_compareFunction(interval, &interval2));
    CuAssertIntEquals(testCase, 1, stPinchInterval_compareFunction(&interval2, interval));
    stSortedSet *intervals = stSortedSet_construct3(
            (int(*)(const void *, const void *)) stPinchInterval_compareFunction, NULL);
    CuAssertPtrEquals(testCase, NULL, stSortedSet_search(intervals, interval));
    CuAssertPtrEquals(testCase, NULL, stSortedSet_search(intervals, &interval2));
    stSortedSet_insert(intervals, interval);
    stSortedSet_insert(intervals, &interval2);
    CuAssertPtrEquals(testCase, interval, stSortedSet_search(intervals, interval));
    CuAssertPtrEquals(testCase, &interval2, stSortedSet_search(intervals, &interval2));
    CuAssertPtrEquals(testCase, interval, stPinchIntervals_getInterval(intervals, name1, start1));
    CuAssertPtrEquals(testCase, interval, stPinchIntervals_getInterval(intervals, name1, start1 + length1 - 1));
    CuAssertPtrEquals(testCase, NULL, stPinchIntervals_getInterval(intervals, name1, start1 + length1));
    CuAssertPtrEquals(testCase, NULL, stPinchIntervals_getInterval(intervals, name1, start1 - 1));
    stSortedSet_destruct(intervals);
    stPinchInterval_destruct(interval);
    teardown();
}

static void testStPinchThreadSet_getLabelIntervals(CuTest *testCase) {
    setup();
    //Tests when there are no blocks in the problem
    stHash *pinchEndsToAdjacencyComponents;
    stList *adjacencyComponents = stPinchThreadSet_getAdjacencyComponents2(threadSet, &pinchEndsToAdjacencyComponents);
    stSortedSet *intervals = stPinchThreadSet_getLabelIntervals(threadSet, pinchEndsToAdjacencyComponents);
    CuAssertIntEquals(testCase, 2, stSortedSet_size(intervals));
    stPinchInterval *interval = stPinchIntervals_getInterval(intervals, name1, start1);
    CuAssertTrue(testCase, interval != NULL);
    CuAssertIntEquals(testCase, name1, stPinchInterval_getName(interval));
    CuAssertIntEquals(testCase, start1, stPinchInterval_getStart(interval));
    CuAssertIntEquals(testCase, length1, stPinchInterval_getLength(interval));
    CuAssertPtrEquals(testCase, NULL, stPinchInterval_getLabel(interval));
    interval = stPinchIntervals_getInterval(intervals, name2, start2);
    CuAssertTrue(testCase, interval != NULL);
    CuAssertIntEquals(testCase, name2, stPinchInterval_getName(interval));
    CuAssertIntEquals(testCase, start2, stPinchInterval_getStart(interval));
    CuAssertIntEquals(testCase, length2, stPinchInterval_getLength(interval));
    CuAssertPtrEquals(testCase, NULL, stPinchInterval_getLabel(interval));
    stHash_destruct(pinchEndsToAdjacencyComponents);
    stSortedSet_destruct(intervals);
    stList_destruct(adjacencyComponents);
    teardown();
}

static stList *getAdjacencyComponentP(stPinchSegment *segment, int64_t position, stHash *pinchEndsToAdjacencyComponents) {
    stPinchBlock *block = stPinchSegment_getBlock(segment);
    assert(block != NULL);
    bool orientation = (position >= stPinchSegment_getLength(segment) / 2)
            ^ stPinchSegment_getBlockOrientation(segment);
    stPinchEnd pinchEnd = stPinchEnd_constructStatic(block, orientation);
    stList *adjacencyComponent = stHash_search(pinchEndsToAdjacencyComponents, &pinchEnd);
    assert(adjacencyComponent != NULL);
    return adjacencyComponent;
}

static stList *getAdjacencyComponent(stPinchSegment *segment, int64_t position, stHash *pinchEndsToAdjacencyComponents) {
    if (stPinchSegment_getBlock(segment) != NULL) {
        return getAdjacencyComponentP(segment, position, pinchEndsToAdjacencyComponents);
    }
    stPinchSegment *segment2 = stPinchSegment_get3Prime(segment);
    while (segment2 != NULL) {
        if (stPinchSegment_getBlock(segment2) != NULL) {
            return getAdjacencyComponentP(segment2, -1, pinchEndsToAdjacencyComponents);
        }
        segment2 = stPinchSegment_get3Prime(segment2);
    }
    segment2 = stPinchSegment_get5Prime(segment);
    while (segment2 != NULL) {
        if (stPinchSegment_getBlock(segment2) != NULL) {
            return getAdjacencyComponentP(segment2, INT64_MAX, pinchEndsToAdjacencyComponents);
        }
        segment2 = stPinchSegment_get5Prime(segment2);
    }
    return NULL;
}

static void testStPinchThreadSet_getLabelIntervals_randomTests(CuTest *testCase) {
    //return;
    for (int64_t test = 0; test < 100; test++) {
        st_logInfo("Starting random get label intervals test %" PRIi64 "\n", test);
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomGraph();
        stHash *pinchEndsToAdjacencyComponents;
        stList *adjacencyComponents = stPinchThreadSet_getAdjacencyComponents2(threadSet,
                &pinchEndsToAdjacencyComponents);
        stSortedSet *intervals = stPinchThreadSet_getLabelIntervals(threadSet, pinchEndsToAdjacencyComponents);
        //Check every base is in a label interval
        stPinchThreadSetSegmentIt segmentIt = stPinchThreadSet_getSegmentIt(threadSet);
        stPinchSegment *segment;
        while ((segment = stPinchThreadSetSegmentIt_getNext(&segmentIt)) != NULL) {
            for (int64_t i = 0; i < stPinchSegment_getLength(segment); i++) {
                stPinchInterval *interval = stPinchIntervals_getInterval(intervals, stPinchSegment_getName(segment),
                        stPinchSegment_getStart(segment) + i);
                CuAssertTrue(testCase, interval != NULL);
                CuAssertIntEquals(testCase, stPinchSegment_getName(segment), stPinchInterval_getName(interval));
                CuAssertTrue(testCase, stPinchSegment_getStart(segment) + i >= stPinchInterval_getStart(interval));
                CuAssertTrue(
                        testCase,
                        stPinchSegment_getStart(segment) + i < stPinchInterval_getStart(interval)
                                + stPinchInterval_getLength(interval));
                //Now check out stupid way of calculating the adjacency component is the same as the calculated by the label function.
                CuAssertPtrEquals(testCase, getAdjacencyComponent(segment, i, pinchEndsToAdjacencyComponents),
                        stPinchInterval_getLabel(interval));
            }
        }
        //Cleanup
        stSortedSet_destruct(intervals);
        stHash_destruct(pinchEndsToAdjacencyComponents);
        stList_destruct(adjacencyComponents);
        stPinchThreadSet_destruct(threadSet);
    }
}

// Check that the given thread has the number of supporting homologies
// for each block specified in supportingHomologiesArray, which is
// indexed by segment index along the thread.
static void testStPinchBlock_getNumSupportingHomologiesP(CuTest *testCase,
                                                          stPinchThread *thread,
                                                          int64_t *supportingHomologiesArray,
                                                          size_t supportingHomologiesArraySize) {
    stPinchSegment *segment = stPinchThread_getFirst(thread);
    int64_t i = 0;
    while (segment != NULL) {
        stPinchBlock *block = stPinchSegment_getBlock(segment);
        if (block != NULL) {
            CuAssertTrue(testCase, i < supportingHomologiesArraySize);
            CuAssertIntEquals(testCase, supportingHomologiesArray[i], stPinchBlock_getNumSupportingHomologies(block));
        }
        i++;
        segment = stPinchSegment_get3Prime(segment);
    }
}

// A mirror of test_stPinchBlock_pinch, except that we check that the
// number of supporting homologies (i.e. total number of times the
// segments in a block were pinched) makes sense.
static void testStPinchBlock_getNumSupportingHomologies(CuTest *testCase) {
    setup();
    // First pinch
    stPinchThread_pinch(thread1, thread2, 5, 5, 8, 1);
    // Here, and in all other cases, -1 is chosen as an impossible
    // value for the number of supporting homologies--signifying that
    // the segment doesn't belong to a block.
    int64_t supportingHomologies1[] = { -1, 1, 1, 1, -1 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1,
                                                 sizeof(supportingHomologies1));
    st_logInfo("First thread, first pinch okay\n");
    
    int64_t supportingHomologies2[] = { -1, 1, 1, 1, -1 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2,
                                                 sizeof(supportingHomologies2));
    st_logInfo("Second thread, first pinch okay\n");


    // Second pinch
    stPinchThread_pinch(thread1, thread2, 4, 10, 4, 0);
    int64_t supportingHomologies1b[] = { -1, 1, 3, 3, 3, 1, 3, 3, 3, -1};
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1b,
                                                 sizeof(supportingHomologies1b));
    st_logInfo("First thread, second pinch okay\n");
    int64_t supportingHomologies2b[] = { -1, 3, 3, 3, 1, 3, 3, 3, 1 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2b,
                                                 sizeof(supportingHomologies2b));
    st_logInfo("Second thread, second pinch okay\n");

    // Third pinch, exactly the same as the second pinch. This should
    // increase the homology support by 1 for the affected blocks, but leave
    // everything else the same.
    stPinchThread_pinch(thread1, thread2, 4, 10, 4, 0);
    int64_t supportingHomologies1c[] = { -1, 2, 4, 4, 4, 1, 4, 4, 4, -1};
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1c,
                                                 sizeof(supportingHomologies1c));
    st_logInfo("First thread, third pinch okay\n");
    int64_t supportingHomologies2c[] = { -1, 4, 4, 4, 1, 4, 4, 4, 2 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2c,
                                                 sizeof(supportingHomologies2c));
    st_logInfo("Second thread, third pinch okay\n");
    st_logInfo("Third pinch okay\n");

    // However, a zero-length pinch should not increase the homology support at all.
    stPinchThread_pinch(thread1, thread2, 5, 10, 0, 0);
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1c,
                                                 sizeof(supportingHomologies1c));
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2c,
                                                 sizeof(supportingHomologies2c));
    st_logInfo("Fourth pinch okay\n");

    // Now check that the homology support is preserved when splitting.
    stPinchSegment *segment = stPinchThread_getSegment(thread1, 8);
    // Add one more support to its block
    stPinchBlock_pinch(stPinchSegment_getBlock(segment), stPinchSegment_getBlock(segment), 0);
    stPinchSegment_split(segment, 8);
    int64_t supportingHomologies1d[] = { -1, 2, 4, 4, 4, 2, 2, 4, 4, 4, -1};
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1d,
                                                 sizeof(supportingHomologies1d));

    // Ensure that when joining trivial boundaries,
    // a) blocks that have trivial boundaries and identical support
    // are joined together
    stPinchThreadSet_joinTrivialBoundaries(threadSet);
    int64_t supportingHomologies1e[] = { -1, 2, 4, 2, 4, -1};
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1e,
                                                 sizeof(supportingHomologies1e));
    int64_t supportingHomologies2e[] = { -1, 4, 2, 4, 2 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2e,
                                                 sizeof(supportingHomologies2e));

    // and b) blocks with different homology support but trivial
    // boundaries are *not* joined together.
    segment = stPinchThread_getSegment(thread1, 8);
    stPinchSegment_split(segment, 8);
    // Add one more support to the left block only
    segment = stPinchThread_getSegment(thread1, 8);
    stPinchBlock_pinch(stPinchSegment_getBlock(segment), stPinchSegment_getBlock(segment), 0);
    int64_t supportingHomologies1f[] = { -1, 2, 4, 3, 2, 4, -1};
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread1, supportingHomologies1f,
                                                 sizeof(supportingHomologies1f));
    int64_t supportingHomologies2f[] = { -1, 4, 3, 2, 4, 2 };
    testStPinchBlock_getNumSupportingHomologiesP(testCase, thread2, supportingHomologies2f,
                                                 sizeof(supportingHomologies2f));

    teardown();
}

// A mirror of test_stPinchBlock_pinch, except that we check that
// pinches can be undone.
static void testStPinchUndo(CuTest *testCase) {
    setup();

    // Get rid of the splits on thread1. Undos can leave extra trivial
    // boundaries, and it's best to join the trivial boundaries to
    // check that the homologies are preserved instead.
    stPinchThreadSet_joinTrivialBoundaries(threadSet);

    // Before pinching
    int64_t lengths1[] = { length1 };
    int64_t blockDegrees1[] = { 1 };
    testStPinchThread_pinchP(testCase, 1, start1, lengths1, blockDegrees1, thread1);
    int64_t lengths2[] = { length2 };
    int64_t blockDegrees2[] = { 1 };
    testStPinchThread_pinchP(testCase, 1, start2, lengths2, blockDegrees2, thread2);

    stPinchUndo *undo1 = stPinchThread_prepareUndo(thread1, thread2, 5, 5, 8, 1);
    stPinchThread_pinch(thread1, thread2, 5, 5, 8, 1);
    int64_t lengths1a[] = { 4, 8, length1 - 12 };
    int64_t blockDegrees1a[] = { 1, 2, 1 };
    testStPinchThread_pinchP(testCase, 3, start1, lengths1a, blockDegrees1a, thread1);
    st_logInfo("First thread, first pinch okay\n");
    int64_t lengths2a[] = { 1, 8, 1 };
    int64_t blockDegrees2a[] = { 1, 2, 1 };
    testStPinchThread_pinchP(testCase, 3, start2, lengths2a, blockDegrees2a, thread2);
    st_logInfo("Second thread, first pinch okay\n");

    // Undo the pinch and check that we're the same as before.
    stPinchThreadSet_undoPinch(threadSet, undo1);
    stPinchThreadSet_joinTrivialBoundaries(threadSet); // Undo can leave extra breakpoints.
    testStPinchThread_pinchP(testCase, 1, start1, lengths1, blockDegrees1, thread1);
    testStPinchThread_pinchP(testCase, 1, start2, lengths2, blockDegrees2, thread2);
    st_logInfo("Pinch 1 undone successfully\n");
    stPinchUndo_destruct(undo1);

    // Reapply pinch 1 and add pinch 2.
    stPinchThread_pinch(thread1, thread2, 5, 5, 8, 1);
    stPinchUndo *undo2 = stPinchThread_prepareUndo(thread1, thread2, 4, 10, 4, 0);
    stPinchThread_pinch(thread1, thread2, 4, 10, 4, 0);
    stPinchThreadSet_joinTrivialBoundaries(threadSet);
    int64_t lengths1b[] = { 3, 1, 3, 2, 3, length1 - 12 };
    int64_t blockDegrees1b[] = { 1, 2, 4, 2, 4, 1 };
    testStPinchThread_pinchP(testCase, 6, start1, lengths1b, blockDegrees1b, thread1);
    st_logInfo("First thread, second pinch okay\n");
    int64_t lengths2b[] = { 1, 3, 2, 3, 1 };
    int64_t blockDegrees2b[] = { 1, 4, 2, 4, 2 };
    testStPinchThread_pinchP(testCase, 5, start2, lengths2b, blockDegrees2b, thread2);
    st_logInfo("Second thread, second pinch okay\n");

    stPinchThreadSet_undoPinch(threadSet, undo2);
    stPinchThreadSet_joinTrivialBoundaries(threadSet);

    testStPinchThread_pinchP(testCase, 3, start1, lengths1a, blockDegrees1a, thread1);
    testStPinchThread_pinchP(testCase, 3, start2, lengths2a, blockDegrees2a, thread2);
    stPinchUndo_destruct(undo2);
    st_logInfo("Pinch 2 undone successfully\n");

    // Test undoing a zero-length pinch
    stPinchUndo *undo3 = stPinchThread_prepareUndo(thread1, thread2, 5, 10, 0, 0);
    stPinchThread_pinch(thread1, thread2, 5, 10, 0, 0);
    testStPinchThread_pinchP(testCase, 3, start1, lengths1a, blockDegrees1a, thread1);
    testStPinchThread_pinchP(testCase, 3, start2, lengths2a, blockDegrees2a, thread2);
    st_logInfo("Third pinch okay\n");

    stPinchThreadSet_undoPinch(threadSet, undo3);
    stPinchThreadSet_joinTrivialBoundaries(threadSet);
    testStPinchThread_pinchP(testCase, 3, start1, lengths1a, blockDegrees1a, thread1);
    testStPinchThread_pinchP(testCase, 3, start2, lengths2a, blockDegrees2a, thread2);
    st_logInfo("Pinch 3 undone successfully\n");
    stPinchUndo_destruct(undo3);

    teardown();
}

static void testStPinchUndo_random(CuTest *testCase) {
    for (int64_t testNum = 0; testNum < 100; testNum++) {
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomEmptyGraph();
        stHash *columns = getUnalignedColumns(threadSet);

        //Randomly push them together, updating both sets, and checking that set of alignments is what we expect
        double threshold = st_random();
        if (threshold < 0.01) {
            threshold = 0.01; // Just to prevent the test from
                              // exploding every so often.
        }
        while (st_random() > threshold) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);

            stPinchUndo *undo = stPinchThread_prepareUndo(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand);
            stPinchThread_pinch(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand);

            if (st_random() > 0.5) {
                //now do all the pushing together of the equivalence classes
                for (int64_t i = 0; i < pinch.length; i++) {
                    mergePositionsSymmetric(columns, pinch.name1, pinch.start1 + i, 1, pinch.name2,
                                            pinch.strand ? pinch.start2 + i : pinch.start2 + pinch.length - 1 - i, pinch.strand);
                }
            } else {
                stPinchThreadSet_undoPinch(threadSet, undo);
            }

            stPinchUndo_destruct(undo);
        }
        if (st_random() > 0.5) {
            stPinchThreadSet_joinTrivialBoundaries(threadSet); //Checks this function does not affect result
        }
        checkPinchSetsAreEquivalentAndCleanup(testCase, threadSet, columns);
        st_logInfo("Random undo test %" PRIi64 " passed\n", testNum);
    }
}

static void testStPinchUndo_chains(CuTest *testCase) {
    for (int64_t testNum = 0; testNum < 100; testNum++) {
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomEmptyGraph();
        int64_t pinchSeriesLength = st_randomInt64(0, 100);
        stList *undos = stList_construct3(0, (void (*)(void *)) stPinchUndo_destruct);

        // Pinch together a bunch of random places
        for (int64_t i = 0; i < pinchSeriesLength; i++) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);
            stPinchThread *thread1 = stPinchThreadSet_getThread(threadSet, pinch.name1);
            stPinchThread *thread2 = stPinchThreadSet_getThread(threadSet, pinch.name2);
            stList_append(undos, stPinchThread_prepareUndo(thread1, thread2, pinch.start1, pinch.start2, pinch.length, pinch.strand));
            stPinchThread_pinch(thread1, thread2, pinch.start1, pinch.start2, pinch.length, pinch.strand);
        }

        // Now undo the pinches and check that we get the same empty graph
        for (int64_t i = pinchSeriesLength - 1; i >= 0; i--) {
            stPinchUndo *undo = stList_get(undos, i);
            stPinchThreadSet_undoPinch(threadSet, undo);
        }
        stPinchThreadSet_joinTrivialBoundaries(threadSet);
        CuAssertIntEquals(testCase, 0, stPinchThreadSet_getTotalBlockNumber(threadSet));

        stList_destruct(undos);
        stPinchThreadSet_destruct(threadSet);
    }
}

// This test is a bit of a copout. Just checks that partially undoing
// all parts of a pinch is equivalent to undoing the whole pinch. It
// would be better to have a test that takes into account the
// transitive undo effects.
static void testStPinchPartialUndo_random(CuTest *testCase) {
    for (int64_t testNum = 0; testNum < 100; testNum++) {
        stPinchThreadSet *threadSet = stPinchThreadSet_getRandomEmptyGraph();
        stHash *columns = getUnalignedColumns(threadSet);

        //Randomly push them together, updating both sets, and checking that set of alignments is what we expect
        double threshold = st_random();
        if (threshold < 0.01) {
            threshold = 0.01; // Just to prevent the test from
                              // exploding every so often.
        }
        while (st_random() > threshold) {
            stPinch pinch = stPinchThreadSet_getRandomPinch(threadSet);

            stPinchUndo *undo = stPinchThread_prepareUndo(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand);
            stPinchThread_pinch(stPinchThreadSet_getThread(threadSet, pinch.name1),
                    stPinchThreadSet_getThread(threadSet, pinch.name2), pinch.start1, pinch.start2, pinch.length,
                    pinch.strand);

            if (st_random() > 0.5) {
                //now do all the pushing together of the equivalence classes
                for (int64_t i = 0; i < pinch.length; i++) {
                    mergePositionsSymmetric(columns, pinch.name1, pinch.start1 + i, 1, pinch.name2,
                                            pinch.strand ? pinch.start2 + i : pinch.start2 + pinch.length - 1 - i, pinch.strand);
                }
            } else {
                int64_t offset = 0;
                while (offset < pinch.length) {
                    int64_t undoLength = st_randomInt64(0, pinch.length - offset + 1);
                    stPinchThreadSet_partiallyUndoPinch(threadSet, undo, offset, undoLength);
                    offset += undoLength;
                }
            }

            stPinchUndo_destruct(undo);
        }
        if (st_random() > 0.5) {
            stPinchThreadSet_joinTrivialBoundaries(threadSet); //Checks this function does not affect result
        }
        checkPinchSetsAreEquivalentAndCleanup(testCase, threadSet, columns);
        st_logInfo("Random undo test %" PRIi64 " passed\n", testNum);
    }
}

CuSuite* stPinchGraphsTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testStPinchThreadSet);
    SUITE_ADD_TEST(suite, testStPinchThreadAndSegment);
    SUITE_ADD_TEST(suite, testStPinchBlock_NoSplits);
    SUITE_ADD_TEST(suite, testStPinchBlock_Splits);
    SUITE_ADD_TEST(suite, testStPinchThread_pinch);
    SUITE_ADD_TEST(suite, testStPinchThread_pinch_randomTests);
    SUITE_ADD_TEST(suite, testStPinchThread_filterPinch_randomTests);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_getAdjacencyComponents);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_getAdjacencyComponents_randomTests);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_joinTrivialBoundaries_randomTests);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_getThreadComponents);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_trimAlignments_randomTests);
    SUITE_ADD_TEST(suite, testStPinchInterval);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_getLabelIntervals);
    SUITE_ADD_TEST(suite, testStPinchThreadSet_getLabelIntervals_randomTests);
    SUITE_ADD_TEST(suite, testStPinchEnd_hasSelfLoopWithRespectToOtherBlock_randomTests);
    SUITE_ADD_TEST(suite, testStPinchEnd_getSubSequenceLengthsConnectingEnds_randomTests);
    SUITE_ADD_TEST(suite, testStPinchBlock_getNumSupportingHomologies);
    SUITE_ADD_TEST(suite, testStPinchUndo);
    SUITE_ADD_TEST(suite, testStPinchUndo_random);
    SUITE_ADD_TEST(suite, testStPinchUndo_chains);
    SUITE_ADD_TEST(suite, testStPinchPartialUndo_random);

    return suite;
}
