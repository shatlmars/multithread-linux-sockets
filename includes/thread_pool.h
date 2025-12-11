#include <thread>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <vector>
#include <queue>
#include <atomic>
#include <unordered_map>
#include <any>
#include <functional>
#include <utility>
#include <variant>
#include <cassert>

enum TaskStatus{
    in_q, 
    completed
};

class Task{
public:
    template<typename FuncRetType, typename ...Args, typename ...FuncTypes>
    Task(FuncRetType(*func)(FuncTypes...), Args&&... args);
    void operator() ();
    bool has_result();
    std::any get_result() const;

private:
    std::function<void()> void_func;
    std::function<std::any()> any_func;
    std::any any_func_result; 
    bool is_void;
};

struct TaskInfo{
    TaskStatus status = TaskStatus::in_q;
    std::any result;
};

class ThreadPool{
public:
    ThreadPool(size_t n);
    ~ThreadPool();
    template<typename FuncReturnedType, typename ...FuncTypes, typename ...Args>
    uint64_t add_task(FuncReturnedType(*func)(FuncTypes...), Args&&... args);
    void wait(uint64_t task_id);
    std::any wait_result(uint64_t task_id); 
    void wait_all();
    template<typename T>
    void wait_result(uint64_t task_id, T& value);
private:
    void run(){
        while (!quite)
        {
            std::unique_lock<std::mutex> lock(q_mtx);
            q_cv.wait(lock, [this]()->bool{return !q.empty() || quite;});

            if(!q.empty() && !quite){
                std::pair<Task, uint64_t> task = std::move(q.front());
                q.pop();
                task.first();

                std::lock_guard<std::mutex> lock(task_info_mtx);
                if(task.first.has_result()){
                    task_info[task.second].result = task.first.get_result();
                }
                task_info[task.second].status = TaskStatus::completed;
                ++cnt_completed_task;
            }
            wait_all_cv.notify_all();
            task_info_cv.notify_all();
        }
        
    }


    std::vector<std::thread> threads;
    std::queue<std::pair<Task, uint64_t>> q;
    std::mutex q_mtx;
    std::condition_variable q_cv;
    

    std::condition_variable wait_all_cv;


    std::unordered_map<uint64_t, TaskInfo> task_info;
    std::condition_variable task_info_cv;
    std::mutex task_info_mtx;

    std::atomic<bool> quite{false};
    std::atomic<uint64_t> last_idx{0};
    std::atomic<uint64_t> cnt_completed_task{0};
};

