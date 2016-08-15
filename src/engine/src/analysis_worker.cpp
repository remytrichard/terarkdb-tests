//
// Created by terark on 16-8-11.
//
#include "stdlib.h"
#include "inttypes.h"
#include "cppconn/prepared_statement.h"

#include "analysis_worker.h"
#include "Stats.h"


class TimeBucket{
private:
    sql::Connection* conn = nullptr;
    uint64_t step_in_seconds = 10; // in seconds

    int findTimeBucket(uint64_t time) {
        uint64_t t = time / (1000 * 1000 * 1000 * step_in_seconds);
        // printf("find time bucket : %" PRIu64 ", result = %" PRIu64 "\n", time, t*10);
        return t*10;
    }

    int getCurrentCPURate(){
        return 10;
    }

    /**
     * total, free, cached, used
     *
     * @return in MB
     */
    int* getCurrentMemory(){
        int arr[] = {64000, 30000, 20000, 14000};
        return arr;
    }
    /**
     * upload data into MySQL cloud database
     * @param bucket
     * @param ops
     * @param type
     */
    void upload(int bucket, int ops, int type){
        assert(conn != nullptr);
        std::string engine = "terarkdb";

        // 上传ops数据
        sql::PreparedStatement* ps_ops = conn->prepareStatement("INSERT INTO engine_test_ops_10s(time_bucket, ops, ops_type, engine_name) VALUES(?, ?, ?, ?)");
        ps_ops->setInt(1, bucket);
        ps_ops->setInt(2, ops);
        ps_ops->setInt(3, type);
        ps_ops->setString(4, engine);
        //ps->executeUpdate();
        delete ps_ops;
        printf("upload time bucket[%d], ops = %d, type = %d\n", bucket, ops, type);

        // 上传内存数据
        sql::PreparedStatement* ps_memory = conn->prepareStatement("INSERT INTO engine_test_memory_10s(time_bucket, total_memory, free_memory, cached_memory, used_memory, engine_name) VALUES(?, ?, ?, ?, ?, ?)");
        int mem[] = getCurrentMemory();
        ps_cpu->setInt(1, bucket);
        ps_cpu->setInt(2, mem[0]);
        ps_cpu->setInt(3, mem[1]);
        ps_cpu->setInt(4, mem[2]);
        ps_cpu->setInt(5, mem[3]);
        ps_cpu->setString(6, engine);
        delete ps_memory;

        // 上传CPU数据
        sql::PreparedStatement* ps_cpu = conn->prepareStatement("INSERT INTO engine_test_cpu_10s(time_bucket, usage, engine_name) VALUES(?, ?, ?)");
        ps_cpu->setInt(1, bucket);
        ps_cpu->setInt(2, getCurrentCPURate());
        ps_cpu->setString(3, engine);

        delete ps_cpu;
    }

public:
    uint64_t current_bucket = 0;  // seconds
    int operation_count = 0;

    TimeBucket(sql::Connection* const conn){
        this->conn = conn;
    }

    /**
     *
     * @param start
     * @param end
     * @param type 0:read, 1:insert, 2:update
     */
    void add(uint64_t start, uint64_t end, int type){
        if(start <=0 || end <= 0 || start == end) {
            return;
        }
        // when meet the next bucket, upload previous one first, default step is 10 seconds
        int next_bucket = findTimeBucket(start);
        if(next_bucket > current_bucket) {
            int ops = operation_count * 5 / (int)step_in_seconds; // sample rate is 20%, here we multiply it back.
            upload(current_bucket, ops, type);
            operation_count = 1;
            current_bucket = next_bucket;
        }else{
            operation_count++;
        }
    }
};


AnalysisWorker::AnalysisWorker() {
    // init mysql connection
    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    // env : MYSQL_PASSWD
    const char* passwd = getenv("MYSQL_PASSWD");
    if(passwd == NULL) {
        printf("Please set env MYSQL_PASSWD !\n");
        shoud_stop = true;
    }
    conn = driver->connect("rds432w5u5d17qd62iq3o.mysql.rds.aliyuncs.com:3306", "terark_benchmark", string(passwd));

    if(conn != nullptr && conn->isValid()) {
        conn->setSchema("benchmark");
        sql::PreparedStatement* pstmt = conn->prepareStatement("DELETE FROM engine_test_ops_10s WHERE time_bucket = 0");
        pstmt->executeUpdate();
        std::cout<<"database connected!"<<std::endl;
        delete pstmt;
    }else{
        std::cout<<"database failed"<<std::endl;
    }
}

AnalysisWorker::~AnalysisWorker() {
    delete conn;
}

void AnalysisWorker::stop() {
    shoud_stop = true;
}

void AnalysisWorker::run() {
    printf("Analysis worker is running ... \n");
    std::pair<uint64_t, uint64_t> read_result, insert_result, update_result;
    assert(conn != NULL);
    TimeBucket read_bucket(conn);
    TimeBucket insert_bucket(conn);
    TimeBucket update_bucket(conn);

    while(true) {
        bool b1 = Stats::readTimeDataCq.try_pop(read_result);
        bool b2 = Stats::createTimeDataCq.try_pop(insert_result);
        bool b3 = Stats::updateTimeDataCq.try_pop(update_result);

        if(b1){
            read_bucket.add(read_result.first, read_result.second, 1);
        }
        if(b2){
            insert_bucket.add(insert_result.first, insert_result.second, 2);
        }
        if(b3){
            update_bucket.add(update_result.first, update_result.second, 3);
        }
        if(!b1 && !b2 && !b3){
            if(shoud_stop){
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }
}