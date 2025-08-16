#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define WIDTH 80
#define HEIGHT 30
#define OFFSETX 10
#define OFFSETY 5

typedef struct {
  int x, y;
  int dx, dy;
} Ball;

typedef struct {
  int x, y;  
  int width;
} Paddle;

typedef struct {
  int paddle_local;  // server needs to send all these to client to function
  int ball_x;
  int ball_y;
  int score_a;
  int score_b;
} server_message;

typedef struct {
  int paddle_local;  // client only needs to send its paddle information. remaining is set by server
} client_message;
 

Ball ball;
Paddle local_paddle;      // local paddle 
Paddle remote_paddle;     // remote paddle 
int score_a = 0;           // server's score
int score_b = 0;           // client's score
int game_running = 1;

// network variables
int is_server = 0;        // 1 if server, 0 if client
int sockfd;               // socket file descriptor
struct sockaddr_in server_addr, client_addr;

// functions used
void init();
void end_game();
void draw(WINDOW *win);
void *move_ball(void *args);
void update_paddle(int ch);
void reset_ball();

int main(int argc, char *argv[]) {
  if (argc < 3) // error conditions
  {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Server: %s server PORT\n", argv[0]);
    fprintf(stderr, "  Client: %s client <server_ip> PORT\n", argv[0]);
    exit(1);
  }
    
  int port;

  if (strcmp(argv[1], "server") == 0) //setting up socket for server
  {
    is_server = 1;
    //set up socket with the port number
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Ipv4 and TCP
    
    port = atoi(argv[2]);
    if (port <= 0 || port > 65535) 
    {
      fprintf(stderr, "Invalid port number\n");
      exit(1);
    }
    
    if (sockfd < 0) //error condition for creation of socket
    {
      perror("socket creation failed");
      exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; //can take in any client address
    server_addr.sin_port = htons(port);   // done in class, host to network
    
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)  //bind and check for error
    {
      perror("bind failed");
      exit(1);
    }
    
    if (listen(sockfd, 1) < 0) 
    {
      perror("listen failed");
      exit(1);
    }

    printf("Server listening on port %d\n", port); //debug condition 1
    
    // find the socket info about the client that got accepted to server
    int client_len = sizeof(client_addr);
    int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_len); //accept keeps runnning till you  get a connection
    if (client_sock < 0) 
    {
      perror("accept failed");
      exit(1);
    }

    close(sockfd);    // closing the listening socket which is the server socker

    sockfd = client_sock;  
    printf("Client connected\n");  
  } 
  else if (strcmp(argv[1], "client") == 0)  // setting up socket for client and connecting to server
  {
    if (argc < 4) 
    {
      fprintf(stderr, "Usage: %s client <server_ip> PORT\n", argv[0]);
      exit(1);
    }
    is_server = 0;
    port = atoi(argv[3]);
    if (port <= 0 || port > 65535) 
    {
      fprintf(stderr, "Invalid port number\n");
      exit(1);
    }

    //setting up client socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // again Ipv4 and TCP
    if (sockfd < 0) 
    {
      perror("socket creation failed");
      exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    printf("port number is: %d \n", ntohs(server_addr.sin_port));
    
    //setting server address into the socket
    if (inet_pton(AF_INET, argv[2], &server_addr.sin_addr) <= 0) 
    {
      perror("invalid server address");
      exit(1);
    }

    printf("server ip address is: %s \n", inet_ntoa(server_addr.sin_addr));

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) //connecting to server
    {
      printf("entered the connect phase \n");
      perror("connection failed");
      exit(1);
    }

    printf("Connected to server at %s:%d\n", argv[2], port); 
    //in client sockfd containts the server socket while sockfd in server has client socket.
  }
  else 
  {
    fprintf(stderr, "Invalid mode: use 'server' or 'client'\n");
    exit(1);
  }

  //initialising the game
  init();

  // Only server handles ball movement
  if (is_server) 
  {
    local_paddle = (Paddle){WIDTH / 2 - 3, HEIGHT - 2, 10};  
    remote_paddle = (Paddle){WIDTH / 2 - 3, 1, 10};             

    ball = (Ball){WIDTH / 2, HEIGHT / 2, 1, 1};
    pthread_t ball_thread;
    if (pthread_create(&ball_thread, NULL, move_ball, NULL) != 0) 
    {
      perror("thread creation failed");
      exit(1);
    }
  }
  else
  {
    local_paddle = (Paddle){WIDTH / 2 - 3, 1, 10};           // Top
    remote_paddle = (Paddle){WIDTH / 2 - 3, HEIGHT - 2, 10};
  }
    
  //game loop same as the boiler code given
  while (game_running)  // at each instant taking the input character, whether it is left,right arrow or 'q'
  {
    int ch = getch();   //  gets the innput character from terminal
    if (ch == 'q')      // escape character
    {
      game_running = 0;
      break;
    }

    update_paddle(ch);  // updating local paddle

    if (is_server) //needs client paddle info to print the screen
    {
      client_message client_msg;
      ssize_t bytes = recv(sockfd, &client_msg, sizeof(client_msg), 0);  //taking datat from recieve buffer
      if (bytes <= 0) 
      {
        printf("client disconnected.\n");
        game_running = 0;
        break;
      }
      
      remote_paddle.x = client_msg.paddle_local;  
      // client needs ball position and server_paddle position
      server_message server_msg = { local_paddle.x, ball.x, ball.y, score_a, score_b};

      if (send(sockfd, &server_msg, sizeof(server_msg), 0) < 0) 
      {
        perror(" server send failed");
        game_running = 0;
        break;
      }
    } 
    else 
    {
      client_message client_msg = { local_paddle.x };
      if (send(sockfd, &client_msg, sizeof(client_msg), 0) < 0) 
      {
        perror("send failed");
        game_running = 0;
        break;
      }

      server_message server_msg;
      ssize_t bytes = recv(sockfd, &server_msg, sizeof(server_msg), 0); //again taking info from server (player 2)

      if (bytes <= 0) 
      {
        printf("Server disconnected.\n");
        game_running = 0;
        break;
      }

      // Updating
      remote_paddle.x = server_msg.paddle_local;
      ball.x = server_msg.ball_x;
      ball.y = server_msg.ball_y;
      score_a = server_msg.score_a;
      score_b = server_msg.score_b;
    }
    
    draw(stdscr);
  }
  
  close(sockfd);
  end_game();
}

void init() {
  initscr();
  start_color();
  init_pair(1, COLOR_BLUE, COLOR_WHITE);
  init_pair(2, COLOR_YELLOW, COLOR_YELLOW);
  timeout(10);      
  keypad(stdscr, TRUE);  
  curs_set(FALSE);
  noecho();
}

void end_game() {
  endwin();  // End curses mode
}

void draw(WINDOW *win) {
  clear();

  // Draw border and header info.
  attron(COLOR_PAIR(1));

  for (int i = OFFSETX; i <= OFFSETX + WIDTH; i++)
    mvprintw(OFFSETY - 1, i, " ");
  mvprintw(OFFSETY - 1, OFFSETX + 3, "CS3205 NetPong");
  mvprintw(OFFSETY - 1, OFFSETX + WIDTH - 25, "Server: %d, Client: %d", score_a, score_b);
    
  for (int i = OFFSETY; i < OFFSETY + HEIGHT; i++) 
  {
    mvprintw(i, OFFSETX, "  ");
    mvprintw(i, OFFSETX + WIDTH - 1, "  ");
  }

  for (int i = OFFSETX; i < OFFSETX + WIDTH; i++) 
  {
    mvprintw(OFFSETY, i, " ");
    mvprintw(OFFSETY + HEIGHT - 1, i, " ");
  }

  attroff(COLOR_PAIR(1));
    
  // drawing the ball.
  mvprintw(OFFSETY + ball.y, OFFSETX + ball.x, "o");

  attron(COLOR_PAIR(2));
  for (int i = 0; i < local_paddle.width; i++) {
    mvprintw(OFFSETY + local_paddle.y, OFFSETX + local_paddle.x + i, " ");
  }
  for (int i = 0; i < remote_paddle.width; i++) {
      mvprintw(OFFSETY + remote_paddle.y, OFFSETX + remote_paddle.x + i, " ");
  }
  attroff(COLOR_PAIR(2));

  refresh();
}

void *move_ball(void *args) { // robounds and misses from paddles
  while (game_running) 
  {
    ball.x += ball.dx;
    ball.y += ball.dy;
    
    //bounces of the left and right walls
    if (ball.x <= 2 || ball.x >= WIDTH - 2)
      ball.dx = -ball.dx;
    
    // hits the bottom paddle and top respectively
    else if (ball.y <= HEIGHT - 2 && ball.x >= local_paddle.x - 1 && ball.x < local_paddle.x + local_paddle.width + 1 ) 
      ball.dy = -ball.dy;
    else if (ball.y == 2 && ball.x >= remote_paddle.x - 1 && ball.x < remote_paddle.x + remote_paddle.width + 1)
      ball.dy = -ball.dy;

    //misses the paddles
    else if (ball.y >= HEIGHT - 1) 
    {
      score_b++;
      reset_ball();
    }
    else if (ball.y <= 1) 
    {
      score_a++;
      reset_ball();
    }
    usleep(150000);
  }
  return NULL;
}

void update_paddle(int ch) {  //to update local paddle position
  if (ch == KEY_LEFT && local_paddle.x > 2)
    local_paddle.x--;
  if (ch == KEY_RIGHT && local_paddle.x < WIDTH - local_paddle.width - 1)
    local_paddle.x++;
}

void reset_ball() {
  ball.x = WIDTH / 2;
  ball.y = HEIGHT / 2;
  ball.dx = (rand() % 2 == 0) ? 1 : -1;
  ball.dy = (rand() % 2 == 0) ? 1 : -1;
}