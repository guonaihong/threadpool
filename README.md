[![Build Status](https://travis-ci.org/guonaihong/threadpool.svg?branch=master)](https://travis-ci.org/guonaihong/threadpool)

# A very simple thread pool
# Table of Contents

* Currently, the implementation
 * 最小化创建线程
 * 自动扩容与收缩线程
 * 一个通用的plugin模块
* API
 * [创建线程池](#创建线程池)
 * [给线程池添加任务](#给线程池添加任务)
 * [给线程池添加n个线程](#给线程池添加n个线程)
 * [给线程池删除n个线程](#给线程池添加n个线程)
 * [给线程池添加plugin](#给线程池添加plugin)
 * [等待线程池](#等待线程池)
 * [释放线程池](#释放线程池)

###### 创建线程池

```c
tp_pool_t *tp_pool_new(int max_threads, int chan_size, int tp_null); 
tp_pool_t *tp_pool_new(int max_threads, int chan_size, int min_threads, int tp_null);
tp_pool_t *tp_pool_new(int max_threads, int chan_size, int min_threads, int flags, int ms, int tp_null)
```
###### 给线程池添加任务
```c
int tp_pool_add(tp_pool_t *pool, void (*function)(void *), void *arg);
```
###### 给线程池添加n个线程
```c
int tp_pool_thread_addn(tp_pool_t *pool, int n);
```
###### 给线程池删除n个线程
```c
int tp_pool_thread_deln(tp_pool_t *pool, int n);
```
###### 给线程池添加plugin
```c
int tp_pool_plugin_producer_consumer_add(tp_pool_t   *pool,
                                         tp_plugin_t *produces,
                                         int          np,
                                         tp_plugin_t *consumers,
                                         int          nc);
```
###### 等待线程池
```c
int tp_pool_wait(tp_pool_t *pool, int flags);
```
###### 释放线程池
```c
int tp_pool_free(tp_pool_t *pool);
```

TODO
 * 修复bug,欢迎找bug
