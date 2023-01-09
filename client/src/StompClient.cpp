    #include <stdlib.h>
    #include "../include/StompConnectionHandler.h"
    #include <thread>
    #include "../include/Subscriptions.h"
    #include <algorithm>
    #include <iostream>
    #include <fstream>
    #include "../include/game.h"
    #include "../include/event.h"

    using namespace std;

    void commandToFrame(string& input);
    bool startsWith(const string& command, const string& start);
    void processLogin(string& command);
    void handleCommands();

    
    
    static int nextId = 1;
    static short port;
    static string user = "";
    static string connectFrame = "";
    static string host = "";
    static bool loggedIn = false;
    static bool changeServer = false;
    static bool shouldTerminate = false;
    static vector<int> logoutIds;
    static Subscriptions subs;
    static std::map<std::pair<string, string>, Game> records; // mapping user,gamename > game
    static StompConnectionHandler* scHandler;

        int main() {
        thread t1(handleCommands);
        while(!shouldTerminate){
            if(changeServer){
                StompConnectionHandler tempHandler(host, port);
                scHandler = &tempHandler; // updating stompConnectionHandler
                if (!scHandler -> connect()) {
                    std::cerr << "Could not connect to the server." << std::endl;       
                }
                else {
                        scHandler -> sendFrame(connectFrame); // sending connect frame
                        string answer;
                        if(scHandler -> getFrameAscii(answer, '\0')){ // handle connected/error frame, update loggedin
                            if(startsWith(answer, "CONNECTED")){
                                loggedIn = true;
                                std::cout << "Login successful" << std::endl;
                            }
                            else if(startsWith(answer, "ERROR")){
                                // returns the message inside the error frame
                                std::cout << answer.substr(answer.find("message:") + 8, answer.find("\n", answer.find("message:"))) << std::endl;
                            }
                        }
                }
                changeServer = false;
            }
            else if(loggedIn){
                // handle frame
                string answer;
                if(scHandler -> getFrameAscii(answer, '\0')){
                    if(startsWith(answer, "RECEIPT")){
                        int id = stoi(answer.substr(answer.find("id:") + 3, answer.find("\n")));
                        if(std::find(logoutIds.begin(), logoutIds.end(), id) != logoutIds.end()){ // logging out, removing all user data
                            scHandler -> close();
                            nextId = 1;
                            subs.clear();
                            logoutIds.clear();
                            records.clear();
                            loggedIn = false;
                        }
                        else if(id < 0){
                            id = id * (-1);
                            string ch = subs.getGamebyId(id);
                            subs.removeSubscription(id);
                            std::cout << "Exited channel " + ch << std::endl;
                        }
                        else if(id > 0){
                            subs.activateSubscription(id);
                            std::cout << "Joined channel " + subs.getGamebyId(id) << std::endl;
                        }
                    }
                    else if(startsWith(answer, "MESSAGE")){
                        
                    }
                    else if(startsWith(answer, "ERROR")){
                        std::cout << answer.substr(answer.find("message:") + 8, answer.find("\n", answer.find("message:"))) << std::endl;
                        scHandler -> close();
                        nextId = 1;
                        subs.clear();
                        logoutIds.clear();
                        records.clear();
                        loggedIn = false;
                    }
                }
            }
        }
        delete scHandler;
        return 0;
    }

    void handleCommands()
    {
        bool terminate = false;
        //cout << "keyboard listening" << endl;
        while(!terminate){
            // receiving user input
            const short bufsize = 1024;
            char buf[bufsize];
            cin.getline(buf, bufsize);
            string line(buf);

            // handling login command
            if(!loggedIn && !startsWith(line, "login")) {
                std::cerr << "Please first log-in to continue." << std::endl;
            }
            else if(startsWith(line,"login")) {
                if(!loggedIn){
                    processLogin(line);
                    changeServer = true;
                }
                else std::cout << "The client is already logged in" << std::endl;
            } // handling other commands
            else {
                commandToFrame(line);
                if(line != ""){
                    scHandler -> sendFrameAscii(line,'\0');
                    line = "";
                }
            }
        }
    }

    void commandToSubscribe(string& input);
    void commandToUnSubscribe(string& input);
    void commandToSend(string& input);
    void commandToSummarize(string& input);
    void commandToLogout(string& input);

    void commandToFrame(string& input) {
        if(startsWith(input, "join")) commandToSubscribe(input);
        else if(startsWith(input, "exit")) commandToUnSubscribe(input);
        else if(startsWith(input, "report")) commandToSend(input);
        else if(startsWith(input, "summary")) commandToSummarize(input);
        else if(startsWith(input, "logout")) commandToLogout(input);
        else input = "";
    }

    void commandToSubscribe(string& input) {
        string temp = "SUBSCRIBE\n";
        temp.append("destination:/" + input.substr(5) + "\n");
        temp.append("id:" + std::to_string(nextId) + "\n");
        temp.append("receipt:" + std::to_string(nextId) + "\n\n");
        subs.addPendSubscription(input.substr(5), nextId);
        input = temp;
        nextId = nextId + 1;
    }

    void commandToUnSubscribe(string& input) {
        string game = input.substr(5);
        int id = subs.getIdbyGame(game);
        input = "UNSUBSCRIBE\n";
        input.append("id:" + std::to_string(id) + "\n");
        input.append("receipt:" + std::to_string(-1 * id) + "\n\n");
    }

    void commandToSend(string& input) {
        names_and_events toSend = parseEventsFile(input.substr(7));
        if(subs.contains(toSend.team_a_name + "_" + toSend.team_b_name)){

        }
        for(Event e : toSend.events){
            input = "SEND\n";          
            input.append("destination:/" + e.get_team_a_name() + "_" + e.get_team_b_name() + "\n\n");
            input.append("user: " + user + "\n");
            input.append("team a: " + e.get_team_a_name() + "\n");
            input.append("team b: " + e.get_team_b_name() + "\n");
            input.append("event name: " + e.get_name() + "\n");
            input.append("time: " + std::to_string(e.get_time()) + "\n");
            input.append("general game updates:\n");
            for(std::pair<string,string> p : e.get_game_updates()){
                input.append("    " + p.first + ": " + p.second + "\n");
            }
            input.append("team a updates:\n");
            for(std::pair<string,string> p : e.get_team_a_updates()){
                input.append("    " + p.first + ": " + p.second + "\n");
            }
            input.append("team b updates:\n");
            for(std::pair<string,string> p : e.get_team_b_updates()){
                input.append("    " + p.first + ": " + p.second + "\n");
            }
            input.append("description:\n");
            input.append(e.get_discription() + "\n");
            scHandler -> sendFrameAscii(input,'\0');
            // save game to a local array
        }
        input = "";
    }

    void commandToSummarize(string& input) {
        int space1 = input.find(" ");
        int space2 = input.find(" ", space1 + 1);
        int space3 = input.find(" ", space2 + 1);
        string game = input.substr(space1 + 1, space2 - (space1 + 1));
        string uname = input.substr(space2 + 1, space3 - (space2 + 1));
        string filename = input.substr(space3 + 1);
        ofstream outFile(filename);
        std::pair<string,string> key(uname,game);
        outFile << game.substr(0, game.find("_")) + " vs " + game.substr(game.find("_") + 1) + "\n";
        outFile << "Game stats:\n";
        outFile << "General stats:\n";
        outFile << "Game stats:\n";
        outFile << "active: " + std::to_string(records.at(key).isActive()) + "\n";
        outFile << "before halftime: " + std::to_string(records.at(key).beforeHalf()) + "\n";
        outFile << records.at(key).aName() + " stats:\n";
        //outFile << "Files can be tricky, but it is fun enough!";
        //outFile.close();
        input = "";
    }

    void commandToLogout(string& input) {
        if(!loggedIn) input = "";
        else {
            input = "DISCONNECT\n";
            input.append("receipt:" + std::to_string(nextId) + "\n\n");
            logoutIds.push_back(nextId);
            nextId = nextId + 1;
        }
    }

    bool startsWith(const string& command, const string& start) {
        for (int i = 0; (unsigned)i < start.length(); i++) {
            if(command[i] != start[i]){
                return false;
            }
        }
        return true;
    }

    void processLogin(string& command) {
        vector<string> temp;
        int to = command.find(":");
        temp.push_back(command.substr(6,to - 6)); // pushing ip
        command = command.substr(to+1); // working with the string without login host:
        to = command.find(" "); 
        temp.push_back(command.substr(0, to)); // pushing port
        command = command.substr(to+1); // working with the string without the login, ip:port
        to = command.find(" ");
        temp.push_back(command.substr(0, to)); // pushing username
        command = command.substr(to+1); // string is just password now
        temp.push_back(command);
        host = temp.at(0); // changing host and port static values
        port = atoi(temp.at(1).c_str());
        connectFrame = "CONNECT\n"; // updating connectFrame
        connectFrame.append("accept-version:1.2\n");
        connectFrame.append("host:stomp.cs.bgu.ac.il\n");
        connectFrame.append("login:" + temp.at(2) + "\n");
        user = temp.at(2);
        connectFrame.append("passcode:" + temp.at(3) + "\n\n");
    }