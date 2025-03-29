#!/bin/bash


echo "press number key to choice"

echo "-------------1.save load calibration-------"
echo "-------------2.download calibration--------"

echo "Tips1: After calibration，press 1(need robot number and the field number),there will be folder named'calibration' in the ROOT"
echo "Tips2: Before calibration,press 2(need the field number),then all robots'calibration data will update"

declare -A number_to_word=(
    [0]="zero"
    [1]="one"
    [2]="two"
    [3]="three"
    [4]="four"
    [5]="five"
    [6]="six"
    [7]="seven"
    [8]="eight"
    [9]="nine"
    [10]="ten"
    [11]="eleven"
    [12]="twelve"
    [13]="thirteen"
    [14]="fourteen"
    [15]="fifteen"
    [16]="sixteen"
    [17]="seventeen"
    [18]="eighteen"
    [19]="nineteen"
    [20]="twenty"
)

read -p "Enter a number: " number

            
if [ $number -eq 1 ];then
read -p "Enter robot number(0-20): " robotnumber
read -p "Enter field number(A/B): " fieldnumber
robotnumber_word=${number_to_word[$robotnumber]}
robotnumber=$((robotnumber + 100))
./Make/Common/downloadCalibration 10.0.64.$robotnumber
source_dir="./Config/Robots/$robotnumber_word"
target_dir="./calibration/$fieldnumber"
if [ ! -d "$source_dir" ]; then
    echo "Source directory $source_dir does not exist."
    exit 1
fi
if [ ! -d "$target_dir" ]; then
    echo "Target directory $target_dir does not exist. Creating it..."
    mkdir -p "$target_dir"
fi
cp -r "$source_dir" "$target_dir"
echo "Directory $source_dir has been copied to $target_dir"

elif [ $number -eq 2 ]; then
read -p "Enter field number(A/B): " fieldnumber
source_dir="./calibration/$fieldnumber"
target_dir="./Config/Robots"
if [ ! -d "$source_dir" ]; then
    echo "Source directory $source_dir does not exist."
    exit 1
fi
if [ ! -d "$target_dir" ]; then
    echo "Target directory $target_dir does not exist. Creating it..."
    mkdir -p "$target_dir"
fi
for subdir in "$source_dir"/*/; do
    if [ -d "$subdir" ]; then
        subdir_name=$(basename "$subdir")
        target_subdir="$target_dir/$subdir_name"
        cp -r "$subdir" "$target_dir"
        echo "Copied $subdir to $target_dir"
    fi
done
echo "All subdirectories from $source_dir have been copied to $target_dir"

fi

