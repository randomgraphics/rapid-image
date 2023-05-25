#/bin/bash
set -e # exit on error

echo "This script is to install native build dependencies on Ubuntu 20.04/22.04 LTS for non-docker based build."
echo
echo "Docker support is on its way."
echo
echo "If you wish to continue, press ENTER. Or else, press CTRL-C to quit."
read

# add vulkan sdk source
# sudo apt-get update
# sudo apt-get install -y wget gnupg2

# check OS distribution
release=$(lsb_release -rs)
if [ $(echo "$release >= 22" | bc) -eq 1 ]; then
    dist="ubuntu_22_04"
elif [ $(echo "$release >= 20" | bc) -eq 1 ]; then
    dist="ubuntu_20_04"
else
    echo "[ERROR] Unrecognized OS version ..."
    exit -1
fi
echo dist=$dist

# Install clang-14 for Ubuntu 20.04.
if [ "ubuntu_20_04" == $dist ]; then
    echo "Add clang 14 suite to apt registry for Ubuntu 20.04 ..."
    wget -P /tmp https://apt.llvm.org/llvm.sh
    chmod +x /tmp/llvm.sh
    sudo /tmp/llvm.sh 14
    rm /tmp/llvm.sh
    sudo apt install clang-14 clang-format-14
fi

# install build dependencies 
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    git-lfs \
    ninja-build

# install python modules
python3 -m pip install --upgrade pip
python3 -m pip install --upgrade termcolor