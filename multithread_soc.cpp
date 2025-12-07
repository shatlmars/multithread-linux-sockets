#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <queue>
#include <memory> 
#include <unistd.h>

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

int main(){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;


    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "Server listening on port: 12345\n";
    std::thread worker(worker_thread);
    worker.detach();


    while (true)
    {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::cout << "client connected\n";

        std::thread(client_thread, client_fd).detach();
    
    }
    
    close(server_fd);

}