#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <ctype.h>
#include <string>
#include <iostream>
#include <sstream>
#include <errno.h>

# define MAX_PIPE 2500
# define MAX_INPUT 20000
using namespace std;

struct my_pipe {
    int fd[2];
    my_pipe(){
        fd[0] = fd[1] = -1;
    }
};
struct command {
    int num;
    vector<string> parsed_cmd;
    char symbol;
    string filename;
    command(){
        num = 0;
        filename = "";
    }
};
struct child {
    int write_pipe_index;
    int read_pipe_index;
    int err_pipe_index;
    child(){
        write_pipe_index = read_pipe_index = err_pipe_index = -1;
    }
};

void print(const char *output){
    write(1, output, strlen(output));
}
void printline(const char *output){
    write(1, output, strlen(output));
    write(1, "\n", strlen("\n"));
}

void parse_line(string &str_input, vector<string> &parsed_input){
    string new_str_input = "";
    for (int i = 0 ; i < str_input.size() ; i ++) {
        new_str_input += str_input[i];
        if (str_input[i] == '|' || str_input[i] == '!') {
            if (!isdigit(str_input[i+1]))
                new_str_input += '1';
            i ++;
            while (isdigit(str_input[i]))
                new_str_input += str_input[i ++];
            new_str_input += '#';
        }
        else if (str_input[i] == '>') {
            for (i = i + 2 ; i < str_input.size() && str_input[i] != ' ' ; i ++)
                new_str_input += str_input[i];
            new_str_input += '#';
        }     
    }
    
    string temp = "";
    for (int i = 0 ; i < new_str_input.size() ; i ++) {
        if (new_str_input[i] == '#') {
            parsed_input.push_back(temp);
            temp = "";
        }
        else
            temp += new_str_input[i];
    }
    if (temp != "")
        parsed_input.push_back(temp);
    
}
void parse_cmd(command &cmd_temp, string &cmd){
    int flag = 0; // 0 -> cmd, 1 -> num, 2 -> file
    string temp = "";
    cmd_temp.symbol = '?';
    for (int i = 0 ; i < cmd.size() ; i ++) {
        if (cmd[i] == '|' || cmd[i] == '!' || cmd[i] == '>') {
            cmd_temp.symbol = cmd[i];
            if (cmd[i] == '>')
                flag = 2;
            else
                flag = 1;
        }
        else {
            if (!flag)
                temp += cmd[i];
            else if (flag == 1)
                cmd_temp.num = cmd_temp.num * 10 + (cmd[i] - '0');
            else
                cmd_temp.filename += cmd[i];
        }
    }

    istringstream sin(temp);
    while (sin >> temp) 
        cmd_temp.parsed_cmd.push_back(temp);
}
void signalHandler(int signo){
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0);
}
bool check_cmd_exist(char* cmd){
    char *env = getenv("PATH");
    char *split_cmd = strtok(env, ":");
    while(split_cmd != NULL) {
        char file_path[1500] = "", *file_p = file_path;
        strcat(strcat(strcat(file_p, split_cmd), "/"), cmd);
    
        if (access(file_path, F_OK) == 0)
            return true;
        split_cmd = strtok(NULL, ":");
    }

    return false;
}
int main(int argc, char* argv[], char* envp[]){

    signal(SIGCHLD, signalHandler);
    setenv("PATH", "bin:.", 1);

    vector<my_pipe> mpp(MAX_PIPE);
    int mpp_exist_flag[MAX_PIPE] = {};
    int mpp_index = 0;
    int last_pid;

    while(1){
        last_pid = -1;
        print("% ");

        // user_input
        string str_input = "";
        if (!getline(cin, str_input)) {
            puts("");
            exit(0);
        }

        if(str_input == "")
            continue;

        // split line to <command | num>
        vector<string> parsed_input;
        parse_line(str_input, parsed_input);
        int parsed_input_len = parsed_input.size();
        vector<child> chd(parsed_input_len+1);
 
        // pipe of pre_cmd must set first 
        // if pipe of pre_cmd exceed len of now_cmd -> sub len of now_cmd 
        for (int i = 0 ; i < MAX_PIPE - 1000 ; i ++)
            if (mpp_exist_flag[i]) {
                if (mpp_exist_flag[i]-1 < parsed_input_len) {
                    chd[mpp_exist_flag[i]-1].read_pipe_index = i;
                    mpp_exist_flag[i] = MAX_PIPE - 500;
                }
                else
                    mpp_exist_flag[i] -= parsed_input_len;
            }
 
        // exec command
        for (int i = 0 ; i < parsed_input_len ; i ++) {
            // parsing command
            command cmd;
            parse_cmd(cmd, parsed_input[i]);

            // c must use char**, string[] to char**
            char *chr_parsed_cmd[305];
            int x = 0;
            for ( ; x < cmd.parsed_cmd.size() ; x ++) {
                chr_parsed_cmd[x] = strcpy(new char(cmd.parsed_cmd[x].size() + 1), cmd.parsed_cmd[x].c_str());
            }
            chr_parsed_cmd[x] = 0;
            
            // built-in command
            if (strcmp(chr_parsed_cmd[0], "setenv") == 0) {
                setenv(chr_parsed_cmd[1],chr_parsed_cmd[2], 1);
            }         
            else if (strcmp(chr_parsed_cmd[0], "printenv") == 0) {
                char *env = getenv(chr_parsed_cmd[1]);
                if (env)
                    printline(env);
                else
                    printline("invailed env name.");
            }
            else if (strcmp(chr_parsed_cmd[0], "exit") == 0) {
                exit(0);
            }     
            // command use execp
            else {
                // set chd write_fd
                if (cmd.num) {
                    if (cmd.symbol != '>') {
                        if (i + cmd.num >= parsed_input_len) {
                            // if exceed len and next_cmd read_pipe exist, use next_cmd read_pipe
                            int next_cmd_num = i + cmd.num - parsed_input_len + 1;
                            for (int j = 0 ; j < MAX_PIPE - 1000 ; j ++)
                                if (mpp_exist_flag[j] == next_cmd_num) {
                                    chd[i].write_pipe_index = j;
                                    break;
                                }    

                            if (chd[i].write_pipe_index == -1) {
                                // create pipe
                                pipe(mpp[mpp_index].fd);

                                mpp_exist_flag[mpp_index] = next_cmd_num;

                                chd[i].write_pipe_index = mpp_index;

                                // stdout and stderr to same fd
                                if (cmd.symbol == '!') {
                                    chd[i].err_pipe_index = mpp_index;
                                }
                                
                                // find pipe not be used
                                while (mpp_exist_flag[mpp_index])
                                    mpp_index = (mpp_index + 1) % (MAX_PIPE - 1000); 
                            }
                        }
                        else {
                            if (chd[i+cmd.num].read_pipe_index == -1) {
                                // create pipe
                                pipe(mpp[mpp_index].fd);

                                mpp_exist_flag[mpp_index] = MAX_PIPE - 500;

                                chd[i].write_pipe_index = chd[i+cmd.num].read_pipe_index = mpp_index;

                                // stdout and stderr to same fd
                                if (cmd.symbol == '!') 
                                    chd[i].err_pipe_index = mpp_index;
                                
                                // find pipe not be used
                                while (mpp_exist_flag[mpp_index])
                                    mpp_index = (mpp_index + 1) % (MAX_PIPE - 1000);                    
                            }
                            else {
                                chd[i].write_pipe_index = chd[i+cmd.num].read_pipe_index;

                                // stdout and stderr to same fd
                                if (cmd.symbol == '!') 
                                    chd[i].err_pipe_index = chd[i+cmd.num].read_pipe_index;
                            }
                        }      
                    }
                }
                else if (cmd.symbol == '>'){
                    // open file
                    mpp[mpp_index].fd[0] = open(cmd.filename.c_str(), O_RDONLY, 0644);
                    mpp[mpp_index].fd[1] = open(cmd.filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    
                    chd[i].write_pipe_index = mpp_index;
                    mpp_exist_flag[mpp_index] = 1200;

                    // find pipe not be used
                    while (mpp_exist_flag[mpp_index])
                        mpp_index = (mpp_index + 1) % 1000;
                }

                int pid;
                while ((pid = fork()) < 0)
                    usleep(1000);

                if (chd[i].write_pipe_index == -1 || cmd.symbol == '>')
                    last_pid = pid;

                if (pid == 0) {  
                
                    // dup 0,1,2 to my_fd
                    if (chd[i].read_pipe_index != -1)
                        dup2(mpp[chd[i].read_pipe_index].fd[0], STDIN_FILENO);

                    if (chd[i].write_pipe_index != -1) 
                        dup2(mpp[chd[i].write_pipe_index].fd[1], STDOUT_FILENO);
                        
                    if (chd[i].err_pipe_index != -1) 
                        dup2(mpp[chd[i].err_pipe_index].fd[1], STDERR_FILENO);

                    for (int j = 0 ; j < MAX_PIPE - 1000 ; j ++) 
                        if (mpp_exist_flag[j]) {
                            close(mpp[j].fd[0]);
                            close(mpp[j].fd[1]);
                        }
                    if (!check_cmd_exist(chr_parsed_cmd[0])) {
                        string unknown_cmd = "Unknown command: [" + cmd.parsed_cmd[0] + "].\r\n";
                        write(2, unknown_cmd.c_str(), strlen(unknown_cmd.c_str()));
                    }
                    else {
                        execvp(chr_parsed_cmd[0], chr_parsed_cmd);
                    }             
                    exit(0);             
                }
                else {
                    // close pipe of chd_read, except stdin
                    if (chd[i].read_pipe_index != -1) {
                        close(mpp[chd[i].read_pipe_index].fd[1]);
                        close(mpp[chd[i].read_pipe_index].fd[0]);
                        mpp_exist_flag[chd[i].read_pipe_index] = 0;
                    }
                    if (cmd.symbol == '>') {
                        close(mpp[chd[i].write_pipe_index].fd[1]);
                        close(mpp[chd[i].write_pipe_index].fd[0]);
                        mpp_exist_flag[chd[i].write_pipe_index] = 0;
                    }

                }
            }
        }
        // wait last pid write is stdout
        if (last_pid != -1) {
            int status;
            waitpid(last_pid, &status, 0);
        }
    }
}
