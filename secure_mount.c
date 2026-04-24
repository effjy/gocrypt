/*
 * secure_mount.c - Menu-driven secure gocryptfs manager
 * Compile: gcc secure_mount.c -o secure_mount -O2
 * Usage:   ./secure_mount
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <errno.h>

#define MAX_PATH 1024

static char* read_line(char *buf, size_t size) {
    if (!fgets(buf, size, stdin)) return NULL;
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

static void pause_prompt() {
    printf("\n[Press Enter to continue...]");
    fflush(stdout);
    char c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static int run_cmd(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) { perror("[-] fork failed"); return -1; }
    if (pid == 0) {
        prctl(PR_SET_DUMPABLE, 0);
        execvp(cmd, argv);
        perror("[-] exec failed");
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static int do_mount() {
    char cipher[MAX_PATH], mount[MAX_PATH];
    printf("\nEnter cipher directory (encrypted storage): ");
    if (!read_line(cipher, sizeof(cipher))) return -1;
    if (!strlen(cipher)) { printf("[-] Path cannot be empty.\n"); return -1; }

    printf("Enter mount point (decrypted view): ");
    if (!read_line(mount, sizeof(mount))) return -1;
    if (!strlen(mount)) { printf("[-] Path cannot be empty.\n"); return -1; }

    if (strcmp(cipher, mount) == 0) {
        fprintf(stderr, "[-] Error: Cipher directory and mount point CANNOT be the same folder.\n");
        fprintf(stderr, "[!] Use two separate directories. Example:\n");
        fprintf(stderr, "    Cipher: ~/data/encrypted\n");
        fprintf(stderr, "    Mount:  ~/data/decrypted\n");
        return -1;
    }

    struct stat st;
    if (stat(cipher, &st) != 0) {
        fprintf(stderr, "[-] Cipher directory '%s' does not exist.\n", cipher);
        fprintf(stderr, "[!] Initialize it first with option [3].\n");
        return -1;
    }
    if (stat(mount, &st) != 0) {
        printf("[*] Mount point missing. Creating (0700)...\n");
        if (mkdir(mount, 0700) != 0) { perror("[-] mkdir failed"); return -1; }
    }

    printf("[*] Mounting... (enter gocryptfs password when prompted)\n");
    char *argv[] = {"gocryptfs", cipher, mount, NULL};
    return run_cmd("gocryptfs", argv);
}

static int do_unmount() {
    char mount[MAX_PATH];
    printf("\nEnter mount point path to unmount: ");
    if (!read_line(mount, sizeof(mount))) return -1;
    if (!strlen(mount)) { printf("[-] Path cannot be empty.\n"); return -1; }

    printf("[*] Unmounting...\n");
    char *argv[] = {"fusermount3", "-u", mount, NULL};
    int ret = run_cmd("fusermount3", argv);
    if (ret != 0) {
        char *argv2[] = {"fusermount", "-u", mount, NULL};
        ret = run_cmd("fusermount", argv2);
    }
    if (ret != 0) {
        char *argv3[] = {"umount", mount, NULL};
        ret = run_cmd("umount", argv3);
    }
    return ret;
}

static int do_init() {
    char cipher[MAX_PATH];
    printf("\nEnter path for NEW encrypted directory: ");
    if (!read_line(cipher, sizeof(cipher))) return -1;
    if (!strlen(cipher)) { printf("[-] Path cannot be empty.\n"); return -1; }

    struct stat st;
    if (stat(cipher, &st) == 0) {
        fprintf(stderr, "[-] Directory '%s' already exists.\n", cipher);
        return -1;
    }

    if (mkdir(cipher, 0700) != 0) {
        perror("[-] Failed to create directory");
        return -1;
    }
    printf("[*] Created directory '%s' (0700)\n", cipher);

    printf("[*] Initializing gocryptfs volume... (enter & confirm master password)\n");
    char *argv[] = {"gocryptfs", "-init", cipher, NULL};
    return run_cmd("gocryptfs", argv);
}

int main() {
    prctl(PR_SET_DUMPABLE, 0);
    int choice;
    char buf[16];

    while (1) {
        printf("\033[H\033[2J");
        printf("========================================\n");
        printf("       Secure gocryptfs Manager\n");
        printf("========================================\n");
        printf(" [1] Mount encrypted partition\n");
        printf(" [2] Unmount encrypted partition\n");
        printf(" [3] Initialize new encrypted partition\n");
        printf(" [4] Exit\n");
        printf("========================================\n");
        printf("Select an option: ");

        if (!read_line(buf, sizeof(buf))) break;
        if (sscanf(buf, "%d", &choice) != 1) {
            printf("[-] Invalid input. Please enter a number.\n");
            pause_prompt();
            continue;
        }

        switch (choice) {
            case 1: do_mount(); break;
            case 2: do_unmount(); break;
            case 3: do_init(); break;
            case 4:
                printf("[*] Exiting securely. Goodbye.\n");
                return EXIT_SUCCESS;
            default:
                printf("[-] Invalid option. Try again.\n");
        }
        pause_prompt();
    }
    return EXIT_SUCCESS;
}
