========================================================================
Transactional Memory + Concurrency Control in Key-Value Store
========================================================================
Evaluation of HTM support for concurrency control in key-value stores.

Setup
========================================================================

(0) Install autoconf and libtool
On Ubuntu : sudo apt-get install autoconf libtool 

(1) Bootstrap, configure and build
./bootstrap

./configure 
To enable DEBUG mode :
./configure CXXFLAGS="-DDEBUG"

make

(2) Test

Test framework :
./tester/main
Test hashtable :
./hashtable/test

