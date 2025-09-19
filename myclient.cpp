#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>  // mkdir
#include <filesystem>  // C++17 filesystem support

using namespace std;
namespace fs = filesystem;

// Function to create directories for a given file path
void ensure_directories_exist(const string &filepath) {
    fs::path dir = fs::path(filepath).parent_path();  // Get the directory part

    if ( !fs::exists(dir)){
        fs::create_directories(dir);  // Create missing directories
    }
}


#define SIZE 1024




void send_file(FILE *fp, int sockfd) {
    char data[SIZE] = {0};
    size_t bytes_read;
    
    // First send "client" identifier
    send(sockfd, "client", 6, 0);
    
    // Then send file contents
    while ((bytes_read = fread(data, 1, SIZE, fp)) > 0) {
        if (send(sockfd, data, bytes_read, 0) == -1) {
            perror("[-] Error in sending data");
            exit(1);
        }
    }
    
    // Send end marker
    send(sockfd, "ENDOFIGNORELIST", 15, 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {  // Expecting <directorypath> <file_list>
        cerr << "Usage: " << argv[0] << " <directorypath> <file_list>\n";
        return 1;
    }

    string base_path = argv[1];  // Directory where files should be replicated
    char *file_list = argv[2];

    int port = 5789;  // Default port
    int client_fd;
    struct sockaddr_in address;
    char buffer[1025] = {0};

    // Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    // Connect to the server
    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Connection failed");
        return 1;
    }
    // send(client_fd, "client", 6, 0);

    FILE *fp = fopen(file_list, "r");
    if (fp == NULL) {
        perror("Error opening file list");
        return 1;
    }
    send_file(fp, client_fd);
    

    cout << "Ignore list sent to server.\n";

    int header_charread = 0;
    char header[11] = {0};  // Extra space for null terminator
     int typeheader = 0;
    char sizewala[11] = {0};  // Extra space for null terminator
    int file_size = 0;
    int filenameiter = 0;
    string filename;
    FILE *file = nullptr;

    while (true) {
        memset(buffer, 0, sizeof(buffer));  // Clear buffer before reading
        int n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            perror("Connection closed or error");
            break;
        }

        int iter = 0;

        while(iter<n){
            while (iter < n && header_charread < 10) {
                header[header_charread] = buffer[iter];
                header_charread++;
                iter++;
            }
            header[10] = '\0';  // Null-terminate
    
            while (iter < n && typeheader < 10) {
                sizewala[typeheader] = buffer[iter];
                typeheader++;
                iter++;
            }
            sizewala[10] = '\0';  // Null-terminate
            
            if (typeheader == 10) {
                file_size = stoi(sizewala);
            }
    
            // If header = create or delete, take the filename
            // If header == "DATA______", then take in the file
            if (strncmp(header, "DATA______", 10) == 0) {
                
                // Write to file from buffer
                iter++;
                while (iter < n && filenameiter < file_size) {
                    fwrite(buffer + iter, 1, 1, file);
                    filenameiter++;
                    iter++;
                }
                if(filenameiter == file_size) {
                    cout << "File received: " << filename << endl;
                    fclose(file);
                    filenameiter = 0;
                    header_charread = 0;
                    typeheader = 0;


                }
            } else {
                while (typeheader == 10 && iter < n && filenameiter < file_size) {
                    filename += buffer[iter];
                    filenameiter++;
                    iter++;
                }
                filename = base_path + "/" + filename;
                filenameiter = 0;
                typeheader = 0;
                header_charread = 0;
    
                if (strncmp(header, "CREATE____", 10) == 0) {
                    
                    cout << "File created: " << filename << endl;
                    fs::create_directories(fs::path(filename).parent_path());
                    file = fopen(filename.c_str(), "w");
                    if (file == NULL) {
                        perror("Error opening file for writing");
                        return 1;
                    }
                } else if (strncmp(header, "DELETE____", 10) == 0) {
                    if (remove(filename.c_str()) != 0) {
                        perror("Error deleting file");
                        return 1;
                    }
                    cout << "File deleted: " << filename << endl;
                } else if (strncmp(header, "CREATE_DIR", 10) == 0) {
                    fs::create_directories(filename);   
                    cout << "Directory created: " << filename << endl;
                } else if (strncmp(header, "DELETE_DIR", 10) == 0) {
                    if (fs::remove_all(filename) == static_cast<uintmax_t>(-1)) {
                        perror("Error deleting directory");
                        return 1;
                    }
                    cout << "Directory deleted: " << filename << endl;
                }
                filename = "";
            }
        }
        
    }

    close(client_fd);
    return 0;
}
