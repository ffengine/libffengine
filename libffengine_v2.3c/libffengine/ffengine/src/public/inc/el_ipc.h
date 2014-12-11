

#ifndef _EL_IPC_H_
#define _EL_IPC_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/times.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "el_std.h"
#include "el_list.h"

// 进程间通讯的几个队列
// 这种设计很巧妙，用enum的最大数目，来作为下面数组的长度
typedef enum _el_ipc_msg_q_name_e_
{
	el_msg_q_engine = 0,
	el_msg_q_decode_video,
	el_msg_q_decode_audio,
	el_msg_q_close,
	el_msg_q_video_sync,
	el_msg_q_max
} el_ipc_msg_q_name_e;


typedef struct _el_ipc_msg_t_
{
	struct q_head msg_q[el_msg_q_max];
	pthread_cond_t has_node[el_msg_q_max];
} el_ipc_msg_t;

typedef struct _el_ipc_msg_node_t_
{
	unsigned long       msg;    // 消息内容
	int64_t             tm;     // 时间戳
	struct list_node    list;   // 消息队列
} el_ipc_msg_node_t;

// 
EL_STATIC void *el_ipc_get_msg_q(el_ipc_msg_q_name_e a_msgname);
EL_STATIC int el_ipc_create(void);
EL_STATIC int el_ipc_wait(el_ipc_msg_q_name_e fd, int *msg, int64_t *tm);
EL_STATIC int el_ipc_send(int msg, el_ipc_msg_q_name_e peer_fd);
EL_STATIC void el_ipc_clear(el_ipc_msg_q_name_e fd);
EL_STATIC void el_ipc_delete(void);

#endif //_EL_IPC_H_
