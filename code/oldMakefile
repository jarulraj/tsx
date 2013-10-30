SRCDIR	= src
UTILDIR = util
TESTDIR = test
BINDIR  = bin
LIBDIR  = lib
CC 		= gcc
CFLAGS	= -Wall -Wextra -g -DDEBUG -DTEST
LFLAGS	= -lrt -L. -lhashtable

EXEC    = hashtable-test

all: hashtable-test 

util: $(UTILDIR)/murmur.c 
	$(CC) $(CFLAGS) -c $(UTILDIR)/murmur.c -o $(BINDIR)/murmur.o

hashtable-lib: util $(SRCDIR)/hashtable.c  
	$(CC) $(CFLAGS) $(SRCDIR)/hashtable.c $(UTILDIR)/murmur.c -fPIC -rdynamic -shared -o $(LIBDIR)/libhashtable.so

hashtable-static-lib: util $(SRCDIR)/hashtable.c  
	$(CC) $(CFLAGS) -I$(UTILDIR) -c $(SRCDIR)/hashtable.c -o $(BINDIR)/hashtable.o
	ar crf $(LIBDIR)/libhashtable.a $(BINDIR)/hashtable.o $(BINDIR)/murmur.o

hashtable-test: $(TESTDIR)/test.c hashtable-static-lib
	$(CC) -I$(SRCDIR) -I$(UTILDIR) -L$(LIBDIR) $(TESTDIR)/test.c $(LFLAGS) $(CFLAGS) -o $(BINDIR)/$(EXEC)

clean:
	rm -f $(BINDIR)/*.so
	rm -f $(LIBDIR)/*.a
	rm $(BINDIR)/$(EXEC)
