Transactional Memory + Concurrency Control in Key-Value Store
=============================================================

Evaluation of HTM support for concurrency control in key-value stores.

Setup
-----

1. Install autoconf and libtool :
   
   sudo apt-get install autoconf libtool 

2. Bootstrap, configure and build :

    ./bootstrap
    ./configure (OR) ./configure CXXFLAGS="-DDEBUG" (for DEBUG mode)
    make

3. Test DB.

    ./tester/main (for Usage)

