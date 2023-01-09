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
                            // assuming message is most recent event, as was said on forums
                            int g_index = answer.find("ion:/") + 5;
                            int u_index = answer.find("user:") + 5;
                            string g = answer.substr(g_index, answer.find(" ", g_index) - g_index);
                            string g_user = answer.substr(u_index, answer.find("\n", u_index) - u_index);
                            std::pair<string,string> p = make_pair(g_user, g);
                            Game toUpdate(g.substr(0, g.find("_")), g.substr(g.find("_") + 1, g.length() - (g.find("_") + 1)));
                            if(records.count(p) > 0){
                                toUpdate = records.at(p);
                            }
                            int etime = stoi(answer.substr(answer.find("time:") + 6, answer.find("\n",answer.find("time:"))));
                            if (answer.find("goals:") != std::string::npos) { // updating goals
                                int index = answer.find("goals:") + 7;
                                int goals = stoi(answer.substr(index, answer.find("\n", index) - index));
                                if((unsigned) index < answer.find("team_b_updates:")){
                                    toUpdate.set_a_goals(goals);
                                    if (answer.find("goals:", index) != std::string::npos) {
                                        int index2 = answer.find("goals:",index) + 7;
                                        goals = stoi(answer.substr(index2, answer.find("\n", index2) - index2));
                                        toUpdate.set_b_goals(goals);
                                    }
                                }
                                else{
                                    toUpdate.set_b_goals(goals);
                                }
                            }
                            if (answer.find("possession:") != std::string::npos) { // updating possession
                                int index = answer.find("possession:") + 12;
                                int pos = stoi(answer.substr(index, answer.find("%", index) - index));
                                if((unsigned) index < answer.find("team_b_updates:")){
                                    toUpdate.set_a_possession(pos);
                                    if (answer.find("possession:", index) != std::string::npos) {
                                        int index2 = answer.find("possession:",index) + 12;
                                        pos = stoi(answer.substr(index2, answer.find("%", index2) - index2));
                                        toUpdate.set_b_possession(pos);
                                    }
                                }
                                else{
                                    toUpdate.set_b_possession(pos);
                                }
                            }
                            if((answer.find("before halftime: false") != std::string::npos)){
                                toUpdate.setBeforeHalf(false);
                            }

                            int des_index = answer.find("description :\n") + 15;
                            int null_index = answer.find('\0');
                            string details = answer.substr(des_index, null_index - 1 - des_index);
                            int name_index = answer.find("event name : ") + 13;
                            string name = answer.substr(answer.find(" ", name_index) + 1, answer.find("\n", name_index) - answer.find(" ", name_index));
                            toUpdate.addDetail(etime + " - " + name + ":\n\n" + details + "\n\n\n");
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
                int ctime=-1;
                Game temp(toSend.team_a_name, toSend.team_b_name);
                if(records.count(std::make_pair(user ,toSend.team_a_name + "_" + toSend.team_b_name))){
                    temp = records.at(std::make_pair(user ,toSend.team_a_name + "_" + toSend.team_b_name));
                    ctime = temp.getTime();
                }
                bool update = false;
            for(Event e : toSend.events){
                temp.addDetail(std::to_string(e.get_time()) + " - " + e.get_name() + ":\n\n" + e.get_discription() + "\n\n\n");
                if(e.get_time() > ctime){                    
                    update = true;
                }
                if(((temp.beforeHalf() && e.get_game_updates().count("before halftime") > 0) && e.get_game_updates().at("before halftime") == "false")){
                    update = true;
                    temp.setBeforeHalf(false);
                }
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
                    if(update){
                        if(p.first == "active"){
                            bool t;
                            if(p.second == "true"){
                                t = true;}
                            else {t = false;}
                            temp.setActive(t);
                        }
                        else if(p.first == "before halftime"){
                            bool t;
                            if(p.second == "true"){
                                t = true;}
                            else {t = false;}
                            temp.setBeforeHalf(t);
                        }
                    }
                }
                input.append("team a updates:\n");
                for(std::pair<string,string> p : e.get_team_a_updates()){
                    input.append("    " + p.first + ": " + p.second + "\n");
                    if(update){
                        if(p.first == "goal"){
                            temp.set_a_goals(stoi(p.second));
                        }
                        else if(p.first == "possession"){
                            temp.set_a_possession(stoi(p.second)); // need to check that they don't add %
                        }
                    }
                }
                input.append("team b updates:\n");
                for(std::pair<string,string> p : e.get_team_b_updates()){
                    input.append("    " + p.first + ": " + p.second + "\n");
                    if(update){
                        if(p.first == "goal"){
                            temp.set_b_goals(stoi(p.second));
                        }
                        else if(p.first == "possession"){
                            temp.set_b_possession(stoi(p.second)); // need to check that they don't add %
                        }
                    }
                }
                input.append("description:\n");
                input.append(e.get_discription() + "\n");
                scHandler -> sendFrameAscii(input,'\0');
                update = false;
                records.insert(std::make_pair(std::make_pair(user, e.get_team_a_name() + "_" + e.get_team_b_name()), temp));
                // save game to a local array
            }
            }
            else{
                std::cout << "you are not subscribed to the topic" << std::endl;
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
            outFile << "goals: " + std::to_string(records.at(key).a_goals()) + "\n";
            outFile << "possession: " + std::to_string(records.at(key).a_possession()) + "%\n";
            outFile << records.at(key).bName() + " stats:\n";
            outFile << "goals: " + std::to_string(records.at(key).b_goals()) + "\n";
            outFile << "possession: " + std::to_string(records.at(key).b_possession()) + "%\n";
            outFile << "Game event reports:\n";
            for(int i = 0; (unsigned) i < records.at(key).getDetails().size(); i++){
                outFile << records.at(key).getDetails().at(i);
            }
            outFile.close();
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