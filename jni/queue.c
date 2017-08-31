#include "queue.h"

struct _Queue {
	int size; //队列长度

	void** tab; //任意指针数组。 AVPacket **packet

	int next_to_write;
	int next_to_read;

	int *ready;
};

/**
 * 初始化队列
 */
Queue* queue_init(int size, queue_fill_func fill_func) {
	Queue* queue = (Queue*) malloc(sizeof(Queue));
	queue->size = size;
	queue->next_to_read = 0;
	queue->next_to_write = 0;

	//数组开辟空间
	queue->tab = malloc(sizeof(*queue->tab) * size);

	int i = 0;
	for (; i < size; i++) {
		queue->tab[i] = fill_func();
	};
	return queue;
}
;

/**
 * 销毁队列
 */
void queue_free(Queue* queue, queue_free_func free_func) {
	int i = 0;
	for (; i < queue->size; i++) {
		free_func((void*) queue->tab[i]);
	}
	free(queue->tab);
	free(queue);
}
;

/**
 * 获取下一个索引的位置
 */
int queue_get_next(Queue* queue, int current){
	return (current+1)%queue->size;
};

/**
 * 压元素入队
 */
void* queue_push(Queue* queue){
	int current = queue->next_to_write;
	queue->next_to_write = queue_get_next(queue,current);
	return queue->tab[current];
};

/**
 * 弹出元素
 */
void* queue_pop(Queue* queue){
	int current = queue->next_to_read;
	queue->next_to_read= queue_get_next(queue,current);
	return queue->tab[current];
};
