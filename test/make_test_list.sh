set -e

# compile the list
cd ../src
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o list.o list.c
gcc -mcx16 -fPIC -shared -o lockfree.so list.o -lm -lpthread
cp lockfree.so liblockfree.so

cd ../test
# compile the test
cc -mcx16 -O3 -Wall -march=native -msse4.2 -D_GNU_SOURCE -L ../src -std=c99 -I ../src -L ../test/lockfree.so -c -o test_list.o test_list.c
gcc -mcx16 -L ../src -o test_list test_list.o -lm -lpthread -llockfree -latomic -I ../src

# run
export LD_LIBRARY_PATH=../src
#gdb -ex run --args ./test_list
./test_list
