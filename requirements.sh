cd /tmp
git clone https://github.com/benmcollins/libjwt.git
git clone https://github.com/akheron/jansson.git
cd jansson
mkdir build
cd build
cmake ..
make
sudo make install
cd ../../libjwt
autoreconf -i
./configure
make
sudo make install
