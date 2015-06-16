#!/bin/sh

# 数据块1024个字节，含256个inode
./fsformat fs.img 1024 256

echo > cmdlist;

i=100
while [ $i -lt 400 ]
do
	echo "create $i" >> cmdlist
	echo "ls" >> cmdlist
	i=$(($i+1))
done

./terminal -input cmdlist > output 2>&1
