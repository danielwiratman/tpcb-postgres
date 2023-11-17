NUM_THREAD=12 # Bisa cek di htop atau proc, ada berapa thread
NUM_SEC=15 # Accepted value for true benchmark is usually 300-600

make

# Initializing tables
./test_postgres -i -s $NUM_THREAD

# Doing the test
./test_postgres -s $NUM_THREAD -c $NUM_THREAD -T $NUM_SEC
