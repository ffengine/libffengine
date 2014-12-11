#ifndef __EL_FFMPEG_LIST_H__
#define __EL_FFMPEG_LIST_H__

typedef struct list_node {
	struct list_node *next, *prev;
} list_node;

/* //~ 无须在这里定义
#define LIST_NODE_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
struct list_head name = LIST_NODE_INIT(name)

EL_STATIC void INIT_LIST_HEAD(struct list_node *list);

EL_STATIC void list_add(struct list_node *new1, struct list_node *head);
EL_STATIC void list_add_tail(struct list_node *new1, struct list_node *head);

EL_STATIC void list_del(struct list_node *entry);
EL_STATIC int list_empty(const struct list_node *head);

*/

#define list_entry(ptr, type, member) \
((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each_entry(pos, head, member)                \
for (pos = list_entry((head)->next, typeof(*pos), member);    \
&pos->member != (head);     \
pos = list_entry(pos->member.next, typeof(*pos), member))


////////////////////////// q  API ///////////////////////////////////////////
// 阻塞模式
typedef enum _q_node_mode_e_
{
	el_q_mode_unblock = 0x0,
	el_q_mode_block_empty = 0x1,
	el_q_mode_block_full = 0x2,	
	el_q_mode_max
} q_node_mode_e;

// 属性
typedef enum _q_node_property_e_
{
	el_q_node_max_nr = 0,
	el_q_node_empty_func,
	el_q_node_full_func,
	el_q_node_push_func,
	el_q_node_pop_func,
	el_q_node_priv_data,
	el_q_node_max
} q_node_property_e;

typedef int (* el_q_check_full_cb_t)(void *);
typedef int (* el_q_check_empty_cb_t)(void *);
typedef void (* el_q_full_cb_t)(void *);
typedef void (* el_q_empty_cb_t)(void *);
typedef void (* el_q_push_cb_t)(void *,struct list_node *new1);
typedef void (* el_q_pop_cb_t)(void *,struct list_node *poped);


struct q_head
{
	struct list_node lst_node;
	void *mutex;  // q mutex lock
	int node_nr;			// node count
	int max_node_nr;		// upper limit node count
	q_node_mode_e	mode;	// once this mode value is set, you should never change it during the q's life
	q_node_mode_e	origin_mode;
	pthread_cond_t  has_node;  // if empty, blocked
	pthread_cond_t  has_space;  // if full, blocked
	el_q_check_full_cb_t is_full_func;
	el_q_check_empty_cb_t is_empty_func;
	el_q_full_cb_t full_func;
	el_q_empty_cb_t empty_func;
	el_q_push_cb_t  push_func;
	el_q_pop_cb_t	pop_func;
	void *empty_parm;
	void *full_parm;
	int stop_blocking_empty;
	int stop_blocking_full;
	long data_size;
	void *priv;
	int cal_pts_denominator;
	int need_reset_time_limit;
	int64_t start_node_pts;
	int64_t current_node_pts;
};

EL_STATIC void q_set_property(struct q_head *q_head,q_node_property_e property,void *parm1,void *parm2);
EL_STATIC int q_default_is_full_func(void *param);
EL_STATIC int q_default_is_empty_func(void *param);
EL_STATIC int q_init(struct q_head *q_head,q_node_mode_e mode);
EL_STATIC void q_destroy(struct q_head *q_head);

//
// @ return value: 
// 0 : item is pushed
// -1:  q full , item is not pushed
// 
EL_STATIC int q_push(struct list_node *new1,struct q_head *q_head);
EL_STATIC int q_is_empty(struct q_head *q_head);
EL_STATIC int q_get_node_nr(struct q_head *q_head);
EL_STATIC int q_get_max_node_nr(struct q_head *q_head);

// if already empty, would block if mode is block, otherwise returns NULL
EL_STATIC struct list_node *q_pop(struct q_head *q_head);

// if already empty, returns NULL
// get first element
EL_STATIC struct list_node *q_get_first(struct q_head *q_head);



#endif //__EL_FFMPEG_LIST_H__

