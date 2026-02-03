/*
Hashir Rashid - 100910330
Lab 2 – A Simple Shell
*/

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_LINE 1024

// Function to trim whitespace from front and end of input
static char *trim_white_space(char *str) {
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

// Function to split at a delimeter if it is not within certain characters (like "")
static char *strmbtok(char *input, char *delimit, char *openBlock, char *closeBlock) {
    static char *token = NULL;
    char *lead = NULL;
    char *block = NULL;
    int iBlock = 0;
    int iBlockIndex = 0;

    if (input != NULL) {
        token = input;
        lead = input;
    }
    else {
        lead = token;
        if (*token == '\0') {
            lead = NULL;
        }
    }

    while (*token != '\0') {
        if (iBlock) {
            if (closeBlock[iBlockIndex] == *token) {
                iBlock = 0;
            }
            token++;
            continue;
        }
        if ((block = strchr(openBlock, *token)) != NULL) {
            iBlock = 1;
            iBlockIndex = block - openBlock;
            token++;
            continue;
        }
        if (strchr(delimit, *token) != NULL) {
            *token = '\0';
            token++;
            break;
        }
        token++;
    }
    return lead;
}

// Function to remove quotation marks from input
static char* remove_quotes(char* s1) {
    size_t len = strlen(s1);
    if (s1[0] == '"' && s1[len - 1] == '"') {
        s1[len - 1] = '\0';
        memmove(s1, s1 + 1, len - 1);
    }
    return s1;
}

// Function to handle input redirection
static int handle_input_redirection(char **args, FILE **input_stream) {
    *input_stream = stdin;
    int i = 0;
    
    // Find input redirection operator
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i+1] != NULL) {
                char *filename = remove_quotes(args[i+1]);
                
                *input_stream = fopen(filename, "r");
                if (*input_stream == NULL) {
                    perror("fopen error");
                    *input_stream = stdin;
                    return -1;
                }
                args[i] = "^_IGNORE"; // Terminate args before redirection
                args[i+1] = "^_IGNORE"; // Also mark filename as NULL
                return 1;
            } else {
                fprintf(stderr, "Syntax error: missing filename after %s\n", args[i]);
                return -1;
            }
        }
        i++;
    }
    return 0;
}

// Function to handle output redirection
static int handle_output_redirection(char **args, FILE **output_stream) {
    *output_stream = stdout;
    int i = 0;
    
    // Find redirection operators
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            if (args[i+1] != NULL) {
                char *filename = remove_quotes(args[i+1]);
                char *mode = (strcmp(args[i], ">") == 0) ? "w" : "a";
                
                *output_stream = fopen(filename, mode);
                if (*output_stream == NULL) {
                    perror("fopen error");
                    *output_stream = stdout;
                    return -1;
                }
                args[i] = "^_IGNORE"; // Terminate args before redirection
                args[i+1] = "^_IGNORE"; // Also mark filename as NULL
                return 1;
            } else {
                fprintf(stderr, "Syntax error: missing filename after %s\n", args[i]);
                return -1;
            }
        }
        i++;
    }
    return 0;
}

// Function to reap zombie processes
static void reap_zombies(void) {
    int status;
    pid_t pid;
    
    // Reap any zombie processes without blocking
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[%d] Background process completed\n", pid);
        fflush(stdout);
    }
}

// Function to process the input line
static int process_line(char *line) {
    char openChar[]  = {"\"[{"};
    char closeChar[] = {"\"]}"};

    // Remove whitespace characters from input
    line[strcspn(line, "\n")] = 0;
    line = trim_white_space(line);

    // Create a pointer to keep track of arguments
    char *currentArg = strmbtok(line, " ", openChar, closeChar);

    // Cd command
    if (strcmp(currentArg, "cd") == 0) {
        char cwd[PATH_MAX]; // Variable to hold the current working directory
        currentArg = strmbtok(NULL, " ", openChar, closeChar); // Get next argument (directory to change to)           
        
        // If no directory given, print path for current working directory
        if (getcwd(cwd, sizeof(cwd)) != NULL && currentArg == NULL) {
            printf("Current working dir: %s\n", cwd);
        }

        // Otherwise, change the working directory to the one specified
        else {
            currentArg = remove_quotes(currentArg); // Remove quotation marks (e.g. cd "lab 2")
            if (chdir(currentArg) == -1)
                perror("chdir error"); // Return an error if requested directory could not be found
        }
    }
    
    // Clr command
    else if (strcmp(currentArg, "clr") == 0) {
        // Clear screen and move cursor to home using escape characters
        printf("\x1b[2J\x1b[H"); 
        fflush(stdout); 
    }
    
    // Dir command
    else if (strcmp(currentArg, "dir") == 0) {
        // Build arguments array
        char *args[64];
        int arg_count = 0;
        args[arg_count++] = currentArg;
        
        // Get the directory argument (could be NULL)
        char *dir_arg = strmbtok(NULL, " ", openChar, closeChar);
        
        // Collect all remaining arguments
        while (dir_arg != NULL) {
            args[arg_count++] = dir_arg;
            dir_arg = strmbtok(NULL, " ", openChar, closeChar);
        }
        args[arg_count] = NULL;
        
        // Handle redirection
        FILE *output_stream;
        int redir_result = handle_output_redirection(args, &output_stream);
        
        if (redir_result < 0) {
            // Error opening file, but continue to print to stdout
            output_stream = stdout;
        }
        
        // Determine which directory to list
        // args[1] is the first argument after "dir"
        char *requestedDir = ".";
        if (args[1] != NULL) {
            requestedDir = remove_quotes(args[1]);
        }
        
        // List files in the requested directory
        DIR *d;
        struct dirent *dir;
        d = opendir(requestedDir);
        
        // If the directory exists, open it and list its contents
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                fprintf(output_stream, "%s\n", dir->d_name);
            }
            
            // Close the directory when done
            closedir(d);
        } else {
            // Note: error messages should still go to stderr, not the output file
            fprintf(stderr, "dir error: cannot open directory '%s'\n", requestedDir);
        }
        
        // Close file if we opened one
        if (output_stream != stdout) {
            fclose(output_stream);
        }
    }

    // Environ command
    else if (strcmp(currentArg, "environ") == 0) {
        // Build arguments array
        char *args[64];
        int arg_count = 0;
        args[arg_count++] = currentArg;
        
        // Collect all arguments
        while ((currentArg = strmbtok(NULL, " ", openChar, closeChar)) != NULL) {
            args[arg_count++] = currentArg;
        }
        args[arg_count] = NULL;
        
        FILE *output_stream;
        int redir_result = handle_output_redirection(args, &output_stream);
        
        if (redir_result < 0) {
            // Error opening file, but continue to print to stdout
            output_stream = stdout;
        }
        
        // Get all environment variables
        extern char **environ;
        char **s = environ;
        
        // Output to either stdout or file
        for (; *s; s++) {
            fprintf(output_stream, "%s\n", *s);
        }
        
        // Close file if one was opened
        if (output_stream != stdout) {
            fclose(output_stream);
        }
    }

    // Echo command
    else if (strcmp(currentArg, "echo") == 0) { 
        // Build arguments array
        char *args[64];
        int arg_count = 0;
        args[arg_count++] = currentArg;
        
        // Collect all arguments
        while ((currentArg = strmbtok(NULL, " ", openChar, closeChar)) != NULL) {
            args[arg_count++] = currentArg;
        }
        args[arg_count] = NULL;
        
        // Handle redirection
        FILE *output_stream;
        int redir_result = handle_output_redirection(args, &output_stream);
        
        if (redir_result < 0) {
            // Error opening file, but continue to print to stdout
            output_stream = stdout;
        }
        
        // Build the echo string from remaining arguments
        char printLine[MAX_LINE] = "";
        int firstWord = 1;
        
        // Start from index 1 to skip "echo"
        for (int i = 1; i < arg_count; i++) {
            // Stop if we hit NULL / ^_IGNORE (where redirection operator was)
            if (args[i] == NULL || strcmp(args[i], "^_IGNORE") == 0) break;
            
            
            // Only add space if this isn't the first word
            if (strlen(args[i]) > 0) {
                if (!firstWord)
                    strcat(printLine, " ");
                
                strcat(printLine, args[i]);
                firstWord = 0;
            }
        }
        
        // Output to either stdout or file
        fprintf(output_stream, "%s\n", printLine);
        
        // Close file if we opened one
        if (output_stream != stdout) {
            fclose(output_stream);
        }
    }

    // Help command
    else if (strcmp(currentArg, "help") == 0) {
        // Build arguments array
        char *args[64];
        int arg_count = 0;
        args[arg_count++] = currentArg;
        
        // Collect all arguments
        while ((currentArg = strmbtok(NULL, " ", openChar, closeChar)) != NULL) {
            args[arg_count++] = currentArg;
        }
        args[arg_count] = NULL;
        
        // Handle redirection
        FILE *output_stream;
        int redir_result = handle_output_redirection(args, &output_stream);
        
        if (redir_result < 0) {
            // Error opening file, but continue to print to stdout
            output_stream = stdout;
        }
        
        // If output stream is a file, write to it
        if (output_stream != stdout) {
            char ch;
            FILE *help_page;
            char help_page_file[]="readme";
            
            help_page = fopen(help_page_file, "r");
            
            if (help_page == NULL) {
                perror("fopen error");
                return 1;
            }
            
            while ((ch = fgetc(help_page)) != EOF)
                fputc(ch, output_stream);
            
            // Close the file
            fclose(output_stream);
        }
        
        else
    	    system("more -d readme");
    }
    
    // Pause command
    else if (strcmp(currentArg, "pause") == 0) {
        // "Pause" by waiting for a key to be input
        getchar();
    }

    // Quit command
    else if (strcmp(currentArg, "quit") == 0) {
        printf("Exiting...\n");
        return 0;
    }
    
    /*=== THE FOLLOWING COMMAND(S) HAVE BEEN ADDED FOR MY OWN PURPOSES ===*/
    
    // Perm command - change file permissions
    else if (strcmp(currentArg, "perm") == 0) {
        // Get the permission mode and filename
        char *mode_str = strmbtok(NULL, " ", openChar, closeChar);
        char *filename = strmbtok(NULL, " ", openChar, closeChar);
        
        if (mode_str == NULL || filename == NULL) {
            fprintf(stderr, "Usage: perm <mode> <filename>\n");
            fprintf(stderr, "Example: perm 755 myfile.txt\n");
            return 1;
        }
        
        // Remove quotes from filename if present
        filename = remove_quotes(filename);
        
        // Convert mode string to octal
        int mode;
        if (sscanf(mode_str, "%o", &mode) != 1) {
            fprintf(stderr, "Invalid permission mode: %s\n", mode_str);
            fprintf(stderr, "Mode must be an octal number (e.g., 755, 644)\n");
            return 1;
        }
        
        // Change file permissions using chmod
        if (chmod(filename, mode) == -1) {
            perror("chmod error");
            return 1;
        }
        
        printf("Changed permissions of '%s' to %o\n", filename, mode);
    }

    // Assume program invocation
    else {
        // Build arguments array
        char *args[256];
        int arg_count = 0;
        args[arg_count++] = currentArg;
        
        // Collect all arguments
        while ((currentArg = strmbtok(NULL, " ", openChar, closeChar)) != NULL) {
            args[arg_count++] = currentArg;
        }
        args[arg_count] = NULL;
    
        // Check for background execution (look for '&' at the end)
        int background = 0;
        if (arg_count > 0 && args[arg_count-1] != NULL) {
            // Check if last argument is '&'
            char *last_arg = args[arg_count-1];
            if (strcmp(last_arg, "&") == 0) {
                background = 1;
                args[arg_count-1] = NULL;  // Remove '&' from arguments
                arg_count--;
            }
        }
    
        // Handle input redirection
        FILE *input_stream = stdin;
        int input_redir_result = handle_input_redirection(args, &input_stream);
        
        if (input_redir_result < 0) {
            // Error opening file, but continue to use stdin
            input_stream = stdin;
        }
        
        // Handle output redirection
        FILE *output_stream = stdout;
        int output_redir_result = handle_output_redirection(args, &output_stream);
        
        if (output_redir_result < 0) {
            // Error opening file, but continue to print to stdout
            output_stream = stdout;
        }
        
        // Use fork and exec to run the requested program
        pid_t pid = fork();
    
        // Fork failed
        if (pid == -1) {
            perror("fork error");
            return 1;
        }
        
        // Child
        else if (pid == 0) {
            // Set environment variable "parent" to the full path of myshell
            char parent_env[PATH_MAX + 20] = "parent=";
            char shell_path[PATH_MAX];
            if (getcwd(shell_path, sizeof(shell_path)) != NULL) {
                strcat(shell_path, "/myshell");
                strcat(parent_env, shell_path);
                putenv(parent_env);
            }
            
            // Handle input redirection in child
            if (input_stream != stdin) {
                int fd_in = fileno(input_stream);
                dup2(fd_in, STDIN_FILENO);
                fclose(input_stream);
            }
            
            // Handle output redirection in child
            if (output_stream != stdout) {
                int fd_out = fileno(output_stream);
                dup2(fd_out, STDOUT_FILENO);
                fclose(output_stream);
            }
            
            // Prepare arguments for execvp
            char *exec_args[256];
            int exec_arg_count = 0;
            
            // Process the arguments
            for (int i = 0; i < arg_count; i++) {
                // Skip NULL and IGNORE arguments caused by io redirection
                if (args[i] != NULL && strcmp(args[i], "^_IGNORE") != 0) {
                    // For the first argument (program name), check if it needs "./" prepended
                    if (i == 0) {
                        // Check if first 2 characters are NOT "./"
                        if (strlen(args[i]) < 2 || 
                            (args[i][0] != '.' && args[i][1] != '/')) {
                            
                            // Check if the file exists in current directory
                            struct stat st;
                            if (stat(args[i], &st) == 0 && S_ISREG(st.st_mode)) {
                                // File exists in current directory, prepend "./"
                                char prefixed_cmd[PATH_MAX];
                                snprintf(prefixed_cmd, sizeof(prefixed_cmd), "./%s", args[i]);
                                
                                // Allocate memory for the new string
                                exec_args[exec_arg_count] = strdup(prefixed_cmd);
                                
                                if (exec_args[exec_arg_count] == NULL) {
                                    perror("strdup error");
                                    exit(1);
                                }
                                
                                exec_arg_count++;
                            } 
                            
                            else {
                                // File doesn't exist in current directory, use as-is
                                // (might be a system command like "ls" or a path like "/bin/ls")
                                exec_args[exec_arg_count++] = args[i];
                            }
                            
                        } 
                        
                        else {
                            // Already has "./" or similar, use as-is
                            exec_args[exec_arg_count++] = args[i];
                        }
                        
                    } 
                    
                    else {
                        // Not the first argument, add as-is
                        exec_args[exec_arg_count++] = args[i];
                    }
                }
            }
            
            exec_args[exec_arg_count] = NULL; // Set final argument to NULL (arrays are null-terminated)

            // Use exec to run the program / command
            if (execvp(exec_args[0], exec_args) == -1) {
                perror("execvp error");
                
                // Clean up any allocated memory
                if (exec_arg_count > 0 && exec_args[0] != args[0]) {
                    // We allocated memory for the prefixed command
                    free(exec_args[0]);
                }
            }               
            
            // Close file streams before exiting
            if (input_stream != stdin) fclose(input_stream);
            if (output_stream != stdout) fclose(output_stream);
            
            exit(127);  // Exit code for "command not found"
        }
        
        // Parent
        else {
            // Close file streams in parent (child has its own copies)
            if (input_stream != stdin && input_redir_result > 0) {
                fclose(input_stream);
            }
            
            if (output_stream != stdout && output_redir_result > 0) {
                fclose(output_stream);
            }
            
            // Wait for child to complete (foreground process)
            if (!background) {
                int status;
                waitpid(pid, &status, 0); // wait for child
                
                // Check exit status
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status == 127) {
                        // Command not found - already printed in child
                    }
                } 
                
                // Child closed due to signal
                else if (WIFSIGNALED(status)) {
                    fprintf(stderr, "Child terminated by signal %d\n", WTERMSIG(status));
                }
            } 
            
            // Background process
            else {
                printf("[%d] Running in background\n", pid);
                fflush(stdout);
            }
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    FILE *in = stdin;
    if (argc == 2) {
        in = fopen(argv[1], "r");
        if (!in) { perror("fopen"); return 1; }
    } else if (argc > 2) {
        fprintf(stderr, "Usage: %s [batchfile]\n", argv[0]);
        return 1;
    }

    char line[MAX_LINE];
    char cwd[PATH_MAX];

    while (1) {
        reap_zombies();
        
        // Get current working directory
        if (getcwd(cwd, sizeof(cwd)) == NULL) { perror("getcwd"); break; }

        // Set environment variable "shell" to the full path of myshell
        char path[100]="shell=";
        char *input = "/myshell";
            
        input = strcat(cwd, input);
        putenv(strcat(path, input));

        if (in == stdin) {
            // Prompt user to enter
            printf(">myshell:%s$ ", cwd);
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), in)) break; // EOF => exit (batch requirement)

        if (process_line(line) == 0) break; // Empty line
    }

    if (in != stdin) fclose(in); // Close the infile if the provided file does not use stdin
    return 0;
}
