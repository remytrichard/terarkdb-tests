//
// Created by terark on 16-8-11.
//

#ifndef TERARKDB_TEST_FRAMEWORK_ANALYSIS_THREAD_H
#define TERARKDB_TEST_FRAMEWORK_ANALYSIS_THREAD_H

#include <thread>
#include "Stats.h"
#include "mysql_driver.h"

class AnalysisWorker{
private:
    std::thread worker;
    sql::Connection* conn;
public:
    AnalysisWorker();
    ~AnalysisWorker();
    void run();
};
#endif //TERARKDB_TEST_FRAMEWORK_ANALYSIS_THREAD_H
