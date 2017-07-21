#ifndef _RAYTRACER_THREAD_POOL_H_
#define _RAYTRACER_THREAD_POOL_H_

struct task
{
    task() {}
    virtual ~task() {}
    virtual void run() = 0;
};

struct work_queue
{
    work_queue(){}
    ~work_queue(){}
    
    task *nextTask()
    {
        task *t = nullptr;
        
        // TODO(nfauvet): est ce que je dois faire des lock_guard?
        std::unique_lock<std::mutex> g(queue_mutex);
        
        // are there tasks to work on?
        if ( !finished || !tasks.empty() )
        {
            if ( tasks.empty() )
            {
                // not finished and no tasks? wait for one!
                work_available_condition.wait(g);
            }
            
            //assert(!tasks.empty());
            if ( !tasks.empty() )
            {
                t = tasks.front();
                tasks.pop();
            }
        }
        
        return t;
    }
    
    void addTask( task *t )
    {
        // stop adding tasks if finished is set
        // or else starving threads will never die!
        if ( !finished )
        {
            std::unique_lock<std::mutex> g(queue_mutex);
            
            tasks.push(t);
            // signal that there is work
            work_available_condition.notify_one(); // signal_all() ???
        }
    }
    
    void finish()
    {
        std::unique_lock<std::mutex> g(queue_mutex);
        finished = true;
        // signal for waiting threads that there is work
        // but it is also finished, so they quit waiting
        // and die happily
        work_available_condition.notify_all();
    }
    
    bool hasWork()
    {
        return !tasks.empty();
    }
    
    std::queue<task*> tasks;
    bool finished = false;
    std::mutex queue_mutex;
    std::condition_variable work_available_condition;
};


static void *get_work_from_queue( void *user_data )
{
    task *t = nullptr;
    work_queue *queue = (work_queue*)user_data;
    // blocking wait until there is a task
    // or "finished" is signaled
    while ( (t = queue->nextTask()) != nullptr )
    {
        t->run();
        delete t;
    }
    
    return nullptr;
}

struct thread_pool
{
    thread_pool( int n ) : num_threads(n)
    {
        threads = new std::thread[n];
        for( int i = 0; i < num_threads; ++i )
        {
            threads[i] = std::thread(&get_work_from_queue, &queue);
        }
    }
    
    ~thread_pool()
    {
        // signal the queue and the waiting threads that this is the end.
        // waiting threads are freed from their locks.
        queue.finish();
        
        // inefficient way to wait...
        while( queue.hasWork() ) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        
        for ( int i = 0; i < num_threads; ++i )
        {
            threads[i].join();
        }
        
        delete [] threads;
    }
    
    void addTask( task *t )
    {
        queue.addTask(t);
    }
    
    void finish()
    {
        queue.finish();
    }
    
    bool hasWork()
    {
        return queue.hasWork();
    }
    
    std::thread *threads = nullptr;
    int num_threads = 0;
    work_queue queue;
};

#endif // _RAYTRACER_THREAD_POOL_H_
