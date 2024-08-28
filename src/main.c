#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_IPS 100          // Set maximum IPs allowed
#define MAX_IP_LENGTH 16     // Maximum length of the IP address string
#define REFRESH_INTERVAL 5   // Time in seconds between updates
#define PING_COUNT 3         // Number of ping packets
#define MAX_OUTPUT_SIZE 1024 // Maximum size of the buffer
#define TEST_INTERVAL 10     // Interval in seconds for each IP test

typedef struct {
  char ip[MAX_IP_LENGTH];
  char output[MAX_OUTPUT_SIZE];
  int is_up;
  int test_count;
  time_t last_up_time;    // Time when the IP was last seen up
  time_t last_check_time; // Time when the IP was last checked
  pthread_t thread;       // Thread ID for monitoring the IP
  pthread_mutex_t mutex;  // Mutex for synchronizing access to output and status
} IPMonitor;

// Function prototypes
void *monitor_ip(void *arg);
void display_results(WINDOW *status_win, WINDOW *output_win,
                     IPMonitor monitors[], int ip_count);
int ping_ip(const char *ip, char *output, size_t output_size);
void get_current_time(char *buffer, size_t buffer_size);
void get_time_diff(time_t diff_seconds, char *buffer, size_t buffer_size);
void print_usage(const char *program_name);

int main(int argc, char *argv[]) {
  IPMonitor monitors[MAX_IPS];
  int ip_count = 0;
  int i;

  // Check if user requested help
  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    print_usage(argv[0]);
    return 0;
  }

  // Parse IP addresses from command-line arguments
  if (argc > 1 && strcmp(argv[1], "--ips") == 0) {
    for (i = 2; i < argc; i++) {
      if (ip_count >= MAX_IPS) {
        printf("Maximum number of IPs reached.\n");
        break;
      }

      strncpy(monitors[ip_count].ip, argv[i], MAX_IP_LENGTH);
      monitors[ip_count].ip[MAX_IP_LENGTH - 1] =
          '\0'; // Ensure null-termination
      monitors[ip_count].test_count = 0;
      monitors[ip_count].is_up = 0;        // Default to DOWN
      monitors[ip_count].last_up_time = 0; // No last up time initially
      monitors[ip_count].last_check_time =
          time(NULL); // Set last check time to now
      pthread_mutex_init(&monitors[ip_count].mutex, NULL); // Initialize mutex
      ip_count++;
    }
  } else {
    print_usage(argv[0]);
    return 1;
  }

  // Initialize threads for asynchronous monitoring
  for (i = 0; i < ip_count; i++) {
    pthread_create(&monitors[i].thread, NULL, monitor_ip, &monitors[i]);
  }

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  timeout(100); // Non-blocking input

  start_color();
  init_pair(1, COLOR_GREEN, COLOR_BLACK);  // UP status color
  init_pair(2, COLOR_RED, COLOR_BLACK);    // DOWN status color
  init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Yellow for ping output
  init_pair(4, COLOR_CYAN, COLOR_BLACK);   // Cyan for ping output details

  int height, width;
  getmaxyx(stdscr, height, width); // Get terminal size

  // Adjust window sizes according to terminal size
  WINDOW *status_win = newwin(height, width / 2, 0, 0);
  WINDOW *output_win = newwin(height, width / 2, 0, width / 2);

  // Print titles for the frames
  mvwprintw(status_win, 0, 1, "IP Monitor Output");
  mvwprintw(output_win, 0, 1, "Ping Output");

  // Main loop to display results and update every REFRESH_INTERVAL seconds
  int ch;
  while ((ch = wgetch(stdscr)) != 'q') {
    display_results(status_win, output_win, monitors, ip_count);
    sleep(REFRESH_INTERVAL); // Wait for a while before updating
  }

  // Display termination message
  clear();
  mvprintw(LINES / 2, COLS / 2 - 10, "Terminating...");
  refresh();
  sleep(2); // Show the termination message for 2 seconds

  // Join threads and cleanup ncurses
  for (i = 0; i < ip_count; i++) {
    pthread_cancel(monitors[i].thread);        // Cancel the thread
    pthread_join(monitors[i].thread, NULL);    // Ensure thread termination
    pthread_mutex_destroy(&monitors[i].mutex); // Destroy mutex
  }

  delwin(status_win);
  delwin(output_win);
  endwin();

  return 0;
}

void *monitor_ip(void *arg) {
  IPMonitor *monitor = (IPMonitor *)arg;

  while (1) {
    time_t now = time(NULL);

    // Check if it's time to perform the test
    if (now - monitor->last_check_time >= TEST_INTERVAL) {
      // Update the IP status
      pthread_mutex_lock(&monitor->mutex); // Lock mutex
      monitor->is_up = ping_ip(monitor->ip, monitor->output, MAX_OUTPUT_SIZE);
      monitor->test_count++;
      if (monitor->is_up) {
        monitor->last_up_time = now; // Update last up time if IP is up
      }
      monitor->last_check_time = now;        // Update last check time
      pthread_mutex_unlock(&monitor->mutex); // Unlock mutex
    }

    // Sleep briefly to avoid busy-waiting
    usleep(100000); // 100 milliseconds
  }

  return NULL;
}

void display_results(WINDOW *status_win, WINDOW *output_win,
                     IPMonitor monitors[], int ip_count) {
  int i;
  char timestamp[64];
  char last_up_buffer[64];

  wclear(status_win);
  wclear(output_win);
  wborder(status_win, '|', '|', '-', '-', '+', '+', '+', '+');
  wborder(output_win, '|', '|', '-', '-', '+', '+', '+', '+');

  // Print IP Monitor Output title
  mvwprintw(status_win, 0, 1, "IP Monitor Output");

  // Print IP status
  for (i = 0; i < ip_count; i++) {
    get_current_time(timestamp, sizeof(timestamp));

    // Lock mutex to safely access monitor data
    pthread_mutex_lock(&monitors[i].mutex);

    // Display status
    if (monitors[i].is_up) {
      wattron(status_win, COLOR_PAIR(1)); // Green color
      mvwprintw(status_win, i + 1, 1, "%s (# of checks: %d) IP %s is UP",
                timestamp, monitors[i].test_count, monitors[i].ip);
    } else {
      wattron(status_win, COLOR_PAIR(2) | A_BOLD); // Red color, bold
      if (monitors[i].last_up_time > 0) {
        get_time_diff(time(NULL) - monitors[i].last_up_time, last_up_buffer,
                      sizeof(last_up_buffer));
        mvwprintw(status_win, i + 1, 1,
                  "%s (# of checks: %d) IP %s is DOWN (Last seen up: %s)",
                  timestamp, monitors[i].test_count, monitors[i].ip,
                  last_up_buffer);
      } else {
        mvwprintw(status_win, i + 1, 1,
                  "%s (# of checks: %d) IP %s is DOWN (Never seen up)",
                  timestamp, monitors[i].test_count, monitors[i].ip);
      }
    }
    wattroff(status_win, COLOR_PAIR(1) | COLOR_PAIR(2) | A_BOLD);

    pthread_mutex_unlock(&monitors[i].mutex); // Unlock mutex
  }

  // Print Ping Output title
  mvwprintw(output_win, 0, 1, "Ping Output");

  // Display ping output with timestamp
  int line = 1;
  for (i = 0; i < ip_count; i++) {
    char ip_timestamp[64];
    pthread_mutex_lock(&monitors[i].mutex);
    get_current_time(ip_timestamp, sizeof(ip_timestamp));

    if (monitors[i].is_up) {
      wattron(output_win, COLOR_PAIR(3)); // Yellow color for the title
    } else {
      wattron(output_win, COLOR_PAIR(4)); // Cyan color for details
    }
    mvwprintw(output_win, line, 1, "%s - IP %s:\n%s", ip_timestamp,
              monitors[i].ip, monitors[i].output);
    wattroff(output_win, COLOR_PAIR(3) | COLOR_PAIR(4));

    line += PING_COUNT +
            2; // Move down by number of lines in ping output + some extra space
    pthread_mutex_unlock(&monitors[i].mutex);
  }

  wrefresh(status_win);
  wrefresh(output_win);
}

int ping_ip(const char *ip, char *output, size_t output_size) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c %d -W 1 %s 2>/dev/null", PING_COUNT, ip);

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    snprintf(output, output_size, "Failed to execute ping");
    return 0; // Failure to execute ping
  }

  // Initialize output buffer
  memset(output, 0, output_size);

  char buffer[256];
  size_t len = 0;
  int received = 0;
  int total_packets = PING_COUNT;

  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (len + strlen(buffer) < output_size) {
      strcat(output, buffer);
      len += strlen(buffer);
    }

    // Check if packet loss is reported
    if (strstr(buffer, "packet loss") != NULL) {
      sscanf(buffer, "%*d packets transmitted, %d received", &received);
    }
  }

  pclose(fp);

  // Determine if the IP is up based on packet loss
  int is_up =
      (received >=
       (total_packets /
        2)); // Consider it UP if at least half of the packets are received
  return is_up;
}

void get_current_time(char *buffer, size_t buffer_size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

void get_time_diff(time_t diff_seconds, char *buffer, size_t buffer_size) {
  int hours = diff_seconds / 3600;
  int minutes = (diff_seconds % 3600) / 60;
  int seconds = diff_seconds % 60;

  snprintf(buffer, buffer_size, "%d hours, %d minutes, and %d seconds ago",
           hours, minutes, seconds);
}

void print_usage(const char *program_name) {
  printf("Usage: %s --ips <ip1> <ip2> \n", program_name);
  printf("Options:\n");
  printf("  --ips       IP addresses to monitor one by one separated by spaces "
         "(eg: 192.168.1.0 192.168.1.2)\n");
  printf("  --help      Display this help message\n");
}
