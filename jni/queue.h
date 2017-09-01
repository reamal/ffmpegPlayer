typedef struct _Queue Queue;

typedef void* (*queue_free_func)(void* elem);
//分配队列元素内存的函数
typedef void* (*queue_fill_func)();
/**
 * 初始化队列
 */
Queue* queue_init(int size,queue_fill_func fill_func);

/**
 * 销毁队列
 */
void queue_free(Queue* queue,queue_free_func free_func);

/**
 * 获取下一个索引的位置
 */
int queue_get_next(Queue* queue,int current);

/**
 * 压元素入队
 */
void* queue_push(Queue* queue);

/**
 * 弹出元素
 */
void* queue_pop(Queue* queue);

/*
 * 当前队列中的包数量
 */
int get_ready(Queue* queue);

/*
 * 队列大小
 */
int get_size(Queue* queue);
