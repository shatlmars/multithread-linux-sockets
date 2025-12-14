#include "includes/thread_pool.h"


template<typename FuncRetType, typename ...Args, typename ...FuncTypes>
Task::Task(FuncRetType(*func)(FuncTypes...), Args&&... args): is_void(std::is_void_v<FuncRetType>)
{
    if constexpr(std::is_void_v<FuncRetType>){
        void_func = std::bind(func, args...);
        any_func = []()->int{return 0;};
    }else{
        void_func = []()->void{};
        any_func = std::bind(func, args...);
    }
}

void Task::operator() (){
    void_func();
    any_func_result = any_func();
}

bool Task::has_result(){
    return !is_void;
}

std::any Task::get_result() const {
    assert(!is_void);
    assert(any_func_result.has_value());
    return any_func_result;
}


/*-----------------------------------------------------------------------------------------*/

ThreadPool::ThreadPool(size_t n){
    for(size_t i = 0; i < n; i++){
        threads.emplace_back(&ThreadPool::run, this);
    }
}
// template<typename FuncReturnedType, typename ...FuncTypes, typename ...Args>
// uint64_t ThreadPool::add_task(FuncReturnedType(*func)(FuncTypes...), Args&&... args)
// template<typename F, typename... Args>
// uint64_t ThreadPool::add_task(F&& f, Args&&... args)
// {
//     const uint64_t task_id = last_idx++;


//     {
//     std::lock_guard<std::mutex> lock(task_info_mtx);
//     task_info[task_id] = TaskInfo{};
//     }


//     std::lock_guard<std::mutex> q_lock(q_mtx);
//     q.emplace(Task(std::forward<F>(f), std::forward<Args>(args)...), task_id);
//     q_cv.notify_one();
//     return task_id;
// }

void ThreadPool::wait(uint64_t task_id){
    std::unique_lock<std::mutex> lock(task_info_mtx);
    task_info_cv.wait(lock, [this, task_id]()->bool{
        return task_id < last_idx && task_info[task_id].status == TaskStatus::completed;
    });
}


std::any ThreadPool::wait_result(uint64_t task_id){
    std::unique_lock<std::mutex> lock(task_info_mtx);
    task_info_cv.wait(lock, [this, task_id]()->bool{
        return task_id < last_idx && task_info[task_id].status == TaskStatus::completed;
    });

    return task_info[task_id].result;
}

template<typename T>
void ThreadPool::wait_result(uint64_t task_id, T& value){
    std::unique_lock<std::mutex> lock(task_info_mtx);
    task_info_cv.wait(lock, [this, task_id]()->bool{
        return task_id < last_idx && task_info[task_id].status == TaskStatus::completed;
    });

    value = std::any_cast<T>(task_info[task_id].result);
}
void ThreadPool::wait_all(){
    std::unique_lock<std::mutex> lock(task_info_mtx);
    wait_all_cv.wait(lock, [this]()->bool{return cnt_completed_task == last_idx;});
}

ThreadPool::~ThreadPool(){
    quite = true;
    q_cv.notify_all();
    for(int i = 0; i < threads.size(); ++i){
        if(threads[i].joinable()){
            threads[i].join();
        }
    }

}
