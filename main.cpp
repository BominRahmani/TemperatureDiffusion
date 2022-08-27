#include <iostream>
#include <zconf.h>
#include <sstream>
#include <vector>
#include <numeric>
#include <signal.h>

using namespace std;

int NUM_CHILDREN = 0;
int NUM_RUNNING_CHILDREN = 0;
bool cutChild = false;
bool running = true; //children running variable.
bool hasStarted = false; // start calculations running.

void signalHandler(int num) {
    running = false;
}

// Grab a string like external 30 70 31 and returns the integers in a vector [30,70,31]
vector<double> parseCoreData(string inp) { //https://www.geeksforgeeks.org/extract-integers-string-c/
    vector<double> temperatures;
    stringstream ss;
    ss << inp;

    string temp;
    double found;
    while (!ss.eof()) {
        ss >> temp;
        if (stringstream(temp) >> found) {
            temperatures.push_back(found);
        }
        temp = "";
    }
    return temperatures;
}

string parseId(string input){
    int end = input.size();
    for (; end >= 0; --end) {
        if (input[end] == ' ') {
            break;
        }
    }

    input = input.substr(end + 1, input.size() - 1);
    return input;
}

double parseExternalTemperatureData(string inp) {
    int end = inp.size();
    for (; end >= 0; --end) {
        if (inp[end] == ' ') {
            break;
        }
    }

    inp = inp.substr(end, inp.size() - 1);

    // retrieve the value:
    double temperature;
    std::istringstream(inp) >> temperature;
    return temperature;
}

// this method essentially scours a string and returns the double value within it
////https://stackoverflow.com/questions/15498462/extracting-a-double-from-a-string-with-text
double parseDoubleData(string inp) {
    int start = 0;
    for (; start < inp.size(); ++start) {
        if (std::isdigit(inp[start])) break;
    }
    inp = inp.substr(start, inp.size() - start);

    // retrieve the value:
    double d;
    std::istringstream(inp) >> d;
    return d;
}

double sumCores(vector<double> coreList) {
    double answer = 0.0;
    for (int i = 0; i < coreList.size(); ++i) {
        if (coreList[i] == -1337.2){
            NUM_RUNNING_CHILDREN--;
            continue;
        }
        else{
            answer += coreList[i];
        }
    }
    return answer;
}

void doChildWork(int childNum, int down[2], int up[2]) {
    // formula is E = (alpha * E) + ((1 - alpha) * C)
    // Do initialization
    string childString = to_string(childNum);
    char strArray[200];
    double alphaS;
    int numRead;
    bool enabled = true;
    double etemp;
    vector<double> temp;
    while (running) {
        int strArraySize;
        // read pipe data
        numRead = read(down[0], &strArraySize, sizeof(int));
        if (numRead < 0) { // get size of incoming input array READS FROM PARENT
            exit(101);
        }
        if (numRead == 0) {
            continue;
        }
        if (read(down[0], strArray, sizeof(char) * strArraySize) < 0) {// get actual input into strArray
            exit(101);
        }
        string input(strArray);

        //handle parsing and doing child work
        if (input.find("external") != string::npos) {
            temp = parseCoreData(input);
            etemp = temp[childNum];
            cout << getpid() << ": Set initial temperature to " << etemp << endl;
        } else if (input.find("quit") != string::npos) {
            cout << getpid() << " is shutting down" << endl;
            break;
        } else if (input.find("alpha") != string::npos) {
            alphaS = parseDoubleData(input);
            // check to see valid value
            if (alphaS < 0 || alphaS > 1) {
                cout << "Invalid value for alpha, please provide a value that falls between 0 and 1" << endl;
            } else {
                cout << getpid() << ": set alpha to " << alphaS << endl;
            }
        } else if (input.find("etemp") != string::npos) {
            //set external temp of a certain process (will carry two elements)
            vector<double> coreTemps;
            coreTemps = parseCoreData(input);
            if (coreTemps.size() < 2) {
                cout << "Not enough information. Please provide a process Id and the temperature." << endl;
            } else {
                if (coreTemps[0] == getpid()) {
                    etemp = parseExternalTemperatureData(input);
                    cout << getpid() << ": Ext. temperature set to " << etemp << endl;
                }
            }
        }
        else if (input.find("disable") != string::npos) {
            if (parseId(input) == to_string(getpid())) {
                cout << getpid() << " Has been disabled" << endl;
                enabled = false;
            }
        }
        else if (input.find("enable") != string::npos) {
            if (parseId(input) == to_string(getpid())) {
                cout << getpid() << " Has been enabled" << endl;
                enabled = true;
            }
        }
        else if (input.find("status") != string::npos) {
            string response = "  Yes     ";
            if(!enabled) {
                response = "  No      ";
            }
            string upstreamString = childString + "  " + to_string(getpid()) + response + to_string(etemp);
            cout << upstreamString << endl;
        } else if (input.find("start") != string::npos) {
            if (!enabled) {
                double disabledCode = -1337.2;
                if (write(up[1], &disabledCode, sizeof(double)) < 0) {
                    exit(100);
                }
            }
            else {
                // start the calculations
                // all start inputs follow the convention "start [etemp]" where etemp is replaced by the double
                etemp = (alphaS * etemp) + ((1.0 - alphaS) * parseDoubleData(input));
                // send back etemp data
                if (write(up[1], &etemp, sizeof(double)) < 0) {
                    exit(100);
                }
            }
        }
        // clear array
        memset(strArray, 0, 200);
    }
    // close pipes write since child doesnt use it
    close(down[1]);
    close(up[0]);
}

int main() {
    signal(SIGUSR1, signalHandler);
    // formula for central process is C = (KC + (sum of all children externals)) / (Number of children + K)
    string input = "";
    double alpha = 0.9;
    double K = 200.0;
    double centralTemp = 0.0;
    int stabilizationCount = 0;
    double lastCentralTemp = 0.0;
    int delay = 1;
    vector<double> initialElements;
    pid_t *processIds;
    bool hasQuit = false;

    int up[10][2];
    int down[10][2];
    // parse input
    while (hasStarted || getline(cin, input)) {
        if (input == "status") {
            cout << "Alpha = " << alpha << " K = " << K << endl;
            cout << "Central temp is " << (double) centralTemp << endl;
            if (NUM_CHILDREN > 0) {
                // let this get handled by child process.
                cout << "#  PID  Enabled  Temperature" << endl;
                cout << "--------------------------------" << endl;
            } else {
                continue;
            }
        } else if (input.find("ctemp") != string::npos) {
            centralTemp = parseCoreData(input)[0];
            cout << "Central Temp is now " << centralTemp << endl;
            lastCentralTemp = centralTemp;
            continue;
        } else if (input.find("external") != string::npos) {
            NUM_CHILDREN = parseCoreData(input).size();
            NUM_RUNNING_CHILDREN = NUM_CHILDREN;
            processIds = (pid_t *) malloc(NUM_CHILDREN * sizeof(pid_t));
            for (int k = 0; k < NUM_CHILDREN; k++) {
                pipe(down[k]);
                pipe(up[k]);
                char strArray[200];
                strcpy(strArray, input.c_str());
                int strArraySize = strlen(strArray);
                //since i am never reading in down, i can just close it
                if (write(down[k][1], &strArraySize, sizeof(int)) < 0) {
                    return 600;
                }
                if (write(down[k][1], strArray, sizeof(char) * strlen(strArray)) <
                    0) {  // this is parent writing to child
                    return 100;
                }
                pid_t whoami = fork();
                if (whoami == 0) {
                    processIds[k] = getpid();
                    doChildWork(k, down[k], up[k]);
                    exit(1); // Prevent parent code from accidentally gets executed by child
                } else {
                    close(down[k][0]);
                    close(up[k][1]);
                    // read from pipes that child just sent back up
                }
            }
        } else if (input.find("alpha") != string::npos) {

        } else if (input.find("start") != string::npos) {
            //start temperature diffusion
            hasStarted = true;
        } else if (input.find("quit") != string::npos) {
            for (int i = 0; i < NUM_CHILDREN; ++i) {
                kill(processIds[i], SIGUSR1);
            }
            hasQuit = true;
        } else if (input.find("etemp") != string::npos) {
            //handled in child
        } else if (input.find("delay") != string::npos) {
            delay = (int) parseCoreData(input)[0];
        }
        else if (input.find("disable") != string::npos) {
            //handled in child
        }
        else if (input.find("enable") != string::npos) {
            //handled in child
        }
        else {
            cout << "Invalid input, please try again." << endl;
            continue;
        }

        double temperatureReport;
        if (NUM_CHILDREN > 0 && input.find("external") == string::npos) {
            for (int i = 0; i < NUM_CHILDREN; ++i) {
                // if input is start, change it to send central temperature as well
                if (hasStarted) {
                    input = "start " + to_string(centralTemp);
                }
                // write to each pipe the value of alpha
                char strArray[200];
                strcpy(strArray, input.c_str());
                int strArraySize = strlen(strArray);
                write(down[i][1], &strArraySize, sizeof(int));
                if (write(down[i][1], strArray, sizeof(char) * strlen(strArray)) < 0) {
                    return 100;
                }
                if (hasStarted) {
                    sleep(delay);
                    read(up[i][0], &temperatureReport, sizeof(double));
                    initialElements.push_back(temperatureReport);
                }
            }
        }

        // report findings
        if (hasStarted) {
            // get sum for central core
            double sumOfCores = sumCores(initialElements);
            centralTemp = ((double) ((K * centralTemp) + sumOfCores)) / ((double) (NUM_RUNNING_CHILDREN + K));
            cout << "Temperature [" << centralTemp << "] ";
            for (int i = 0; i < initialElements.size(); ++i) {
                if (initialElements[i] != -1337.2){
                    cout << initialElements[i] << " ";
                }
            }
            cout << endl;
            initialElements.clear();
            NUM_RUNNING_CHILDREN = NUM_CHILDREN;
            if (abs(lastCentralTemp - centralTemp) <= 0.01) {
                stabilizationCount++;
            }
            if (stabilizationCount == 4) {
                hasStarted = false;
                stabilizationCount = 0;
                cout << "The system stabilized at: " << centralTemp << endl;
            }
            lastCentralTemp = centralTemp;
        }

        if(hasQuit){
            break;
        }
    }
    return 0;
}