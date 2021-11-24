sudo yum install cmake
sudo yum install boost
sudo yum install boost-devel
sudo yum install -y gcc libxml2-devel gcc-c++ make
sudo yum install git

#https://aws.amazon.com/premiumsupport/knowledge-center/ec2-enable-epel/
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
sudo yum -y install devtoolset-8
echo "source scl_source enable devtoolset-8 &> /dev/null" >> ~/.bashrc
source ~/.bashrc
cd ~

sudo yum -y install cmake3
mkdir -p ~/src && cd ~/src
git clone https://github.com/zaphoyd/websocketpp.git
cd websocketpp
mkdir build && cd build && cmake3 ..
make
sudo make install
cd ~

sudo yum -y install openssl-devel
mkdir -p ~/src && cd ~/src
git clone https://github.com/Microsoft/cpprestsdk.git
cd cpprestsdk
git reset --hard v2.10.7
git submodule update --init
mkdir Release/build.release && cd Release/build.release
m
make -j 8
sudo make install
sudo ldconfig /usr/local/lib /usr/local/lib64
cd ~

git clone https://carmenc@gitlab.com/woloai/bulldog.git

cd bulldog
git submodule init
git submodule --update --init

mkdir build
mkdir logs
cd build
cmake ..
make install

cd ../bin