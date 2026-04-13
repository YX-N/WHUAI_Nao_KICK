#!/bin/bash


echo "100 Robot setting"
echo "101 install bzip2"
echo "102 make .ext3(need .OPN) Tips:Make sure this opn in your folder"
echo "103 make bhuman.opn"
read number

if [ $number -eq 100 ];then
Install/createRobot -t 64 -r 101 -s P0000074A05S94S00007 -b P0000073A07S94J00052 one
Install/createRobot -t 64 -r 102 -s P0000074A05S94S00009 -b P0000073A07S94S00003 two
Install/createRobot -t 64 -r 112 -s P0000074A07S04Q00037 -b P0000073A12S04Q00031 twelve
Install/createRobot -t 64 -r 113 -s P0000074A07S04Q00052 -b P0000073A12S04Q00053 thirteen
Install/createRobot -t 64 -r 111 -s P0000074A07S04Q00058 -b P0000073A12S04Q00060 eleven
Install/createRobot -t 64 -r 108 -s P0000074A05S94S00013 -b P0000073A07S94J00053 eight
Install/createRobot -t 64 -r 107 -s P0000074A05S94S00015 -b P0000073A07S94S00024 seven
Install/createRobot -t 64 -r 109 -s P0000074A05S94S00014 -b P0000073A07S94J00056 nine
Install/createRobot -t 64 -r 106 -s P0000074A05S94S00011 -b P0000073A07S94S00009 six
Install/createRobot -t 64 -r 103 -s P0000074A05S94S00028 -b P0000073A07S94S00023 three
Install/createRobot -t 64 -r 110 -s P0000074A05S94S00017 -b P0000073A07S94S00020 ten
Install/createRobot -t 64 -r 100 -s P0000074A03S85V00005 -b P0000073A04S86E00004 zero
Install/createRobot -t 64 -r 104 -s P0000074A05S94J00059 -b P0000073A07S94S00006 four
Install/createRobot -t 64 -r 105 -s P0000074A05S94S00002 -b P0000073A07S94S00007 five
Install/createRobot -t 64 -r 114 -s P0000074A09S3AK00016 -b P0000073A19S3AK00016 fourteen
Install/createRobot -t 64 -r 115 -s P0000074A09S3AK00017 -b P0000073A19S3AK00017 fifteen
Install/createRobot -t 64 -r 116 -s P0000074A10S41M00033 -b P0000073A20S41M00033 sixteen
Install/createRobot -t 64 -r 117 -s P0000074A09S3AK00012 -b P0000073A19S3AK00013 seventeen
Install/createRobot -t 64 -r 130 -s P0000074A10S55C00014 -b P0000073A20S54L00044 thirty
Install/createRobot -t 64 -r 140 -s P0000074A10S54L00025 -b P0000073A20S54L00043 forty
Install/createRobot -t 64 -r 150 -s P0000074A10S54L00028 -b P0000073A20S54L00005 fifty
cd

elif [ $number -eq 101 ];then
sudo apt install bzip2 debootstrap patchelf
cd

elif [ $number -eq 102 ];then
sudo sudo Install/createRootImage ./nao-2.8.5.11_ROBOCUP_ONLY_with_root.opn 
cd

elif [ $number -eq 103 ];then
Make/Common/deploy -i -v 70 -w SPL_A
cd
fi



