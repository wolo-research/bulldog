#REQUIRED: ubuntu 20.04
#git clone https://gitlab.com/woloai/bulldog.git
#install cmake https://www.matbra.com/2017/12/07/install-cmake-on-aws-linux.html
sudo snap install cmake --classic
y
#install the g++9 https://linuxconfig.org/how-to-switch-between-multiple-gcc-and-g-compiler-versions-on-ubuntu-20-04-lts-focal-fossa
sudo apt-get update
sudo apt-get install build-essential
#install libcpprest https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Linux
sudo apt-get install libcpprest-dev
y

git clone https://gitlab.com/woloai/bulldog.git
cd bulldog
git submodule init
git submodule update
#install libtoch
#cd 3rdparty
#sudo apt install unzip
#wget https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip
#unzip libtorch-shared-with-deps-latest.zip
#cd ..

#build
mkdir build
cd build/
cmake ..

#
sudo apt install python3-pip
pip install jupyterlab

