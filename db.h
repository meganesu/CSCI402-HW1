#include <pthread.h>

typedef struct Node {
	char *name;
	char *value;
	struct Node *lchild;
	struct Node *rchild;

        pthread_mutex_t request_mutex;
        int num_readers;
        pthread_mutex_t num_readers_mutex;
        pthread_mutex_t node_mutex;
} node_t;

extern node_t head;

void interpret_command(char *, char *, int);
