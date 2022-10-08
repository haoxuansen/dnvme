#!/bin/bash
#maxio hc-vd update @2021.7.20
#Date:2019-05-27
DEV=nvme0n1
runtime=1 #1800 #test runing time, uint is min
ramp=30

Host_info_collection()
{
	echo "Host Model/SN Indormation" 
	echo "---Host Model/SN Indormation---------------------------------------------" >> host_info.log
	dmidecode | grep "System Information" -A9 | egrep "Manufacturer|Product|Serial" >> host_info.log
	sleep 1

	echo "Host OS kernel" 
	echo "---Host OS kernel--------------------------------------------------------" >> host_info.log
	cat /etc/redhat-release >> host_info.log
	uname -a >> host_info.log
	sleep 1

	echo "Host CPU Information" 
	echo "---Host CPU Information--------------------------------------------------" >> host_info.log
	lscpu >> host_info.log
	sleep 1

	echo "Host DIMM Indormation" 
	echo "---Host DIMM Indormation--------------------------------------------------" >> host_info.log
	dmidecode -t memory >> host_info.log
	sleep 1

	echo "Host OS dmesg" 
	echo "---Host OS dmesg--------------------------------------------------" >> host_info.log
	dmesg >> host_info.log
	sleep 1

}


Test()
{
	rm -rf test_log/
	mkdir test_log
	#**************************Script of precondition*************************#

	# echo -e "\033[32m $DEV precondition seq write ready \033[0m"
	# date
	# fio --ioengine=libaio --direct=1 --thread --norandommap --filename=/dev/"$DEV" --name="$DEV"_init_seq --output="$DEV"_init_seq.log \
	#	--rw=write --bs=8M --numjobs=1 --iodepth=256 --loops=1 --output=Precondition_Seq.log --write_lat_log=seqwrite_QD256.log
	# date

	#for WL3 in randwrite
	#do
	#fio --output=prep${WL3}4k_Num1QD32.log --name=myjob --filename=/dev/"$DEV" --ioengine=libaio --direct=1 --norandommap --randrepeat=0 \
	#	--loops=2  --size=100% --blocksize=4k --rw=${WL3} --iodepth=64 --numjobs=4 --group_reporting --log_avg_msec=1000 \
	#	--write_bw_log=prep${WL3}4k_Num1QD32 --write_iops_log=prep${WL3}4k_Num1QD32 --write_lat_log=prep${WL3}4k_Num1QD32 --output=prep${WL3}4k_Num1QD32.log
	#done

	# for block_size in 1024k 512k 256k 128k 64k 32k 16k 8k 4k;
	# for mix_com in 30 40 50 60 70 80 90 100;

	echo "SSD mixed Performance Test"
	for rwtype in randrw;
	do
		for block_size in 128k;
		do
			for mix_com in 0;
			do
			name=pft6_${rwtype}_${block_size}_${mix_com}
			# dmesg >"$DEV"_${name}_dmesg.log  --verify=md5  
			# --log_avg_msec=1000  --write_iops_log=${name}_iops.log  --write_bw_log=${name}_bw.log  --write_lat_log=${name}_lat.log
			echo -e "\033[32m $name \033[0m"
			fio  --name=${name}  --rw=${rwtype}  --direct=1  --norandommap  --ioengine=libaio --numjobs=8  --rwmixread=${mix_com}  \
				--filename=/dev/"$DEV"  --iodepth=128 --bs=${block_size}  --group_reporting  --randrepeat=0  --thread  --runtime="$runtime"  \
				--time_based  --ramp_time="$ramp"  --output=${name}.log
			done
		done
	done

	echo "-----------------------------------------------------------" >> test_dmesg.log
	echo "dmesg"  >> test_dmesg.log
	dmesg  >> test_dmesg.log
	echo "-----------------------------------------------------------" >> test_dmesg.log
	echo "test done time"
	echo "test done time" >> test_dmesg.log
	date >> test_dmesg.log
	echo "-----------------------------------------------------------" >> test_dmesg.log

	mv *.log  test_log

}

Host_info_collection
Test
