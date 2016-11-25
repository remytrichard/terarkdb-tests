//
// Created by terark on 9/5/16.
//

#include "RocksDbBenchmark.h"
#include <string>
#include <terark/fstring.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>

using namespace terark;

void RocksDbBenchmark::Open() {
    std::cout << "Create database " << setting.FLAGS_db << std::endl;
    rocksdb::Status s = rocksdb::DB::Open(options, setting.FLAGS_db, &db);
    if (!s.ok()) {
        fprintf(stderr, "open error: %s\n", s.ToString().c_str());
        exit(1);
    }
}

RocksDbBenchmark::RocksDbBenchmark(Setting &set) : Benchmark(set) {
    db = nullptr;
    options.create_if_missing = true;
// new features to add
    options.allow_concurrent_memtable_write = true;
    options.enable_write_thread_adaptive_yield = true;
    options.allow_mmap_reads = true;
    options.allow_mmap_writes = true;
    options.max_background_compactions = 2;
// end

    static const std::map<char, ullong> kmgtp = {
            {'k', 1024ull},
            {'K', 1024ull},
            {'m', 1024ull*1024},
            {'M', 1024ull*1024},
            {'g', 1024ull*1024*1024},
            {'G', 1024ull*1024*1024},
            {'t', 1024ull*1024*1024*1024},
            {'T', 1024ull*1024*1024*1024},
            {'p', 1024ull*1024*1024*1024*1024},
            {'P', 1024ull*1024*1024*1024*1024},
    };
    valvec<fstring> dircaps;
    fstring(setting.FLAGS_db).split(',', &dircaps);
    if (dircaps.size() > 1) {
        setting.dbdirs.clear();
        for (size_t i = 0; i < dircaps.size(); ++i) {
            fstring dircap = dircaps[i];
            const char* colon = dircap.strstr(":");
            if (colon) {
                char* suffix = NULL;
                double cap = strtof(colon+1, &suffix);
                auto iter = kmgtp.find(*suffix);
                if (kmgtp.end() != iter) {
                    cap *= iter->second;
                }
                std::string dir(dircap.data(), colon);
                options.db_paths.push_back({dir, uint64_t(cap)});
                setting.dbdirs.push_back(dir);
                fprintf(stderr, "RocksDB: add dbdir: cap=%6.1fG, %s\n", cap/(1ull<<30), dir.c_str());
            }
            else {
                fprintf(stderr, "ERROR: invalid dir:cap,...: %s\n", setting.FLAGS_db);
                exit(1);
            }
        }
        setting.FLAGS_db = "multi-dir-rocksdb";
    }
    if (setting.logdir.size()) {
        options.db_log_dir = setting.logdir;
        setting.dbdirs.push_back(setting.logdir);
    }
    if (setting.waldir.size()) {
        options.wal_dir = setting.waldir;
        setting.dbdirs.push_back(setting.waldir);
    }

    rocksdb::BlockBasedTableOptions block_based_options;
    block_based_options.index_type = rocksdb::BlockBasedTableOptions::kBinarySearch;
    block_based_options.block_cache = setting.FLAGS_cache_size >= 0 ?
                                      rocksdb::NewLRUCache(setting.FLAGS_cache_size) : NULL;
    block_based_options.block_size = set.FLAGS_block_size;

    filter_policy_.reset(set.FLAGS_bloom_bits >= 0 ?
                         rocksdb::NewBloomFilterPolicy(set.FLAGS_bloom_bits, false) : NULL);
    block_based_options.filter_policy = filter_policy_;
    options.table_factory.reset(NewBlockBasedTableFactory(block_based_options));
    write_options = rocksdb::WriteOptions();
    read_options = rocksdb::ReadOptions();

    options.compression = set.FLAGS_compression_type;
    if (set.FLAGS_min_level_to_compress >= 0) {
        assert(set.FLAGS_min_level_to_compress <= set.FLAGS_num_levels);
        options.compression_per_level.resize(set.FLAGS_num_levels);
        for (int i = 0; i < set.FLAGS_min_level_to_compress; i++) {
            options.compression_per_level[i] = rocksdb::kNoCompression;
        }
        for (int i = set.FLAGS_min_level_to_compress;
             i < set.FLAGS_num_levels; i++) {
            options.compression_per_level[i] = set.FLAGS_compression_type;
        }
    }
    for (int i = 0; i < options.compression_per_level.size(); i++) {
        printf("options.compression_per_level[%d]=%d\n", i, options.compression_per_level[i]);
    }
    options.write_buffer_size = options.write_buffer_size * 4;
//    options.write_buffer_size = 2*1024*1024;
}

void RocksDbBenchmark::Close() {
    delete db;
    db = NULL;
}

void RocksDbBenchmark::Load() {
    std::cout << "RocksDbBenchmark Load" << std::endl;
    Auto_fclose loadFile(fopen(setting.getLoadDataPath().c_str(), "r"));
    assert(loadFile != NULL);
//    posix_fadvise(fileno(loadFile), 0, 0, POSIX_FADV_SEQUENTIAL);
    LineBuf line;
    std::string key, value;
	size_t lines_num = 0;
    size_t bytes = 0, last_bytes = 0;
    profiling pf;
    long long t0 = pf.now();
    long long t1 = t0;
    while (line.getline(loadFile) > 0) {
        line.chomp();
        if (getKeyAndValue(line, key, value) == 0)
            continue;
        rocksdb::Status s = db->Put(write_options, key, value);
        if (!s.ok()) {
            fprintf(stderr, "put error: %s\n", s.ToString().c_str());
        }
        bytes += line.size();
        pushKey(key);
        lines_num++;
        const size_t statisticNum = 100000;
        if (lines_num % statisticNum == 0) {
            long long t2 = pf.now();
            printf("line:%zuw, bytes:%9.3fG, records/sec: { cur = %6.2fw  avg = %6.2fw }, bytes/sec: { cur = %6.2fM  avg = %6.2fM }\n"
                    , statisticNum / 10000, bytes/1e9
                    , lines_num*1e5/pf.ns(t1,t2), lines_num*1e5/pf.ns(t0,t2)
                    , (bytes - last_bytes)/pf.uf(t1,t2), bytes/pf.uf(t0,t2)
            );
            t1 = t2;
            last_bytes = bytes;
        }
    }
}

size_t RocksDbBenchmark::getKeyAndValue(fstring str, std::string &key, std::string &val) {
    thread_local valvec<fstring> strvec;
    strvec.erase_all();
    str.split(setting.fieldsDelim, &strvec);
    key.resize(0);
    val.resize(0);
    if (strvec.size() < setting.numFields)
        return 0;
    auto& kf = setting.keyFields;
    for (size_t field: kf) {
        key.append(strvec[field].data(), strvec[field].size());
        key.append(" ");
    }
    key.pop_back();
    for (size_t i = 0; i < strvec.size(); i++) {
        if (std::find(kf.begin(), kf.end(), i) != kf.end())
            continue;
        val.append(strvec[i].data(), strvec[i].size());
        val.append("\t");
    }
    val.pop_back();
    return strvec.size();
}

bool RocksDbBenchmark::ReadOneKey(ThreadState *ts) {
    if (false == getRandomKey(ts->key, ts->randGenerator))
        return false;
    if (false == db->Get(read_options, ts->key, &(ts->value)).ok())
        return false;
    return true;
}

bool RocksDbBenchmark::UpdateOneKey(ThreadState *ts) {
    if (false == getRandomKey(ts->key, ts->randGenerator)) {
     	fprintf(stderr,"RocksDbBenchmark::UpdateOneKey:getRandomKey false\n");
	    return false;
    }
    if (false == db->Get(read_options, ts->key, &(ts->value)).ok()) {
    // 	fprintf(stderr,"RocksDbBenchmark::UpdateOneKey:db-Get false, value.size:%05zd, key:%s\n"
    //            , ts->value.size(), ts->key.c_str());
	    return false;
    }
    auto status = db->Put(write_options, ts->key, ts->value);
    if (false == status.ok()){
     	fprintf(stderr,"RocksDbBenchmark::UpdateOneKey:db-Put false\n key:%s\nvalue:%s\n",ts->key.c_str(),ts->value.c_str());
    	fprintf(stderr,"RocksDbBenchmakr::UpdateOneKey:db-Put status:%s\n",status.ToString().c_str());
	    return false;
    }
    fflush(stderr);
    return true;
}

bool RocksDbBenchmark::InsertOneKey(ThreadState *ts) {
    if (updateDataCq.try_pop(ts->str) == false){
	    return false;
    }
    auto ret = getKeyAndValue(ts->str, ts->key, ts->value);
    if (ret == 0){
     	fprintf(stderr,"RocksDbBenchmark::InsertOneKey:getKeyAndValue false\n");
	    return false;
    }
    auto status = db->Put(write_options, ts->key, ts->value);
    if (false == status.ok()){
     	fprintf(stderr,"RocksDbBenchmark::InsertOneKey:db->Put false.\nkey:%s\nvalue:%s\n",ts->key.c_str(),ts->value.c_str());
    	fprintf(stderr,"RocksDbBenchmakr::InsertOneKey:db-Put status:%s\n",status.ToString().c_str());
	    updateDataCq.push(ts->str);
      	return false;
    }
    pushKey(ts->key);
    return true;
}

bool RocksDbBenchmark::Compact(void) {
    db->CompactRange(NULL, NULL);
    return false;
}

ThreadState *RocksDbBenchmark::newThreadState(std::atomic<std::vector<bool > *> *whichSamplingPlan) {
    return new ThreadState(threads.size(), nullptr, whichSamplingPlan);
}
