NUM_THREAD=12
NUM_SEC=30

./test_postgres -i -s $NUM_THREAD

./test_postgres -s $NUM_THREAD -c $NUM_THREAD -T $NUM_THREAD