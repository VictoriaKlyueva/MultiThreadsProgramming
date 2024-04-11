#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <random>
#include <condition_variable>
#include <chrono>
#include <Windows.h>
using namespace std;


int getRandomNumber(int start, int end) {
    return rand() % (end - start + 1) + start;
}

struct Request {
    int group;
    int type;
    int priority;

    Request(int group){
        group = group;
        type = getRandomNumber(1, 3);
        priority = getRandomNumber(1, 3);

    };
};

struct ATM {
    int group;
    bool isBusy;
    thread thread;

    ATM() {
        group = -1;
        isBusy = -1;
    }

    ATM(const ATM& ATM){
        group = ATM.group;
        isBusy = ATM.isBusy;
    }
};

void fillGroups(vector<vector<ATM>>& ATMs, int groups, int ATMsNumber) {
    for (int group = 0; group < groups; group++) {
        for (int number = 0; number < ATMsNumber; number++) {
            ATMs[group][number].group = group;
        }
    }
}

Request generateRequest(int group) {
    Request request(group);
    return request;
}

void processRequests(ATM& ATM, queue<Request>& queue, mutex& mutex, condition_variable& cv) {
    while (true) {
        unique_lock<std::mutex> lock(mutex);

        while (queue.empty()) {
            cv.wait(lock);
        }

        Request request = queue.front();

        queue.pop();
        lock.unlock();

        ATM.isBusy = true;
        this_thread::sleep_for(chrono::milliseconds(getRandomNumber(100, 2000)));
        ATM.isBusy = false;

        cv.notify_all();
    }
}

void generator(queue<Request>& requests, int countGroups, int maxQueueSize, mutex& mutex, condition_variable& cv) {
    int currentGroup = 0;
    while (true) {
        unique_lock<std::mutex> lock(mutex);
        while (requests.size() >= maxQueueSize) {
            cv.wait(lock);
            /*
             * без этой строчки все таки не работает :(
             * она блокирует текущий поток на время невыполнени условия
             * если ее убрать, текуйщий поток не даст выполняться другим потокам
             */
        }

        lock.unlock();

        Request request = generateRequest(currentGroup);

        lock.lock();
        requests.push(request);
        lock.unlock();

        cv.notify_all();

        currentGroup = (currentGroup + 1) % countGroups;
        this_thread::sleep_for(chrono::milliseconds(getRandomNumber(100, 200)));
    }
}

// завершение работы программы при нажатии ESC
DWORD WINAPI checkEscape() {
    while (GetAsyncKeyState(VK_ESCAPE) == 0) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    exit(0);
}

void printStatus(int groups, int ATMsNumber, vector<vector<ATM>> ATMs, queue<Request>& requests) {
    cout << "Elements in queue: " << requests.size() << endl << endl;
    for (int group = 0; group < groups; group++) {
        for (int i = 0; i < ATMsNumber; i++) {
            cout << "ATM group: " << ATMs[group][i].group << ", " << "number: " << i + 1 << endl;
            cout << "ATM is busy now: " << ATMs[group][i].isBusy << endl;
        }
        cout << endl << endl;
    }
}

int main() {
    int groups, ATMsNumber, storageSize;
    cout << "Enter number of groups, number of ATMs in one group and storage size: " << endl;
    cin >> groups >> ATMsNumber >> storageSize;

    vector<vector<ATM>> ATMs(groups, vector<ATM>(ATMsNumber));
    queue<Request> requests;

    mutex mutex;
    condition_variable cv;

    fillGroups(ATMs, groups, ATMsNumber);

    for (int i = 0; i < groups; i++) {
        for (int j = 0; j < ATMsNumber; j++) {
            ATMs[i][j].thread = thread(processRequests, ref(ATMs[i][j]), ref(requests), ref(mutex), ref(cv));
        }
    }

    thread generatorThread(generator, ref(requests), ref(groups), ref(storageSize),
                           ref(mutex), ref(cv));

    // отдельный поток для обработки нажатия ESC
    HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)checkEscape,
                                 NULL, 0, 0);

    while (true) {
        printStatus(groups, ATMsNumber, ATMs, requests);
        this_thread::sleep_for(chrono::seconds(1));
    }
}