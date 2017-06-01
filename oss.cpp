#include <iostream>
#include <ctime>
#include <unistd.h>
#include <cstdlib>
#include <sys/shm.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/mman.h>
#include <iterator>

using namespace std;

const int FRAME_ALLOCATION_SIZE = 5;
const int MAIN_MEMORY_SIZE = 50;
const int NUM_OF_PROCESSES = 10;
const int PAGE_REQUEST_STREAM_SIZE = 15;
const int PAGES_PER_PROCESS = 8;

void generateRequestStream(int[], const int, unsigned int);
bool requestPageFIFO(int[], const int, const int);
bool requestPageOptimal(int[], const int, const int, const int[], const int);
int getTimeToNextRequest(const int[], const int, const int);
int getIndexOfOptimalPolicySwap(int[], const int[], const int, const int);
void releaseMemory();


//Main memory for fifo algorithm
int *fifoMainMemory;
int fdFifoMainMemory;

//Main memory for optimal policy
int *optimalMainMemory;
int fdOptimalMainMemory;



int main(){
	unsigned int seed = time(0);

	/**
		Initialize FIFO Main Memory Array
	**/

	fdFifoMainMemory = shm_open("/fifoMainMemory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	ftruncate(fdFifoMainMemory, sizeof(int) * MAIN_MEMORY_SIZE);
	fifoMainMemory = (int *) mmap(NULL, sizeof(int) * MAIN_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdFifoMainMemory, 0);

	for(int i = 0; i < MAIN_MEMORY_SIZE; i++){
		fifoMainMemory[i] = -1;
	}

	/**
		Initialize Main Memory Array
	**/

	fdOptimalMainMemory = shm_open("/optimalMainMemory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	ftruncate(fdOptimalMainMemory, sizeof(int) * MAIN_MEMORY_SIZE);
	optimalMainMemory = (int *) mmap(NULL, sizeof(int) * MAIN_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdOptimalMainMemory, 0);

	for(int i = 0; i < MAIN_MEMORY_SIZE; i++){
		optimalMainMemory[i] = -1;
	}

	/**
	*
	* MAIN PROGRAM LOGIC
	*
	**/
	
	for(int process = 0; process < NUM_OF_PROCESSES; process++){
		if(fork() == 0){
			unsigned int randSeed = seed + getpid();

			int requestStream[PAGE_REQUEST_STREAM_SIZE];
			generateRequestStream(requestStream, process, randSeed);


			cout << "Process " << process<< " request stream: [";
				for(int j = 0; j < PAGE_REQUEST_STREAM_SIZE; j++){
					cout << requestStream[j] << " ";
				}
			cout << "]" << endl;


			int fifoFaults = 0;
			int optimalFaults = 0;

			cout << endl << "=====\tProcess " << process << " First-In First-Out Policy Swap Trace =====" << endl;

			for(int j = 0; j < PAGE_REQUEST_STREAM_SIZE; j++){
				if(requestPageFIFO(fifoMainMemory, requestStream[j], process)){
					fifoFaults++;
				}
			}

			cout << endl << "=====\tProcess " << process << " Optimal Policy Swap Trace =====" << endl;

			for(int j = 0; j < PAGE_REQUEST_STREAM_SIZE; j++){
				if(requestPageOptimal(optimalMainMemory, requestStream[j], process, requestStream, j)){
					optimalFaults++;
				}
			}

			cout << endl << "FIFO Final Buffer: [";
			for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
				cout << fifoMainMemory[i] << " ";
			}
			cout << "]" << endl;

			cout << endl << "Optimal Policy Final Buffer [";
			for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
				cout << optimalMainMemory[i] << " ";
			}
			cout << "]" << endl;

			cout << "===============================================" << endl;
			cout << "Process " << process << " results: " << endl;
			cout << "FIFO Page Faults: " << fifoFaults << endl;
			cout << "Optimal Policy Page Faults: " << optimalFaults << endl;
			cout << "===============================================" << endl;

			exit(0);
		}
		else{
			wait(NULL);
			
		}
	}



	releaseMemory();
	return 0;
}

/**
*
*FUNCTION DEFINITIONS
*
**/

void generateRequestStream(int arr[], const int process, unsigned int randSeed){
	int last = -1;
	for(int i = 0; i < PAGE_REQUEST_STREAM_SIZE; i++){
		int pageRequest = (rand_r(&randSeed) % PAGES_PER_PROCESS) + 1  + (process * PAGES_PER_PROCESS);
		while(pageRequest == last){
			pageRequest = (rand_r(&randSeed) % PAGES_PER_PROCESS) + 1  + (process * PAGES_PER_PROCESS);
		}
		arr[i] = pageRequest;
		last = pageRequest;
	}
}


/**
* @description: Checks if a page buffer is in a frame for a given process
* 		if it isn't, use a FIFO algorithm to swap a page out for the given page
* 		also, update main memory with the new allocation
* @param mainMemory: 
* @param page: page number requested
* @param process: 
* @return: true if a page fault occured, false otherwise
*/

bool requestPageFIFO(int mainMemory[], const int page, const int process){
	bool didFault = false;
	static int counter = 0; //used to keep track of which frame is to be swapped out for FIFO

	//ensure that the page is not in the frame array already
	bool found = false;


	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		if(mainMemory[i] == page){
			found = true;
			break;
		}
	}

	if(found){
		return didFault;
	}

	//if frame is not found in buffer, attempt to find an empty slot for it

	//find first empty spot
	int spot = -1;
	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		if(mainMemory[i] == -1){
			didFault = false;
			spot = i;
			break;
		}
	}

	//if there is an empty spot, the new page will be placed in it
	//if there was not an empty spot, a page fault has occured
		//use FIFO counter to find a spot for the new page

	if(spot == -1){
		didFault = true;
		cout << getpid() << ":\tFault!" << endl;
		spot = (counter % FRAME_ALLOCATION_SIZE) + (process * FRAME_ALLOCATION_SIZE);
		++counter;
	}

	//update the process' local frame buffer and main memory
	
	cout << getpid() << ": swapping " << mainMemory[spot] << " for " << page << endl;
	mainMemory[spot] = page;


	cout << getpid() << " buffer: [";
		for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
			cout << mainMemory[i] << " ";
		}
	cout << "]" << endl;

	return didFault;
	
}


/**
* @description: Checks if a page buffer is in a frame for a given process
* 		if it isn't, find the page least likely to be referenced in the near future using
* 		optimal policy.
*
* 		Note: In cases where a page wont be requested again, the algorithm replaces it instead
*		If there are multiple pages in the buffer that wont be needed again, the algorithm
* 		swaps out the first page in the buffer that won't be needed again
*
* @param mainMemory: 
* @param page: page number requested
* @param process: 
* @param requestStream[]: 
* @param startIndex: 
* @return: true if a page fault occured, false otherwise
*/


bool requestPageOptimal(int mainMemory[], const int page, const int process, const int requestStream[], const int startIndex){

	bool didFault = false;;

	//ensure that the page is not in the frame array already
	bool found = false;

	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		if(mainMemory[i] == page){
			found = true;
			break;
		}
	}

	if(found){
		return didFault;
	}

	//if frame is not found in buffer, attempt to find an empty slot for it

	//find first empty spot
	int spot = -1;

	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		if(mainMemory[i] == -1){
			didFault = false;
			spot = i;
			break;
		}
	}

	//if there is an empty spot, the new page will be placed in it
	//if there was not an empty spot, a page fault has occured
		//use optimal policy to find a spot for the new page

	if(spot == -1){
		didFault = true;
		cout << getpid() << ":\tFault!" << endl;
		spot = getIndexOfOptimalPolicySwap(optimalMainMemory, requestStream, startIndex, process);
	}

	cout << getpid() << ": swapping " << mainMemory[spot] << " for " << page << endl;
	
	mainMemory[spot] = page;

	cout << getpid() << " buffer: [";
	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		cout << mainMemory[i] << " ";
	}
	cout << "]" << endl;

	return didFault;
}

/**
* @description: given a request stream, uses optimal replacement policy algorithm to
* 		determing which page in process frame buffer to swap out
*
* @param mainMemory: 
* @param requestStream:
* @param startIndex:
* @param process:
* @return: the index of page that should be swapped out for optimal policy
*/

int getIndexOfOptimalPolicySwap(int mainMemory[], const int requestStream[], const int startIndex, const int process){
	//if the buffer isn't full, there is no page to swap out
	bool isFull = true;
	for(int i = (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE; i < j; i++){
		if(mainMemory[i] == -1){
			isFull = false;
			break;
		}
	}

	if(isFull == false){
		return -1;
	}

	//else if the buffer is full, find the page for which the time to the 
	//next request is the longest
	int maxNextRequest;
	int longestPage;
	int longestPageIndex;

	//assume first value in buffer has the longest time to next request
	longestPage = mainMemory[(process * FRAME_ALLOCATION_SIZE)];
	longestPageIndex = (process * FRAME_ALLOCATION_SIZE);
	maxNextRequest = getTimeToNextRequest(requestStream, startIndex, longestPage);


	//search each other value in the buffer to find which has the longest time to next request
	for(int i = 1 + (process * FRAME_ALLOCATION_SIZE), j = i + FRAME_ALLOCATION_SIZE - 1; i < j; i++){
		int t = getTimeToNextRequest(requestStream, startIndex, mainMemory[i]);
		if(t > maxNextRequest){
			maxNextRequest = t;
			longestPage = mainMemory[i];
			longestPageIndex = i;
		}
	}

	return longestPageIndex;
}


/**
* @description: counts the time (in page requests) before a page is requested again
*
* @param requestStream:
* @param startIndex:
* @param page:
* @return: the number of page requests before the given page is requested after startIndex,
* 		or PAGE_REQUEST_STREAM_SIZE, an impossibly high value, if the given page is not 
* 		requested again after startIndex
*/

int getTimeToNextRequest(const int requestStream[], const int startIndex, const int page){
	int timeToNextRequest = PAGE_REQUEST_STREAM_SIZE;
	for(int i = startIndex, t = 0; i < PAGE_REQUEST_STREAM_SIZE; i++, t++){
		if(requestStream[i] == page){
			timeToNextRequest = t;
			break;
		}
	}

	return timeToNextRequest;
}

/**
* @description: Frees OS resources for the entire program on exit
*/

void releaseMemory(){
	shm_unlink("/fifoMainMemory");
	shm_unlink("/optimalMainMemory");
}
