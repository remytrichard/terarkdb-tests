#!/usr/bin/env bash

if [ -z "$1" ]; then
	echo usage $0 MySQL_Password
	exit 1
fi
set -x
cd ..
cd shell
echo "####Now, running benchmark"
echo 3 > /proc/sys/vm/drop_caches
if [ -z "$BMI2" ]; then
	BMI2=1
fi
export LD_LIBRARY_PATH=/opt/${CXX}/lib64
if [ -d ../lib ]; then
	export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH
else
	export LD_LIBRARY_PATH=`cd ..; pwd`/pkg/terarkdb-tests-Linux-x86_64-${CXX}-bmi2-${BMI2}/lib:$LD_LIBRARY_PATH
fi
export DictZipBlobStore_zipThreads=12
./Terark_Engine_Test \
  rocksdb \
  --action=run \
  --keys_data_path=/disk2/tmp/lineitem.keys.0.06 \
  --insert_data_path=<(zcat /data/tpch_data/lineitem_512b.tbl.gz) \
  --db=/newssd1/terocksdb_tpch:400G,/newssd2/terocksdb_tpch:400G,/experiment/terocksdb_tpch:300G,/datainssd/terocksdb_tpch:300G \
  --logdir=/data/terocksdb_tpch.logdir \
  --waldir=/data/terocksdb_tpch.waldir \
  --fields_delim="|" \
  --fields_num=16 \
  --key_fields=0,1,2 \
  --key_sample_ratio=0.05 \
  --disable_wal \
  --num_levels=4 \
  --compact_threads=2 \
  --use_universal_compaction=0 \
  --plan_config=0:0:100:0 \
  --plan_config=1:100:0:0 \
  --thread_plan_map=0:0 \
  --thread_plan_map=1-24:1 \
  --thread_num=25 \
  --alt_engine_name=rocksdb_official \
  --mysql_passwd=$1

