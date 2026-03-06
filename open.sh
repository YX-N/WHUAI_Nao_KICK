#!/bin/bash


echo "press number key to choice"
echo "--------     1.generate             --------"
echo "--------     2.compile              --------"
echo "--------     3.compile --release    --------"
echo "--------     4.Deploy               --------"
echo "--------     5.SimRobot             --------"
echo "--------     6.GameController       --------"


read number

if [ $number -eq 1 ];then
NO_CLION=true Make/Linux/generate
cd;

elif [ $number -eq 2 ];then
Make/Linux/compile
cd;

elif [ $number -eq 3 ];then
Make/Linux/compile Release
cd;

elif [ $number -eq 4 ];then
cd Build/Linux/DeployDialog/Develop
./DeployDialog;

elif [ $number -eq 9 ];then
./A_deploy.sh

elif [ $number -eq 10 ];then
./B_deploy.sh

elif [ $number -eq 5 ];then
cd Build/Linux/SimRobot/Develop
./SimRobot;

elif [ $number -eq 6 ];then
cd ../
cd ./gamecontroller
./game_controller

fi

