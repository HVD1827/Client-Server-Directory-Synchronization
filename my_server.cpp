#include <bits/stdc++.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <filesystem>
#include <fcntl.h>    // For O_RDONLY
#include <sys/stat.h>

using namespace std;

#define EVENT_SIZE (sizeof(struct inotify_event)) 
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

struct Client_store{
    int sock;
    set<string> ignore_list;
};

map<int, Client_store> all_clients;
pthread_mutex_t mymutex = PTHREAD_MUTEX_INITIALIZER;
int cnt;
map<int, string> wd_to_path;

string find_extension(string file_name){
    if(file_name.find('.') == string::npos){
        return "";
    }

    string extension = "";
    for(int i = file_name.size() - 1; i >= 0; i--){
        if(file_name[i] == '.'){
            break;
        }
        extension = file_name[i] + extension;
    }
    extension="."+extension;
    return extension;
}

#define SIZE 1024

string write_file(int sockfd)
{
    int n; 
    
    char buffer[SIZE];
    string s="";
    
    while(1)
    {
        n = recv(sockfd, buffer, SIZE, 0);
        if(n<=0)
        {
            break;
            return s;
        }
        for(int i=0; i<n; i++){
            s += buffer[i];
        }
        bzero(buffer, SIZE);
    }
    cout<<s<<endl;
    return s;
    
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);

    Client_store client;
    client.sock = client_fd;
    
    // Read initial message (should be "client")
    char init_msg[10] = {0};
    recv(client_fd, init_msg, 6, 0); // Read 6 bytes ("client")
    init_msg[6] = '\0';
    cout << "Initial message: " << init_msg << endl;

    // Now read the ignore list with proper termination
    string fulllist="";
    char buffer[SIZE];
    bool end_marker_found = false;
    
    while (!end_marker_found) {
        int n = recv(client_fd, buffer, SIZE, 0);
        if (n <= 0) {
            cout << "client disconnec hogaya.clientfd= " << client_fd <<endl; 
            cnt=cnt-1;
            break;
        }
        // Check for our end marker in the received data
        string chunk(buffer, n);
        cout << "Received chunk: " << chunk << endl;
        size_t end_pos = chunk.find("ENDOFIGNORELIST");
        if (end_pos != string::npos) {
            fulllist.append(chunk.substr(0, end_pos));
            end_marker_found = true;
        } else {
            fulllist.append(chunk);
        }
    }


    cout << "Received ignore list: " << fulllist << endl;

    // Parse the ignore list (comma separated)
    size_t pos = 0;
    while ((pos = fulllist.find(',')) != string::npos) {
        string token = fulllist.substr(0, pos);
        if (!token.empty()) {
            client.ignore_list.insert(token);
        }
        fulllist.erase(0, pos + 1);
    }
    // Add the last item
    if (!fulllist.empty()) {
        client.ignore_list.insert(fulllist);
    }

    pthread_mutex_lock(&mymutex);
    all_clients[client_fd] = client;
    pthread_mutex_unlock(&mymutex);
    cout<<"Client added"<<endl;


    return nullptr;
}

void send_file(int sock, const std::string &file_name) {
    struct stat file_stat;
    if (stat(file_name.c_str(), &file_stat) < 0) {
        perror("Error getting file size");
        return;
    }

    long unsigned int file_size = file_stat.st_size;
    char header[21];  // Increased size
    string sizeoffile = to_string(file_size);
    snprintf(header, sizeof(header), "DATA______%010lu", file_size);

    if (send(sock, header, sizeof(header), 0) < 0) {
        perror("Error sending file header");
        return;
    }

    int file_fd = open(file_name.c_str(), O_RDONLY);
    if (file_fd == -1) {
        perror("Error opening file for sending");
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = 0;
        send(sock, buffer, bytes_read, 0);
    }

    close(file_fd);
}

void *directory_watcher(void *arg) {
    string sync_directory = *((string *)arg);
    int inotify_fd = inotify_init();

    if (inotify_fd < 0) {
        cout << "Error in inotify_init" << endl;
        return nullptr;
    }

    int watch_fd = inotify_add_watch(inotify_fd, sync_directory.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if(watch_fd>=0){
        wd_to_path[watch_fd] = sync_directory;
    }
    std::filesystem::recursive_directory_iterator iterptr(sync_directory);
    for (auto &entry : iterptr) {
        if (entry.is_directory()) {
            int newwatch= inotify_add_watch(inotify_fd, entry.path().c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
            wd_to_path[newwatch] = entry.path().string();
        }
    }

    char buffer[EVENT_BUF_LEN];

    while (1) {

        //printing wd_to_path
        cout<<"wd_to_path"<<endl;
        for(auto it = wd_to_path.begin(); it != wd_to_path.end(); it++){
            cout<<it->first<<" "<<it->second<<endl;
        }

        int buffer_length = read(inotify_fd, buffer, EVENT_BUF_LEN);
        if (buffer_length < 0) {
            cout << "Error in read" << endl;
            return nullptr;
        }

        int iter = 0;

        while (iter < buffer_length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[iter];
            if (event->len > 0) {
                cout<<wd_to_path[event->wd]<<endl;
                string absolute_path = wd_to_path[event->wd] + "/" + event->name;
                string relative_path = absolute_path.substr(sync_directory.length() + 1);
                string event_type = "";

                cout<<"event of interest "<< absolute_path<<endl;

                if (event->mask & IN_CREATE) event_type = "CREATE";
                if (event->mask & IN_DELETE) event_type = "DELETE";
                if (event->mask & IN_MOVED_TO) event_type = "CREATE";
                if (event->mask & IN_MOVED_FROM) event_type = "DELETE";

                bool is_directory = (event->mask & IN_ISDIR);

                pthread_mutex_lock(&mymutex);
                for (auto &client : all_clients) {
                    if (is_directory) {
                        if (event_type == "CREATE") {
                            // Add watch for newly created directory and its subdirectories
                            int newwatch=inotify_add_watch(inotify_fd, absolute_path.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
                            wd_to_path[newwatch] = absolute_path;
                            for (const auto &subentry : std::filesystem::recursive_directory_iterator(absolute_path)) {
                                if (subentry.is_directory()) {
                                    int tempwatch= inotify_add_watch(inotify_fd, subentry.path().c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
                                    wd_to_path[tempwatch] = subentry.path().string();
                                }
                            }

                            // Send CREATE_DIR message
                            char create_msg[1024];
                            snprintf(create_msg, sizeof(create_msg), "CREATE_DIR%010lu%s", relative_path.length(), relative_path.c_str());
                            cout<<"Sending create message"<<endl;
                            cout<<create_msg<<endl;
                            send(client.second.sock, create_msg, strlen(create_msg), 0);

                            // Send CREATE messages for all files inside the directory
                            for (const auto &subentry : std::filesystem::recursive_directory_iterator(absolute_path)) {
                                if (!subentry.is_directory()) {
                                    string file_path = subentry.path().string();
                                    string file_rel_path = file_path.substr(sync_directory.length() + 1);
                                    string extension = find_extension(file_path);

                                    if (client.second.ignore_list.find(extension) == client.second.ignore_list.end()) {
                                        char file_create_msg[1024];
                                        snprintf(file_create_msg, sizeof(file_create_msg), "CREATE____%010lu%s", file_rel_path.length(), file_rel_path.c_str());
                                        cout<<"Sending file create message"<<endl;
                                        cout<<file_create_msg<<endl;
                                        send(client.second.sock, file_create_msg, strlen(file_create_msg), 0);
                                        send_file(client.second.sock, file_path);
                                    }
                                }
                                else{
                                    string dir_path = subentry.path().string();
                                    string dir_rel_path = dir_path.substr(sync_directory.length() + 1);
                                    char dir_create_msg[1024];
                                    snprintf(dir_create_msg, sizeof(dir_create_msg), "CREATE_DIR%010lu%s", dir_rel_path.length(), dir_rel_path.c_str());
                                    cout<<"Sending create message"<<endl;
                                    cout<<dir_create_msg<<endl;
                                    send(client.second.sock, dir_create_msg, strlen(dir_create_msg), 0);

                                }

                            }
                        } else {  // Handle directory deletion
                            char delete_msg[1024];
                            snprintf(delete_msg, sizeof(delete_msg), "DELETE_DIR%010lu%s", relative_path.length(), relative_path.c_str());
                            cout<<"Sending delete message"<<endl;
                            cout<<delete_msg<<endl;
                            send(client.second.sock, delete_msg, strlen(delete_msg), 0);
                        }
                    } else {  // Handle files
                        string file_extension = find_extension(absolute_path);
                        if (client.second.ignore_list.find(file_extension) == client.second.ignore_list.end()) {
                            if (event_type == "CREATE") {
                                char create_msg[1024];
                                snprintf(create_msg, sizeof(create_msg), "CREATE____%010lu%s", relative_path.length(), relative_path.c_str());
                                cout<<"Sending create message"<<endl;
                                cout<<create_msg<<endl;
                                send(client.second.sock, create_msg, strlen(create_msg), 0);
                                send_file(client.second.sock, absolute_path);
                            } else {
                                char delete_msg[1024];
                                snprintf(delete_msg, sizeof(delete_msg), "DELETE____%010lu%s", relative_path.length(), relative_path.c_str());
                                cout<<"Sending delete message"<<endl;
                                cout<<delete_msg<<endl;
                                send(client.second.sock, delete_msg, strlen(delete_msg), 0);
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&mymutex);
            }
            iter += EVENT_SIZE + event->len;
        }
    }
}



int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    string directoryarg = argv[1];
    int port = stoi(argv[2]);
    int max_clients = stoi(argv[3]);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);


    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd , max_clients);

    pthread_t look_for_changes;
    pthread_create(&look_for_changes, nullptr, directory_watcher, &directoryarg);
     cnt = 0;
    while (true) {
        if (cnt >= max_clients) {
            continue;
        }

        client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("Error accepting client connection");
            continue;
        }

        int *client_fd_ptr = (int *)malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_t thread;
        pthread_create(&thread, nullptr, handle_client, client_fd_ptr);

        pthread_mutex_lock(&mymutex);
        cnt++;
        pthread_mutex_unlock(&mymutex);

        pthread_detach(thread);
    }
    close(server_fd);

    return 0;
}