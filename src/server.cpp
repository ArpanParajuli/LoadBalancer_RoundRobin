#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>



#define PORT 3001
#define MAXCONNECTIONS 100


using namespace std;



class Server{
    public:
    Server * next;
    string address;
    int port;

    Server(int p , string s)
    {
        address = s;
        port = p;
        next = nullptr;
    }
};


class RoundRobin{
    public:
    Server * head;

    Server * current;

    RoundRobin()
    {
        head = nullptr;
        current = nullptr;
    }

    void InsertServer(int p , string s)
    {
        Server * newServer = new Server(p , s);

        if(head == nullptr)
        {
            head = newServer;
            newServer->next = head;
            return;
        }


        Server * temp = head;

        while(temp->next != head)
        {
            temp = temp->next;
        }

        temp->next = newServer;
        newServer->next = head;
    }

   void display() {
    if (head == nullptr) return;

    Server* temp = head;
    do {
        cout << temp->address << ":" << temp->port <<endl;
        temp = temp->next;
    } while (temp != head);
}




 Server * NextServer()
{
    if(current == nullptr)
    {
        current = head;
    }

    Server * temp = current;
    cout << "Request will be handle by " << temp->address << " : " << temp->port << endl;
    current = current->next;
    return temp;
}

};





int ConnectBackend(Server * backend)
{
    int sock = -1;
    struct addrinfo hints , *res , *temp;

           memset(&hints, 0, sizeof(hints));
           hints.ai_family = AF_INET;    /* Allow IPv4 aaile lai */
           hints.ai_socktype = SOCK_STREAM; /* TCP socket */
           hints.ai_flags = 0;
           hints.ai_protocol = 0;          /* Any protocol */

     char portStr[6];
    sprintf(portStr, "%d", backend->port);

    int err =  getaddrinfo(backend->address.c_str() ,portStr, &hints , &res);

    if(err != 0){
        cerr << "Getaddrinfo failed to find" << endl;
       return -1;
    } 

    for(temp = res; temp != NULL; temp = temp->ai_next)
    {
         sock = socket(temp->ai_family , temp->ai_socktype , temp->ai_protocol);
         if(sock < 0) continue;

           if (connect(sock, temp->ai_addr, temp->ai_addrlen) == 0)
                   break;                  /* Success */



          close(sock);
          sock = -1;         
    }      


    freeaddrinfo(res);
   
    return sock;
}


void ForwardDataToClient(int client_sock, int server_socket, const string& servername) {
    const int MaxSize = 1024;
    char buffer[MaxSize];

    ssize_t clientbytes = recv(client_sock, buffer, sizeof(buffer), 0);
    if (clientbytes < 0) {
        cerr << "Error receiving from client\n";
        return;
    }

     // send(server_socket, buffer, bytes, 0);



    string response = "HTTP/1.1 200 OK\r\nContent-Length: ";
    response += to_string(servername.size()) + "\r\n\r\n";
    response += servername;


    
// ssize_t backend_bytes = recv(server_socket, buffer, sizeof(buffer), 0);
// if (backend_bytes > 0)
//     send(client_sock, buffer, backend_bytes, 0);

    ssize_t sentbytes = send(client_sock, response.c_str(), response.size(), 0);
    if (sentbytes < 0) {
        cerr << "Error sending to client\n";
    }
}

int main()
{
    RoundRobin rr;
    rr.InsertServer(5215 , "127.0.0.1"); // http://localhost:5215
    rr.InsertServer(8008 , "127.0.0.1"); // http://localhost:5215
    rr.InsertServer(3000 , "127.0.0.1"); // http://localhost:5215

    rr.display();
       
        struct sockaddr_in serveraddr , clientaddr;
        memset(&serveraddr, 0, sizeof(serveraddr));


         int socket_fd = socket(AF_INET , SOCK_STREAM , 0);
       
         serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); /* IPv4 address */
         serveraddr.sin_family = AF_INET;                /* AF_INET */
         serveraddr.sin_port = htons(PORT);              /* Port number */

         int yes = 1;
          setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)); // address already inuse fix 

    if (bind(socket_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        cerr << "socket binding failed! \n";
    }

    if (listen(socket_fd, MAXCONNECTIONS) < 0)
    {
        cerr << "Listen failed! \n";
    }

    socklen_t cliaddrlen = sizeof(clientaddr);


    cout << "Load Balancer running on PORT : " << PORT << endl;

    while (1)
    {

        int client_sock = accept(socket_fd, (struct sockaddr *)&clientaddr, &cliaddrlen);

        if (client_sock < 0)
        {
            cerr << "Accept failed! \n";

            if (errno == EAGAIN || errno == EWOULDBLOCK) // handle resource temporaily unavailable
            {
                sleep(1);
                continue;
            }

            continue;
        }




        Server * ServingServer = rr.NextServer();
        int server_sock = ConnectBackend(ServingServer);
        while(server_sock < 0)
        {
            cout << "Server failed to connect redirecting to next server " << endl;
            ServingServer = rr.NextServer();
            server_sock = ConnectBackend(ServingServer);
        }

        ForwardDataToClient(client_sock , server_sock , ServingServer->address);


        close(client_sock);
        close(server_sock);

        cout << "Client request! accepted \n";

     }


     close(socket_fd);

     return 0;

}
