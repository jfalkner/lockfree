set -e

# compile the mempool
cd ../src
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o list.o list.c
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o mempool.o mempool.c
gcc -mcx16 -fPIC -shared -o hashmap.so mempool.o list.o -lm -lpthread
cp hashmap.so libhashmap.so

cd ../test
# compile the test
cc -mcx16 -O3 -Wall -march=native -msse4.2 -D_GNU_SOURCE -L ../src -std=c99 -I ../src -L ../test/hashmap.so -c -o test_mempool.o test_mempool.c
gcc -mcx16 -L ../src -o test_mempool test_mempool.o -lm -lpthread -lhashmap -latomic -I ../src

# run
export LD_LIBRARY_PATH=../src
#gdb -ex run --args ./test_list
./test_mempool
