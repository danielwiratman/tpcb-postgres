#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <libpq-fe.h>

#define DSN "host=localhost port=5432 dbname=postgres user=daniel"

int num_fields;

long tps = 1;
long nbranches = 1;
long ntellers = 10;
long naccounts = 10000;
long i, sqlcode, Bid, Tid, Aid, delta, Abalance;

int running = 1;

long totalCount = 0;

unsigned char error_exist;

// RANDOM NUMBER GENERATOR

static uint64_t s[2];

void xoroshiro128plus_init(uint64_t seed)
{
    s[0] = seed;
    s[1] = seed + 0x9E3779B97F4A7C15;
}

uint64_t xoroshiro128plus_next(void)
{
    const uint64_t s0 = s[0];
    uint64_t s1 = s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    s[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
    s[1] = (s1 << 36) | (s1 >> 28);

    return result;
}

int rand_range(int min, int max)
{
    const uint64_t random = xoroshiro128plus_next();
    return (random % (max - min + 1)) + min;
}

uint64_t get_random_seed() { return (uint64_t)time(NULL); }

// ===

void init_database(int scaling_factor)
{
    nbranches = scaling_factor * nbranches;
    ntellers = scaling_factor * ntellers;
    naccounts = scaling_factor * naccounts;

    PGconn *conn;
    conn = PQconnectdb(DSN);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Failed to connect : %s\n", PQerrorMessage(conn));
        exit(1);
    }
    PGresult *res = PQexec(conn, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Transaction begin failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }
    error_exist = 'F';

    fprintf(stderr, "Dropping tables if exists...");
    res = PQexec(conn, "DROP TABLE IF EXISTS idnameage;");
    fprintf(stderr, "DONE\n");

    fprintf(stderr, "Creating tables...");
    res = PQexec(conn, "CREATE TABLE idnameage(id int primary key, name "
                       "eqpg_encrypted, age int);");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query execution failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }
    fprintf(stderr, "DONE\n");

    fprintf(stderr, "Inserting %ld accounts...",
            nbranches, ntellers, naccounts);
    for (i = 0; i < nbranches * tps; i++)
    {
        char *query;
        query = malloc(sizeof(char) * 100);
        sprintf(query, "INSERT INTO idnameage(id, name, age) VALUES (%ld, 'Paijo', 0)",
                i + 1);
        res = PQexec(conn, query);
    }
    fprintf(stderr, "DONE\n");
    res =
        (error_exist == 'T') ? PQexec(conn, "ROLLBACK") : PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query execution failed: %s", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }
}

long do_one(PGconn *myconn, long Bid, long Tid, long Aid, long delta)
{
    error_exist = 'F';

    char *query;
    query = malloc(sizeof(char) * 1000);

    PGresult *myres = PQexec(myconn, "BEGIN");
    if (PQresultStatus(myres) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query 1 execution failed: %s", PQerrorMessage(myconn));
        PQclear(myres);
        PQfinish(myconn);
        exit(1);
    }
    sprintf(query, "UPDATE idnameage SET age=%ld WHERE id=%ld",
            Tid, Bid);
    myres = PQexec(myconn, query);
    if (PQresultStatus(myres) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query 2 execution failed: %s", PQerrorMessage(myconn));
        PQclear(myres);
        PQfinish(myconn);
        exit(1);
    }
    myres =
        (error_exist == 'T') ? PQexec(myconn, "ROLLBACK") : PQexec(myconn, "COMMIT");
    if (PQresultStatus(myres) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "Query 7 execution failed: %s", PQerrorMessage(myconn));
        PQclear(myres);
        PQfinish(myconn);
        exit(1);
    }

    return Abalance;
}

// pthread_mutex_t mtx;

void *benchmark_database(void *arg)
{
    int duration = *((int *)arg);
    int max_bid = *((int *)arg + 1);
    int max_tid = *((int *)arg + 2);
    int max_aid = *((int *)arg + 3);

    PGconn *conn;
    conn = PQconnectdb(DSN);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Failed to connect : %s\n", PQerrorMessage(conn));
        exit(1);
    }

    while (running)
    {
        int rand_bid = rand_range(1, max_bid);
        int rand_tid = rand_range(1, max_tid);
        int rand_aid = rand_range(1, max_aid);
        int rand_delta = rand_range(-5000, 5000);

        // pthread_mutex_lock(&mtx);
        // seq_branchId++;
        // seq_branchId %= max_bid;
        // if (seq_branchId == 0)
        //     seq_branchId++;

        // pthread_mutex_unlock(&mtx);

        do_one(conn, rand_bid, rand_tid, rand_aid, rand_delta);
        totalCount++;
    }
    return 0;
}

int main(int argc, char **argv)
{

    int opt;
    int init_mode = 0;
    int duration = 10;
    int concurrent_users = 1;
    int scaling_factor = 1;
    pthread_t thread;

    char *h_flag = "";
    char *P_flag = "";
    char *u_flag = "";
    char *p_flag = "";
    char *T_flag = "";
    char *c_flag = "";
    char *s_flag = "";

    while ((opt = getopt(argc, argv, ":ih:u:p:P:T:c:s:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            init_mode = 1;
            break;
        case 'h':
            h_flag = optarg;
            break;
        case 'P':
            P_flag = optarg;
            break;
        case 'u':
            u_flag = optarg;
            break;
        case 'p':
            p_flag = optarg;
            break;
        case 'T':
            T_flag = optarg;
            break;
        case 'c':
            c_flag = optarg;
            break;
        case 's':
            s_flag = optarg;
            break;
        }
    }

    if (strcmp(T_flag, ""))
    {
        duration = atoi(T_flag);
    }

    if (strcmp(c_flag, ""))
    {
        concurrent_users = atoi(c_flag);
    }

    if (strcmp(s_flag, ""))
    {
        scaling_factor = atoi(s_flag);
    }

    if (init_mode)
    {
        fprintf(stderr, "Init mode\n");
        init_database(scaling_factor);
    }
    else
    {
        fprintf(stderr, "Run mode\n");

        xoroshiro128plus_init(get_random_seed());
        // pthread_mutex_init(&mtx, NULL);
        pthread_t th[concurrent_users];

        int params[4] = {duration, scaling_factor, scaling_factor * 10,
                         scaling_factor * 10000};
        for (int i = 0; i < concurrent_users; i++)
        {
            if (pthread_create(th + i, NULL, &benchmark_database,
                               (void *)params) != 0)
            {
                perror("pthread_create");
                return 1;
            }
        }

        sleep(duration);

        // Terminate all threads
        running = 0;

        // Make sure all threads are terminated successfully
        for (int i = 0; i < concurrent_users; i++)
        {
            if (pthread_join(th[i], NULL) != 0)
            {
                perror("pthread_join");
                return 1;
            }
        }

        // pthread_mutex_destroy(&mtx);

        printf("\nTotal Count: %ld transactions in %d seconds..\n", totalCount,
               duration);
        printf("\nAverage TPS: %.2f\n", (float)totalCount / duration);
    }
}
