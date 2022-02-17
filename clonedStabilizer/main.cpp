#include <iostream>
#include <zconf.h>
#include <sstream>
#include <vector>

using namespace std;

// formula variables
int NUM_CHILDREN = 0;
bool running = true;

// Grab a string like external 30 70 31 and returns the integers in a vector [30,70,31]
vector <int> parseCoreData(string inp){ //https://www.geeksforgeeks.org/extract-integers-string-c/
    vector<int> temperatures;
    stringstream ss;
    ss << inp;

    string temp;
    int found;
    while (!ss.eof()) {
        ss >> temp;
        if (stringstream(temp) >> found) {
            temperatures.push_back(found);
        }
        temp = "";
    }
    return temperatures;
}

void doChildWork(int childNum, int fd[2]) {
    // formula is E = (alpha * E) + ((1 - alpha) * C)
    // Do initialization
    char strArray[200];
    int strArraySize;
    double alphaS;
    double etemp;
    vector <int> temp;
    while (1){
    // read pipe data
    if(read(fd[0], &strArraySize, sizeof(int)) < 0){ // get size of incoming input array
        exit(101);
    }
    if(read(fd[0], strArray, sizeof(char) * strArraySize) < 0){// get actual input into strArray
        exit(101);
    }
    string input(strArray);
    if(input.find("external") != string::npos){
        temp = parseCoreData(input);
        etemp = temp[childNum];
        cout << getpid() << ": Set initial temperature to " << etemp << endl;
    }
    else if(input.find("alpha") != string::npos){
        alphaS = parseCoreData(input)[0];
        // check to see valid value
        if(alphaS < 0 || alphaS > 1){
            cout << "Invalid value for alpha, please provide a value that falls between 0 and 1" << endl;
        } else{
            cout << getpid() << ": set alpha to " << alphaS << endl;
        }
    }
    // close pipes write since child doesnt use it
    close(fd[1]);
    close(fd[0]);
    // Make upstream Pipe
    int upstreamPipe[2];
    pipe(upstreamPipe);
    char upstreamArray[200];
    string upstreamString = to_string(childNum) + "  " + to_string(getpid()) + "  Yes  " + to_string(etemp);
    strcpy(upstreamArray, upstreamString.c_str());
    int upstreamArraySize = strlen(upstreamArray);
    // write size first, then string into pipe
    if (write(upstreamPipe[1], &upstreamArraySize, sizeof(int)) < 0){
     exit(100);
    }
    if (write(upstreamPipe[1], upstreamArray, sizeof(char) * strlen(upstreamArray)) < 0){
        exit(100);
    }

    }
    // Do child cleanup
}

int main() {

    // formula for central process is C = (KC + (sum of all children externals)) / (Number of children + K)
    string input = "";
    double alpha = 0.9;
    double K = 200.0;
    double centralTemp = 0.0;
    vector <int *> pipeArray;
    // parse input
    while (getline(cin, input)){
        if (input == "status"){
            cout << "Alpha = " << alpha << " K = " << K << endl;
            cout << "Central temp is " << centralTemp << endl;
            continue;
        }
        else if (input.find("ctemp") != string::npos){
            centralTemp = parseCoreData(input)[0];
            cout << "Central Temp is now " << centralTemp << endl;
            continue;
        }
        else if (input.find("external") != string::npos){
            NUM_CHILDREN = parseCoreData(input).size();
        }
        else if(input.find("alpha") != string::npos){
            // continue down to child, this is taken care of there
        }
        else if (input.find("start") != string::npos){
            //start temperature diffusion
        }
        else if (input.find("quit") != string::npos){
            break; // USE SIG HANDLER STUFF HERE
        }
        else {
            cout << "Invalid input, please try again." << endl;
            return 2; // 2 represents invalid input
        }


        for (int k = 0; k < NUM_CHILDREN; k++) {
            pid_t whoami = fork();
            if (whoami == 0) {
                int fd[2];
                pipe(fd);
                pipeArray.push_back(fd);
                //pipeArray.push_back(fd);
                // place input into pipe
                char strArray[200];
                strcpy(strArray, input.c_str());
                int strArraySize = strlen(strArray);
                write(fd[1], &strArraySize, sizeof(int));
                if (write(fd[1], strArray, sizeof(char) * strlen(strArray)) < 0){
                    return 100;
                }
                close(fd[1]); // close writing end of pipe
                doChildWork(k, fd);
                exit(1); // Prevent parent code from accidentally gets executed by child
            } else {
                // read from pipes that child just sent back up
                /* more code for parent here */
            }
        }

        /* more code for parent below this line */

        for (int k = 0; k < NUM_CHILDREN; k++) {
            int child_status;
            pid_t which_child = wait(&child_status);
        }
        /* more code for parent below here */
    }
}