#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_IPS 100
#define MAX_IP_LENGTH 16

// Function prototypes
void display_results(WINDOW *win, char *ips[], int ip_count);
void clear_screen();
int ping_ip(const char *ip);

int main() {
  char *ips[MAX_IPS];
  int ip_count = 0;
  char input[MAX_IP_LENGTH];
  int i;

  // Initialize standard input/output
  printf("Enter IP addresses (type 'done' to finish):\n");
  while (1) {
    printf("IP: ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0'; // Remove newline character

    if (strcmp(input, "done") == 0) {
      break;
    }

    if (ip_count >= MAX_IPS) {
      printf("Maximum number of IPs reached.\n");
      break;
    }

    ips[ip_count] = strdup(input);
    ip_count++;
  }

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  timeout(100); // Non-blocking input
  int height = 20, width = 60;
  WINDOW *main_win = newwin(height, width, 0, 0);
  WINDOW *status_win = newwin(height, width / 2, 0, width / 2);

  // Display results
  display_results(status_win, ips, ip_count);

  // Cleanup ncurses
  delwin(main_win);
  delwin(status_win);
  endwin();

  // Free allocated memory
  for (i = 0; i < ip_count; i++) {
    free(ips[i]);
  }

  return 0;
}

void display_results(WINDOW *win, char *ips[], int ip_count) {
  int i;
  for (i = 0; i < ip_count; i++) {
    int status = ping_ip(ips[i]);
    wclear(win);
    wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(win, 1, 1, "IP %s is %s", ips[i], status ? "UP" : "DOWN");
    wrefresh(win);
    sleep(1); // Delay for demonstration
  }
}

int ping_ip(const char *ip) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c 3 -W 1 %s", ip);

  FILE *fp = popen(cmd, "r");
  if (fp == NULL) {
    return 0; // Failure to execute ping
  }

  char buffer[256];
  int up = 0;
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (strstr(buffer, "packets transmitted,") != NULL) {
      up = strstr(buffer, " 0% packet loss") != NULL;
      break;
    }
  }

  pclose(fp);
  return up;
}
