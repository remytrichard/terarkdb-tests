cmake_minimum_required(VERSION 2.8)

#cmake -DCMAKE_VERBOSE_MAKEFILE=ON

project(Terark_Engine_Test)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++1y -g3 -O2 -DNDEBUG")
SET(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

aux_source_directory(./ SRC_LIST)

include_directories(./)
include_directories(./include)
include_directories(/usr/local/include)
include_directories(/opt/include)
include_directories(${TERARKDB_PATH}/include)
include_directories(${TERARKDB_ROCKS}/include)
include_directories(/usr/include/mysql)

link_directories(${TERARKDB_PATH}/lib)
#link_directories(${TERARKDB_ROCKS}/lib)
link_directories(/usr/local/mysql/lib)
link_directories(/usr/lib64/mysql/)
link_directories(/opt/lib)
link_directories(/opt/lib64)
link_directories(/usr/local/lib)

set(EXECUTABLE_OUTPUT_PATH ../../build)

set(COMMON_LIBS
	bench_common
   	boost_system
   	boost_filesystem
	mysqlclient tbb bz2 z pthread)

add_library(bench_common SHARED
	${SRC_LIST}
	db/builder.cc db/c.cc db/dbformat.cc db/db_impl.cc
	db/db_iter.cc db/filename.cc db/log_reader.cc db/log_writer.cc db/memtable.cc db/repair.cc
	db/table_cache.cc db/version_edit.cc db/version_set.cc db/write_batch.cc table/block_builder.cc
	table/block.cc table/filter_block.cc table/format.cc table/iterator.cc table/merger.cc
	table/table_builder.cc table/table.cc table/two_level_iterator.cc util/arena.cc util/bloom.cc
	util/cache.cc util/coding.cc util/comparator.cc util/crc32c.cc util/env.cc util/env_posix.cc
	util/filter_policy.cc util/hash.cc util/histogram.cc util/logging.cc util/options.cc util/status.cc
	port/port_posix.cc ./util/testutil.cc src/Setting.cpp src/Setting.h
	src/TcpServer.cpp src/TcpServer.h
	src/Stats.cpp src/Stats.h
	src/Benchmark.cpp src/Benchmark.h
	src/analysis_worker.cpp util/system_resource.h
	src/ThreadState.cpp src/ThreadState.h
	main/main_impl.cpp
	)

add_executable(bench_terarkdb
		  main/bench_terarkdb.cpp
		  src/RocksDbBenchmark.cpp src/RocksDbBenchmark.h
		  src/TerocksBenchmark.cpp src/TerocksBenchmark.h
		  )
target_link_libraries(bench_terarkdb
		  -Wl,--whole-archive terark-zip-rocksdb-r -Wl,--no-whole-archive
		  terark-zbs-r
		  terark-fsa-r
		  terark-core-r
		  rocksdb
		  snappy
		  ${COMMON_LIBS}
		  crypto
		  )

add_executable(tcp_client
		main/tcp_client.cpp)
target_link_libraries(tcp_client
		boost_system
		pthread)

add_executable(bench_rocksdb
		  main/bench_rocksdb.cpp
		  src/RocksDbBenchmark.cpp src/RocksDbBenchmark.h
		  )
target_link_libraries(bench_rocksdb
		  rocksdb
		  terark-core-r
		  snappy
		  ${COMMON_LIBS}
		  )

#add_executable(bench_terichdb
#		  main/bench_terichdb.cpp
#		  src/TerichBenchmark.cpp src/TerichBenchmark.h
#		  )
#target_link_libraries(bench_terichdb
#		  terichdb-r
#		  terark-zbs-r
#		  terark-fsa-r
#		  terark-core-r
#		  ${COMMON_LIBS}
#		  )

add_executable(bench_wiredtiger
		  main/bench_wiredtiger.cpp
		  src/WiredTigerBenchmark.cpp src/WiredTigerBenchmark.h
		  )
target_link_libraries(bench_wiredtiger
		  terark-core-r
		  wiredtiger
		  snappy
		  ${COMMON_LIBS}
		  )

add_executable(bench_posix
		  main/bench_posix.cpp
		  src/PosixBenchmark.cpp src/PosixBenchmark.h
		  )
target_link_libraries(bench_posix
		  terark-core-r
		  ${COMMON_LIBS}
		  )

