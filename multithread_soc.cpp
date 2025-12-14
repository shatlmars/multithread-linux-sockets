#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <condition_variable>
#include <memory> 
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <vector>
#include <queue>
#include <atomic>

// #include "includes/thread_pool.h"


static const int MAX_EVENTS = 1024;

std::queue<std::vector<char>> message_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

void worker_thread(){
    while (true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, []{
            return !message_queue.empty();
        });

        std::vector<char> buf = std::move(message_queue.front());
        message_queue.pop();
        lock.unlock();
        std::string s(buf.begin() , buf.end());
        std::cout << "[Worker] Received message: " << s << "\n";
    }
}

void client_thread(int client_fd){
    while (true)
    {
        std::vector<char> buf(4096);
        ssize_t n = read(client_fd, buf.data(), buf.size());
        if(n <= 0){
            std::cout << "Client disconnected\n";
            close(client_fd);
            return;
        }

        buf.resize(n);

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push(std::move(buf));

        }

        queue_cv.notify_all();
    }
    
}


int sum(int x, int y){
    return x + y;
}

int make_fd_nonblocking(int& fd){
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]){
    int port = 12345;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("server_fd error\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if((bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr))) < 0){
        perror("bind error\n");
        return 0;
    }
    if((listen(server_fd, SOMAXCONN)) < 0){
        perror("listen error\n");
        return 0;
    }


    make_fd_nonblocking(server_fd);

    int epoll_fd = epoll_create1(0);

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;


    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    constexpr int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];

    char buffer[4096];

    while (true)
    {

        auto start_time = std::chrono::high_resolution_clock::now();
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for(int i = 0; i < n; i++){
            int fd = events[i].data.fd;
            if(fd == server_fd){
                std::cout << "new client\n";
                for(;;){
                    int client_fd = accept(server_fd, nullptr, nullptr);
                    if(client_fd < 0){
                        break;
                    }
                    make_fd_nonblocking(client_fd);

                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLRDHUP;
                    cev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
                }
            }else{
                std::cout << "new msg\n";
                if(events[i].events & EPOLLRDHUP){
                    close(fd);
                    std::cout << "client disconnected\n";
                    continue;
                }

                for(;;){
                    ssize_t bytes = read(fd, buffer, sizeof(buffer));
                    if(bytes > 0){
                        write(fd, buffer, bytes);
                        std::cout<< buffer << "\n" ;
                    }else if(bytes == 0){
                        close(fd);
                        
                        break;
                    }else{
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }
                        close(fd);
                        perror("error\n");
                        break;
                    }
                }
            }
        }
         auto end_time = std::chrono::high_resolution_clock::now();
          
         auto duration = end_time - start_time;
         std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "\n";
    }
    

    return 0;
}

// int main(){
//     int server_fd = socket(AF_INET, SOCK_STREAM, 0);

//     int opt = 1;
//     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

//     sockaddr_in addr{};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(12345);
//     addr.sin_addr.s_addr = INADDR_ANY;


//     bind(server_fd, (sockaddr*)&addr, sizeof(addr));
//     listen(server_fd, 10);

//     std::cout << "Server listening on port: 12345\n";
//     std::thread worker(worker_thread);
//     worker.detach();


//     while (true)
//     {
//         int client_fd = accept(server_fd, nullptr, nullptr);
//         std::cout << "client connected\n";

//         std::thread(client_thread, client_fd).detach();
    
//     }
    
//     close(server_fd);
//     ThreadPool t(5);
// }