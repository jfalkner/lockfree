set -e

# compile the list
cd ../src
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o free_later.o free_later.c
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o list.o list.c
cc -ggdb -O3 -fPIC -Wall -march=native -msse4.2 -D_GNU_SOURCE -fPIC -shared -MD  -c -o hashmap.o hashmap.c
gcc -mcx16 -fPIC -shared -o lockfree.so hashmap.o list.o free_later.o -lm -lpthread
cp lockfree.so liblockfree.so

cd ../test
# compile the test
cc -mcx16 -O3 -Wall -march=native -msse4.2 -D_GNU_SOURCE -L ../src -std=c99 -I ../src -L ../test/lockfree.so -c -o test_hashmap.o test_hashmap.c
gcc -mcx16 -L ../src -o test_hashmap test_hashmap.o -lm -lpthread -llockfree -latomic -I ../src

# run
export LD_LIBRARY_PATH=../src
#gdb -ex run --args ./test_hashmap
./test_hashmap
