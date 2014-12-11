#include "el_mutex.h"
#include "el_ipc.h"
#include "el_media_player_engine.h"

static el_ipc_msg_t el_ipc_msg_q;

// 创建几个进程间通讯的队列
EL_STATIC int el_ipc_create(void)
{
	EL_DBG_LOG("ipc: create called...\n");

	q_init(&el_ipc_msg_q.msg_q[el_msg_q_engine],el_q_mode_block_empty);
	q_init(&el_ipc_msg_q.msg_q[el_msg_q_decode_video],el_q_mode_block_empty);
	q_init(&el_ipc_msg_q.msg_q[el_msg_q_decode_audio],el_q_mode_block_empty);
	q_init(&el_ipc_msg_q.msg_q[el_msg_q_close],el_q_mode_block_empty);
	q_init(&el_ipc_msg_q.msg_q[el_msg_q_video_sync],el_q_mode_block_empty);

	return 0;
}

EL_STATIC void* el_ipc_get_msg_q(el_ipc_msg_q_name_e a_msgname)
{
	return (void*) &el_ipc_msg_q.msg_q[a_msgname];
}

EL_STATIC int el_ipc_wait(el_ipc_msg_q_name_e fd,int *msg, int64_t *tm)
{
	struct list_node *ptr = q_pop(&el_ipc_msg_q.msg_q[fd]);

	if(!ptr)
	{
		return -1;
	}
	else
	{
		el_ipc_msg_node_t *msg_node = (el_ipc_msg_node_t *)list_entry(ptr, el_ipc_msg_node_t, list);
		*msg = msg_node->msg;
		*tm = msg_node->tm;
		free(msg_node);
		return 1;
	}
}

EL_STATIC int el_ipc_send(int msg,el_ipc_msg_q_name_e peer_fd)
{
	el_ipc_msg_node_t *msg_node = malloc(sizeof(el_ipc_msg_node_t));

	if(!msg_node)
	{
		EL_DBG_LOG("ipc: alloc msg_node err!\n");

		return -1;
	}

	//allocate OK, add to ipc q
	msg_node->msg = msg;
	msg_node->tm = el_get_sys_time_ms();

	q_push(&msg_node->list, &el_ipc_msg_q.msg_q[peer_fd]);

	return 0;
}

EL_STATIC void el_ipc_clear(el_ipc_msg_q_name_e fd)
{
	el_ipc_msg_node_t *msg_node;
	struct list_node *poped_item;
	struct q_head *q_head = &el_ipc_msg_q.msg_q[fd];

	el_mutex_lock(q_head->mutex);

	//EL_DBG_LOG("clearing fd %d\n",fd);
	while(!list_empty(&q_head->lst_node))
	{
		poped_item = q_head->lst_node.next;
		list_del(poped_item);
		q_head->node_nr--;
		msg_node = (el_ipc_msg_node_t *)list_entry(poped_item, el_ipc_msg_node_t, list);
		EL_DBG_LOG("skipping msg %d\n",msg_node->msg);
		free(msg_node);
	}
	el_mutex_unlock(q_head->mutex);
	return;	
}


EL_STATIC void el_ipc_delete(void)
{
	int i;
	EL_DBG_LOG("ipc: destroy called...\n");
	for(i = 0; i < el_msg_q_max; i++)
	{
		q_destroy(&el_ipc_msg_q.msg_q[i]);
	}
}


