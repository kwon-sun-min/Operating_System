//2-1 split_n_merge���� ����
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <fstream>
#include <string>
#include <memory>


class Process {
public:
    std::string name;
    bool isForeground;
    std::thread thread;

    Process(const std::string& name, bool isForeground)
        : name(name), isForeground(isForeground), thread() {}

    Process(Process&& other) noexcept
        : name(std::move(other.name)), isForeground(other.isForeground), thread(std::move(other.thread)) {}

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
};

class ProcessNode {
public:
    std::shared_ptr<Process> process;
    std::shared_ptr<ProcessNode> next;

    ProcessNode(std::shared_ptr<Process> process)
        : process(process), next(nullptr) {}
};

class StackNode {
public:
    std::shared_ptr<ProcessNode> processList;
    std::shared_ptr<StackNode> next;

    StackNode() : processList(nullptr), next(nullptr) {}
};


std::vector<Process> dynamicQueue;
std::queue<Process> waitQueue;
std::mutex mtx;
std::condition_variable cv;
std::shared_ptr<StackNode> bottomNode = std::make_shared<StackNode>(); 
std::shared_ptr<StackNode> topNode = bottomNode; 
std::shared_ptr<StackNode> p = bottomNode; 
bool lineProcessed = false; 
bool ready = false; 
int processID = 0; 



// Main scheduling function
void scheduler() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return ready; }); 
        
        ready = false;
    }
}



void addNewStackNode() {
    std::shared_ptr<StackNode> newNode = std::make_shared<StackNode>();
    std::shared_ptr<StackNode> current = bottomNode;

    while (current->next != nullptr) {
        current = current->next;
    }

    current->next = newNode;
    topNode = newNode; 
}



void promote() {
    if (!p->processList) {
        // P�� ����Ű�� ����Ʈ�� ��� ������ �ƹ��͵� ���� ����
        return;
    }

    // P�� ����Ű�� ����Ʈ�� ��� ��带 ���� ����Ʈ�� ������ ����
    auto processNode = p->processList;
    p->processList = processNode->next; // P�� ����Ʈ���� ��� ����

    if (topNode->processList) {
        auto current = topNode->processList;
        while (current->next) {
            current = current->next;
        }
        current->next = processNode; // ���� ����Ʈ�� ������ ����
    }
    else {
        topNode->processList = processNode; // ���� ����Ʈ�� ��� ������ ���� ����
    }

    processNode->next = nullptr; // ���ο� ��ġ������ ����� next�� nullptr�� ����

    // �̵���Ų �� ����Ʈ�� ��� ������ �ش� ���� ��带 ����
    if (!p->processList && p != bottomNode) {
        auto current = bottomNode;
        while (current->next != p) {
            current = current->next;
        }
        current->next = p->next; // P�� ���ÿ��� ����
    }

    // P�� ���� ���� ���� �̵�
    if (p->next) {
        p = p->next;
    }
    else {
        p = bottomNode; // P�� topNode�� ��� bottomNode�� �̵�
    }
}

void split_n_merge() {
    int totalProcesses = 0;
    std::shared_ptr<StackNode> node = bottomNode;
    while (node != nullptr) {
        std::shared_ptr<ProcessNode> processNode = node->processList;
        while (processNode != nullptr) {
            totalProcesses++;
            processNode = processNode->next;
        }
        node = node->next;
    }

    // ���� ��� �� ���
    int stackNodeCount = 0;
    node = bottomNode;
    while (node != nullptr) {
        stackNodeCount++;
        node = node->next;
    }

    // �Ӱ�ġ ���
    int threshold = totalProcesses / stackNodeCount;

    // split_n_merge ���� ����
    std::shared_ptr<StackNode> current = bottomNode;
    while (current != nullptr) {
        int count = 0;
        auto processNode = current->processList;
        while (processNode) {
            count++;
            processNode = processNode->next;
        }

        if (count > threshold) {
            // ����Ʈ ���̰� �Ӱ�ġ�� �Ѿ�� ����Ʈ�� ���� ������ ���� ����Ʈ�� ������ ����
            auto midNode = current->processList;
            for (int i = 0; i < count / 2 - 1; i++) {
                midNode = midNode->next;
            }

            // ���� ����Ʈ�� ���� ������ �и�
            auto toPromote = midNode->next;
            midNode->next = nullptr;

            // ���� ����Ʈ�� ������ ����
            if (current->next) {
                auto upperCurrent = current->next->processList;
                if (upperCurrent) {
                    while (upperCurrent->next) {
                        upperCurrent = upperCurrent->next;
                    }
                    upperCurrent->next = toPromote;
                }
                else {
                    current->next->processList = toPromote;
                }
            }
            else {
                // �ֻ���(top)����Ʈ�� split�ϰ� �Ǹ� ���ÿ� ���ο� ����(����Ʈ)�� �߰���
                addNewStackNode();
                topNode->processList = toPromote;
            }
        }

        current = current->next;
    }
}

void enqueue(std::shared_ptr<Process> process) {
    auto newNode = std::make_shared<ProcessNode>(process);
    if (process->isForeground) {
        if (!topNode->processList) {
            topNode->processList = newNode;
        }
        else {
            auto current = topNode->processList;
            while (current->next) {
                current = current->next;
            }
            current->next = newNode;
        }
    }
    else {
        if (!bottomNode->processList) {
            bottomNode->processList = newNode;
        }
        else {
            auto current = bottomNode->processList;
            while (current->next) {
                current = current->next;
            }
            current->next = newNode;
        }
    }
    split_n_merge();
}

std::shared_ptr<Process> dequeue() {
    if (topNode->processList == nullptr) {
        if (topNode != bottomNode) {
            std::shared_ptr<StackNode> current = bottomNode;
            while (current->next != topNode) {
                current = current->next;
            }
            current->next = nullptr; 
            topNode = current; 
        }
        split_n_merge();
        return nullptr; 
    }

    auto processNode = topNode->processList;
    topNode->processList = processNode->next; 

    if (topNode->processList == nullptr && topNode != bottomNode) {
        std::shared_ptr<StackNode> current = bottomNode;
        while (current->next != topNode) {
            current = current->next;
        }
        current->next = nullptr; 
        topNode = current; 
    }
    split_n_merge();
    return processNode->process; 
}






void printDynamicQueue() {
    std::shared_ptr<StackNode> currentStackNode = bottomNode;
    while (currentStackNode != nullptr) {
        std::shared_ptr<ProcessNode> currentProcessNode = currentStackNode->processList;
        while (currentProcessNode != nullptr) {
            std::cout << currentProcessNode->process->name << " -> ";
            currentProcessNode = currentProcessNode->next;
        }
        std::cout << "NULL" << std::endl;
        currentStackNode = currentStackNode->next;
    }
    std::cout << "NULL" << std::endl; 
}

void printDynamicQueueWithStackLinks() {
    std::shared_ptr<StackNode> currentStackNode = bottomNode;
    int stackLevel = 0;
    while (currentStackNode != nullptr) {
        std::cout << "Stack Level " << stackLevel << " -> ";
        std::shared_ptr<ProcessNode> currentProcessNode = currentStackNode->processList;
        while (currentProcessNode != nullptr) {
            std::cout << currentProcessNode->process->name << " -> ";
            currentProcessNode = currentProcessNode->next;
        }
        std::cout << "NULL" << std::endl;
        currentStackNode = currentStackNode->next;
        stackLevel++;
    }
    std::cout << "End of Stack\n" << std::endl; 
}

void createAndEnqueueProcesses() {
    auto process1 = std::make_shared<Process>("Process1", true);
    auto process2 = std::make_shared<Process>("Process2", false);
    auto process3 = std::make_shared<Process>("Process3", true);
    auto process4 = std::make_shared<Process>("Process4", false);
    auto process5 = std::make_shared<Process>("Process5", true);
    auto process6 = std::make_shared<Process>("Process6", false);
    auto process7 = std::make_shared<Process>("Process7", true);
    auto process8 = std::make_shared<Process>("Process8", false);
    auto process9 = std::make_shared<Process>("Process9", true);
    auto process10 = std::make_shared<Process>("Process10", false);

    // ���ÿ� ���� ��带 �߰��Ͽ� �Ӱ�ġ�� �ʰ��ϵ��� ����
    addNewStackNode();
    addNewStackNode();
    addNewStackNode();

    enqueue(process1);
    enqueue(process2);
    enqueue(process3);
    enqueue(process4);
    enqueue(process5);
    enqueue(process6);
    enqueue(process7);
    enqueue(process8);
    enqueue(process9);
    enqueue(process10);
}

void shellProcess() {
    using namespace std::chrono_literals;
    std::ifstream file("commands.txt");
    std::string command;
    while (std::getline(file, command)) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "Shell received command: " << command << std::endl;

            lineProcessed = true; // �� �� ó�� 
            auto dequeuedProcess = dequeue();
            if (dequeuedProcess != nullptr) {
                std::cout << "\nDispatching process: " << dequeuedProcess->name << std::endl;
            }

        }
        cv.notify_one(); 
        std::this_thread::sleep_for(3s); // Y�� ���� ���
    }
}

// Modified monitorProcess function
void monitorProcess() {
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> lock(mtx);
    while (true) {
        cv.wait(lock, [] { return lineProcessed; }); // shell ���μ����� ��ȣ ���
        std::cout << "Monitor process is now running." << std::endl;
        printDynamicQueueWithStackLinks(); // DQ�� ���¸� ���
        lineProcessed = false; 
        std::this_thread::sleep_for(2s); // X�� ���� ���
    }
}


int main() {
    std::thread shellThread(shellProcess);
    std::thread monitorThread(monitorProcess);

    createAndEnqueueProcesses(); // ���μ��� ���� �� ť�� �߰�

    shellThread.join();
    monitorThread.join();

    return 0;
}



