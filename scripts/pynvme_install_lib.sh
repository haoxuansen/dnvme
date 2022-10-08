sudo apt-get install cmake libcunit1-ncurses-dev


https://github.com/openchannelssd/liblightnvm


http://lightnvm.io/liblightnvm/quick_start/index.html

make configure
make 
sudo make install

nvme_cmd idfy /dev/nvme0n1
nvm_dev info /dev/nvme0n1
nvm_cmd rprt_all /dev/nvme0n1
sector 3 within chunk 142,within chunk 142 within parallel unit 2.within parallel group 4 
nvm_addr s20_to_gen /dev/nvme0n1 4 2 142 3
nvm_addr gen2dev /dev/nvme0n1 0x0402008e0000003



https://github.com/pynvme/pynvme.git
