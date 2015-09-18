/*
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "CuTest.h"
#include "sonLib.h"

CuSuite* stCactusGraphsTestSuite(void);
CuSuite* stPinchGraphsTestSuite(void);
CuSuite* stPinchPhylogenyTestSuite(void);
CuSuite* stOnlineCactusTestSuite(void);
CuSuite* stOnlinePinchToCactusTestSuite(void);

int stPinchesAndCactiRunAllTests(void) {
    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();
    /* CuSuiteAddSuite(suite, stPinchGraphsTestSuite()); */
    /* CuSuiteAddSuite(suite, stCactusGraphsTestSuite()); */
    /* CuSuiteAddSuite(suite, stPinchPhylogenyTestSuite()); */
    CuSuiteAddSuite(suite, stOnlineCactusTestSuite());
    CuSuiteAddSuite(suite, stOnlinePinchToCactusTestSuite());

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
    CuStringDelete(output);
    bool failed = suite->failCount > 0;
    CuSuiteDelete(suite);
    return failed;
}

int main(int argc, char *argv[]) {
    if(argc == 2) {
        st_setLogLevelFromString(argv[1]);
    }
    int i = stPinchesAndCactiRunAllTests();
    //while(1);
    return i;
}
