# scsi_test

SCSI Testing 

[![Build Status](https://travis-ci.org/nttlman23/scsi_test.svg?branch=main)](https://travis-ci.org/nttlman23/scsi_test)

### Build
```bash
$ mkdir build
$ cd build
$ cmake ../
$ cmake --build . # or make
```

### Usage
```bash
scsi_test [-l LBAVAL] [-b BLOCKS] [-m MAXBLOCKS [-c COUNT]] [-s] [-v] -d DEVICE 
```
  -l, --lba VAL - logical block address  
  -b, --blocks VAL - number of blocks  
  -m, --maxblocks VAL - max number of blocks  
  -c, --count VAL - test count  
  -v, --verbose - verbose execution  
  -t, --table - show partition table  
  -i, --inquiry - test inquiry  
  -w, --write - test write  
  -r, --read - test read  
  -d, --device - device node (/dev/sgX or /dev/sdXX)  
  -h, --help - print this message  
