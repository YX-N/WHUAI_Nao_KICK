#!/bin/bash


echo "press number key to choice"
echo "-------------1.save copy------------"
echo "--------2.load calibration---------"
echo "--------3.download calibration---------"
echo "----------4.save default-----------"
echo "---------5.change default----------"
echo "标定完成后，3获取机器人标定信息，1备份标定信息，2备份的标定信息上传"

read number

if [ $number -eq 1 ];then
echo "input field number"
read num1
mkdir -p calibration/$num1
cp -r Config/Robots/ calibration/$num1

elif [ $number -eq 2 ];then
echo "input field number"
read num1
rm -rf Config/Robots
cp -rf calibration/$num1/Robots Config/

elif [ $number -eq 3 ];then
echo "input robot number"
read num
echo "the password you need is 'nao'(only these three English letters)"
if [ $num -eq 1 ];then
scp -r nao@192.168.64.101:Config/Robots/one/Body/ ./Config/Robots/one
scp -r nao@192.168.64.101:Config/Robots/one/one/ ./Config/Robots/one

elif [ $num -eq 2 ];then
scp -r nao@192.168.5.102:Config/Robots/two/Body/ ./Config/Robots/two
scp -r nao@192.168.5.102:Config/Robots/two/two/ ./Config/Robots/two

elif [ $num -eq 3 ];then
scp -r nao@192.168.64.103:Config/Robots/three/Body/ ./Config/Robots/three
scp -r nao@192.168.64.103:Config/Robots/three/three/ ./Config/Robots/three

elif [ $num -eq 4 ];then
scp -r nao@192.168.64.104:Config/Robots/four/Body/ ./Config/Robots/four
scp -r nao@192.168.64.104:Config/Robots/four/four/ ./Config/Robots/four

elif [ $num -eq 5 ];then
scp -r nao@192.168.5.105:Config/Robots/five/Body/ ./Config/Robots/five
scp -r nao@192.168.5.105:Config/Robots/five/five/ ./Config/Robots/five

elif [ $num -eq 6 ];then
scp -r nao@192.168.64.106:Config/Robots/six/Body/ ./Config/Robots/six
scp -r nao@192.168.64.106:Config/Robots/six/six/ ./Config/Robots/six

elif [ $num -eq 7 ];then
scp -r nao@192.168.64.107:Config/Robots/seven/Body/ ./Config/Robots/seven
scp -r nao@192.168.64.107:Config/Robots/seven/seven/ ./Config/Robots/seven

elif [ $num -eq 8 ];then
scp -r nao@192.168.64.108:Config/Robots/eight/Body/ ./Config/Robots/eight
scp -r nao@192.168.64.108:Config/Robots/eight/eight/ ./Config/Robots/eight

elif [ $num -eq 9 ];then
scp -r nao@192.168.64.109:Config/Robots/nine/Body/ ./Config/Robots/nine
scp -r nao@192.168.64.109:Config/Robots/nine/nine/ ./Config/Robots/nine

elif [ $num -eq 10 ];then
scp -r nao@192.168.5.110:Config/Robots/ten/Body/ ./Config/Robots/ten
scp -r nao@192.168.5.110:Config/Robots/ten/ten/ ./Config/Robots/ten

elif [ $num -eq 11 ];then
scp -r nao@192.168.64.111:Config/Robots/eleven/Body/ ./Config/Robots/eleven
scp -r nao@192.168.64.111:Config/Robots/eleven/eleven/ ./Config/Robots/eleven

elif [ $num -eq 12 ];then
scp -r nao@192.168.64.112:Config/Robots/twelve/Body/ ./Config/Robots/twelve
scp -r nao@192.168.64.112:Config/Robots/twelve/twelve/ ./Config/Robots/twelve

elif [ $num -eq 13 ];then
scp -r nao@192.168.64.113:Config/Robots/thirteen/Body/ ./Config/Robots/thirteen
scp -r nao@192.168.64.113:Config/Robots/thirteen/thirteen/ ./Config/Robots/thirteen

elif [ $num -eq 14 ];then
scp -r nao@192.168.64.114:Config/Robots/fourteen/Body/ ./Config/Robots/fourteen
scp -r nao@192.168.64.114:Config/Robots/fourteen/fourteen/ ./Config/Robots/fourteen

elif [ $num -eq 15 ];then
scp -r nao@192.168.64.115:Config/Robots/fifteen/Body/ ./Config/Robots/fifteen
scp -r nao@192.168.64.115:Config/Robots/fifteen/fifteen/ ./Config/Robots/fifteen

elif [ $num -eq 16 ];then
scp -r nao@192.168.64.116:Config/Robots/sixteen/Body/ ./Config/Robots/sixteen
scp -r nao@192.168.64.116:Config/Robots/sixteen/sixteen/ ./Config/Robots/sixteen
fi

elif [ $number -eq 4 ];then
echo "input robot number"
read num2
mkdir -p all_defaults/$num2
cp -r Config/Robots/Default all_defaults/$num2/

elif [ $number -eq 5 ];then
echo "input robot number"
read num2
rm -rf Config/Robots/Default
cp -rf all_defaults/$num2/Default Config/Robots

fi

