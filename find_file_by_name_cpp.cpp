//1 % pplication capp, to find file on
//3. to nake this search faster - app creates new searchin• %%read for each sab.directord in coot director, but not rare than 8 execution threads.
//•.thread who will find needed file hes to print full path to the file and he% to notify all the met thread% to atop ccccc nine,
//-belicetion just uses this lib., to find files in a fast way.
//- use % %kettle for c on /
//makefile, gcc, static library)


#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <stdexcept>
#include <conio.h>
#include <string>
#include <unordered_map>

const int MAX_THREADS = 8;
std::mutex mtx;
std::condition_variable cv;
std::atomic_bool fileFound(false);
std::atomic_bool stopSearch(false);
std::string result_path = "";

std::unordered_map<std::thread::id, std::filesystem::directory_entry> entryMap;

class ThreadPool {
public:
	ThreadPool(int numThreads) {
		for (int i = 0; i < numThreads; ++i) {
			threads.emplace_back(std::thread(&ThreadPool::workerThread, this));
		}
	}

	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(mtx);
			stopSearch = true;
		}

		cv.notify_all();

		for (std::thread& thread : threads) {
			thread.join();
		}
	}

	void addTask(std::function<void()> task) {
		std::unique_lock<std::mutex> lock(mtx);
		tasks.push(task);
		cv.notify_one();
	}

	std::thread::id getThreadId() const {
		return std::this_thread::get_id();
	}

	void setEntryForThread(std::thread::id threadId, const std::filesystem::directory_entry& entry) {
		std::lock_guard<std::mutex> lock(mtx);
		entryMap[threadId] = entry;
	}

	std::filesystem::directory_entry getEntryForThread(std::thread::id threadId) {
		std::lock_guard<std::mutex> lock(mtx);
		return entryMap[threadId];
	}

private:
	void workerThread() {
		while (true) {
			std::function<void()> task;

			{
				std::unique_lock<std::mutex> lock(mtx);

				cv.wait(lock, [&]() {
					return stopSearch || !tasks.empty();
				});

				if (stopSearch) {
					break;
				}

				task = tasks.front();
				tasks.pop();
			}

			try {
				task();
			}
			catch (const std::exception& ex) {
				std::lock_guard<std::mutex> lock(mtx);
				std::cerr << "Exception in thread: " << ex.what() << std::endl;
			}
			catch (...) {
				std::lock_guard<std::mutex> lock(mtx);
				std::cerr << "Unknown exception in thread." << std::endl;
			}
		}
	}

	std::vector<std::thread> threads;
	std::queue<std::function<void()>> tasks;
};

ThreadPool threadPool(MAX_THREADS);

void searchFile(const std::filesystem::path& directory, const std::string& targetFileName) {
	for (const auto& entry_notmt : std::filesystem::directory_iterator(directory)) {
		if (stopSearch) return;

		threadPool.setEntryForThread(std::this_thread::get_id(), entry_notmt);

		auto entry = threadPool.getEntryForThread(std::this_thread::get_id());

		if (entry.is_directory()) {
			threadPool.addTask([&]() {
				std::cout << "Looking at directory: " << entry.path() << std::endl;
				searchFile(entry.path(), targetFileName);
			});
		}
		else if (entry.is_regular_file() && entry.path().filename() == targetFileName) {
			std::lock_guard<std::mutex> lock(mtx);
			fileFound = true;
			std::cout << "Found file at: " << entry.path() << " (Thread ID: " << std::this_thread::get_id() << ")" << std::endl;
			result_path = entry.path().generic_string();
			stopSearch = true; // Set stopSearch to true to stop all threads gracefully
			cv.notify_all();
			return;
		}
	}
}


int main() {
	std::string targetFileName;
	std::cout << "To stop search press 'z' on keyboard";
	std::cout << "\nEnter the name of the file to search for: ";
	std::cin >> targetFileName;

	std::filesystem::path rootDirectory; // Root directory Windows or linus
#ifdef _WIN32
	rootDirectory = "C:\\";
#else
	rootDirectory = "/";
#endif

	threadPool.addTask([&]() {
		searchFile(rootDirectory, targetFileName);
	});

	// Main loop to check for 'z' key press and set stopSearch to true
	while (!fileFound) {
		if (_kbhit()) {
			char ch = _getch();
			if (ch == 'z') {
				stopSearch = true;
				break;
			}
		}
	}


	cv.notify_all(); // Notify other threads to stop

	// Wait for some time for other threads to finish
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::cout << "\nResult: " << result_path << std::endl;

	return 0;
}
