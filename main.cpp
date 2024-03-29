#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <ctime>
#include <thread>
#include <random>
#include <condition_variable>
#include <chrono>
#include <Windows.h>
using namespace std;


struct Request {
    int time;
    int priority;
    int type;
    int group;

    Request(int receivedTime, int receivedGroup) {
        time = receivedTime;
        group = receivedGroup;
        type = rand() % 4;
        priority = rand() % 4;
    }
};

struct ATM {
    chrono::system_clock::time_point endingOfProcess; // беды с временем
    int group;
    bool isBusy;
    thread thread;

    void updateStatus(Request request) {
        isBusy = true;
        chrono::system_clock::time_point now = chrono::system_clock::now();
        endingOfProcess = now + chrono::seconds(request.time);
    }

    bool isFree() {
        chrono::system_clock::time_point now = chrono::system_clock::now();
        std::chrono::duration<double> diff = (now - endingOfProcess);
        // cout << diff.count() << endl;
        return diff.count() > 0;
    }

    ATM() {
        group = 0,
        isBusy = false;
    }
    ATM(const ATM& ATM) {
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

int getRandomTime(int startSeconds = 1, int endSeconds = 5) {
    return rand() % (endSeconds - startSeconds + 1) + startSeconds;
}

void ATMProcess(ATM& ATM, queue<Request>& queue, mutex& mutex, condition_variable& cv) {
    while (true) {
        unique_lock<std::mutex> lock(mutex);

        // засыпаем и после обновляем статус занятости
        if (ATM.isFree()) {
            this_thread::sleep_for(chrono::seconds(getRandomTime()));
            ATM.endingOfProcess = chrono::system_clock::now() - chrono::milliseconds(100);
            ATM.isBusy = false;
        }

        // освобождаем поток и ждем пока очередь пуста или банкомат занят
        while (queue.empty() || ATM.isBusy) {
            cv.wait(lock);
            /*
                без этой штуки все таки не работает :(
                эта строка освобождает мьютекс и приостанавливает этот поток, пока не выполнится условная перменная
                без без этой строки поток будет работать все время и не даст  выполняться другим потокам,
                которые с этим мьютексом работают
             */
        }

        Request request = queue.front();
        queue.pop();

        lock.unlock();

        // банкомат начинает работать
        ATM.updateStatus(request);
        cv.notify_all();
    }
}

void generator(queue<Request>& requests, mutex& mutex, condition_variable& cv, int countGroups, int maxQueueSize) {
    int currentGroup = 0;
    while (true) {
        unique_lock<std::mutex> lock(mutex);

        while (requests.size() >= maxQueueSize) {
            cv.wait(lock);
        }

        lock.unlock();

        Request request(getRandomTime(2, 5), currentGroup);
        lock.lock();

        requests.push(request);

        lock.unlock();
        cv.notify_all();

        currentGroup = (currentGroup + 1) % countGroups;
        this_thread::sleep_for(chrono::milliseconds(getRandomTime(300, 1000)));
    }
}

void printStatus(int groups, int ATMsNumber, vector<vector<ATM>> ATMs, queue<Request> requests) {
    cout << "Elements in queue: " << requests.size() << endl << endl;
    for (int group = 0; group < groups; group++) {
        for (int i = 0; i < ATMsNumber; i++) {
            cout << "ATM group: " << ATMs[group][i].group << ", " << "number: " << i + 1 << endl;
            cout << "ATM is busy now: " << ATMs[group][i].isBusy << endl;
            if (ATMs[group][i].isBusy) {
                // cout << "ATM will not be busy at: " << ctime(&ATMs[group][i].endingOfProcess);
            }
        }
        cout << endl << endl;
    }
}

// завершение работы программы при нажатии ESC
DWORD WINAPI checkEscape() {
while (GetAsyncKeyState(VK_ESCAPE) == 0) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    exit(0);
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
            ATMs[i][j].thread = thread(ATMProcess, ref(ATMs[i][j]), ref(requests), ref(mutex), ref(cv));
        }
    }

    thread requestGeneratorThread(generator, ref(requests), ref(mutex), ref(cv),
                                  ref(groups), ref(storageSize));

    // отдельный поток для обработки нажатия ESC
    HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)checkEscape,
                                 NULL, 0, 0);

    while (true) {
        printStatus(groups, ATMsNumber, ATMs, requests);
        this_thread::sleep_for(chrono::seconds(1));
    }
}