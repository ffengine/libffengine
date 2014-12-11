//
//  el_ffmpeg_list.c
//  ELMediaLib
//
//  Created by Wei Xie on 12-7-3.
//  Copyright (c) 2012年 __MyCompanyName__. All rights reserved.
//

#include <pthread.h>

#include "el_std.h"
#include "el_mutex.h"
#include "el_log.h"
#include "el_list.h"

EL_STATIC inline void INIT_LIST_HEAD(struct list_node *list)
{
    list->next = list;
    list->prev = list;
}

EL_STATIC inline void __list_add(struct list_node *new1,
                                  struct list_node *prev,
                                  struct list_node *next)
{
    next->prev = new1;
    new1->next = next;
    new1->prev = prev;
    prev->next = new1;
}

EL_STATIC inline void __list_del(struct list_node * prev, struct list_node * next)
{
    next->prev = prev;
    prev->next = next;
}

EL_STATIC inline void list_add(struct list_node *new1, struct list_node *head)
{
    __list_add(new1, head, head->next);
}

EL_STATIC inline void list_add_tail(struct list_node *new1, struct list_node *head)
{
    __list_add(new1, head->prev, head);
}

EL_STATIC inline void list_del(struct list_node *entry)
{
    __list_del(entry->prev, entry->next);
}

EL_STATIC inline int list_empty(const struct list_node *head)
{
    return (head->next == head);
}

////////////////////////// q  API ///////////////////////////////////////////

EL_STATIC void q_set_property(struct q_head *q_head,q_node_property_e property,void *parm1,void *parm2)
{
    el_mutex_lock(q_head->mutex);
    switch(property)
    {
        case el_q_node_max_nr:
        {
            q_head->max_node_nr = (int)parm1;
            break;
        }
        case el_q_node_empty_func:
        {
            q_head->empty_func = (el_q_empty_cb_t)parm1;
            q_head->empty_parm = parm2;
            break;
        }
        case el_q_node_full_func:
        {
            q_head->full_func = (el_q_full_cb_t)parm1;
            q_head->full_parm = parm2;
            break;
        }
        case el_q_node_push_func:
        {
            q_head->push_func = (el_q_push_cb_t)parm1;
            break;
        }
        case el_q_node_pop_func:
        {
            q_head->pop_func = (el_q_pop_cb_t)parm1;
            break;
        }
        case el_q_node_priv_data:
        {
            q_head->priv = parm1;
            break;
        }
        default:
        {
            break;
        }
    }

    el_mutex_unlock(q_head->mutex);
    return;
}

EL_STATIC inline int q_default_is_full_func(void *param)
{
    struct q_head *q_head = (struct q_head *)(param);
    return (q_head->node_nr >= q_head->max_node_nr);
}

EL_STATIC inline int q_default_is_empty_func(void *param)
{
    struct q_head *q_head = (struct q_head *)(param);
    return (!q_head->node_nr);
}

EL_STATIC int q_init(struct q_head *q_head, q_node_mode_e mode)
{
    memset(q_head,0,sizeof(struct q_head));
    INIT_LIST_HEAD(&q_head->lst_node);
    q_head->node_nr = 0;
    q_head->mutex = el_mutex_init();

    if(!q_head->mutex)
    {
        return -1;
    }
    
    q_head->mode = mode;
    q_head->origin_mode = mode;
    
    if(mode & el_q_mode_block_empty)
    {
        pthread_cond_init(&q_head->has_node, (const pthread_condattr_t *)NULL); 
    }
    
    if(mode & el_q_mode_block_full)
    {
        pthread_cond_init(&q_head->has_space, (const pthread_condattr_t *)NULL);
    }
    
    q_head->max_node_nr = 0xFFFF; // default value
    q_head->is_full_func = q_default_is_full_func;
    q_head->is_empty_func = q_default_is_empty_func;
    
    return 0;
}

EL_STATIC void q_destroy(struct q_head *q_head)
{
    if(q_head->mode & el_q_mode_block_empty)
    {
        pthread_cond_destroy(&q_head->has_node); 
    }
    
    if(q_head->mode & el_q_mode_block_full)
    {
        pthread_cond_destroy(&q_head->has_space);
    }
    
    el_mutex_destroy(q_head->mutex);
}

//
// @ return value: 
// 0 : item is pushed
// -1:  q full , item is not pushed
// 
EL_STATIC int q_push(struct list_node *new1,struct q_head *q_head)
{
    //el_mutex_lock(q_head->mutex);
    if(q_head->mode & el_q_mode_block_full)
    {
        //while(q_head->node_nr >= q_head->max_node_nr)
        while(q_head->is_full_func(q_head))
        {
            // block until new node added
            if(q_head->full_func)
            {
                //el_mutex_unlock(q_head->mutex);
                q_head->full_func(q_head->full_parm);
            }
            
            el_mutex_lock(q_head->mutex);
            pthread_cond_wait(&q_head->has_space, (pthread_mutex_t *)q_head->mutex);
            el_mutex_unlock(q_head->mutex);
            
            if(q_head->stop_blocking_full)
            {
                break;
            }
        }
    }
    else
    {
        //if(q_head->node_nr >= q_head->max_node_nr)
        if(q_head->is_full_func(q_head))
        {
            if(q_head->full_func)
            {
                q_head->full_func(q_head->full_parm);
            }
        }
    }
    
    el_mutex_lock(q_head->mutex);
    list_add_tail(new1,&q_head->lst_node);
    q_head->node_nr++;
    //printf("e:q_head = %x,q_head->node_nr = %d\n",q_head,q_head->node_nr);
    el_mutex_unlock(q_head->mutex);
    if(q_head->mode & el_q_mode_block_empty)
    {
        pthread_cond_signal(&q_head->has_node);
    }
    if(q_head->push_func)
    {
        q_head->push_func(q_head,new1);	
    }
    return 0;
}

EL_STATIC inline int q_is_empty(struct q_head *q_head)
{
    return (!q_head->node_nr);
}

EL_STATIC inline int q_get_node_nr(struct q_head *q_head)
{
    return q_head->node_nr;
}

EL_STATIC inline int q_get_max_node_nr(struct q_head *q_head)
{
    return q_head->max_node_nr;
}

// if already empty, would block if mode is block, otherwise returns NULL
EL_STATIC struct list_node *q_pop(struct q_head *q_head)
{
    struct list_node *poped_item;
    el_mutex_lock(q_head->mutex);
    
    if(q_head->mode & el_q_mode_block_empty)
    {
        // block mode
        //while(list_empty(&q_head->lst_head))
        while(q_head->is_empty_func(q_head))
        {
            // block until new code added
            if(q_head->empty_func)
            {
                el_mutex_unlock(q_head->mutex);
                q_head->empty_func(q_head->empty_parm);
                el_mutex_lock(q_head->mutex);
            }
            pthread_cond_wait(&q_head->has_node, (pthread_mutex_t *)q_head->mutex);
            if(q_head->stop_blocking_empty)
            {
                break;
            }
        }
    }
    else
    {
        // unblock mode
        //if(list_empty(&q_head->lst_head))
        if(q_head->is_empty_func(q_head))
        {
            // unblock mode
            el_mutex_unlock(q_head->mutex);
            if(q_head->empty_func)
            {
                q_head->empty_func(q_head->empty_parm);
            }
            
            return (struct list_node *)NULL;
        }
    }
    
    poped_item = q_head->lst_node.next;
    list_del(poped_item);
    q_head->node_nr--;
    el_mutex_unlock(q_head->mutex);
    if(q_head->mode & el_q_mode_block_full)
    {
        pthread_cond_signal(&q_head->has_space);
    }
    if(q_head->pop_func)
    {
        q_head->pop_func(q_head,poped_item);	
    }
    
    return poped_item;
}

// if already empty, returns NULL
// get first element
EL_STATIC struct list_node *q_get_first(struct q_head *q_head)
{
    struct list_node *got_item;
    el_mutex_lock(q_head->mutex);
    if(list_empty(&q_head->lst_node))
    {
        el_mutex_unlock(q_head->mutex);
        return (struct list_node *)NULL;
    }
    got_item = q_head->lst_node.next;
    el_mutex_unlock(q_head->mutex);
    return got_item;
}


