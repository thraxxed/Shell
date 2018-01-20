#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>


#define ARRAY_SIZE 1<<16


extern char ** get_args();


void run_process (char * name, char ** args) {
   pid_t child = fork();
   pid_t tpid;
   int status;
   if (child == 0) {
      execvp(args[0], args);
      // If this next line is reached, execvp failed.
      printf("myshell: %s: command not found\n", args[0]);
   }
   else {
   // Now we wait for the child process to terminate.
      do {
         tpid = wait(&status);
         if (tpid == -1) {
            printf("Something went wrong!\n");
         }
      } while (tpid != child);
   }
}


// Check for special characters and handle them
void parse_args (char ** args) {


   int input_file, out_file;
   int i;
   char * first_part[ARRAY_SIZE];
   // Declare new array of only args we want to keep
   char * wanted[ARRAY_SIZE];
   int wanted_index = 0;


   for (i = 0; args[i] != NULL; i++) {
      //
      // Check for ';' (semicolon)
      //
      if (args[i][0] == ';') {
         // Parse everything before the ; then everything after
         // Partition args into [0 .. i-1] and [i+1 .. n]
         for (int j = 0; j < i; ++j) {
            first_part[j] = args[j];
         }
         first_part[i] = NULL;
         parse_args(first_part);
         parse_args(&args[i+1]);
         return;
      }
   }


   for (i = 0; args[i] != NULL; i++) {
      //
      // Check for | (pipe)
      //
      if (args[i][0] == '|') {
         int fd[2];
         int status, pid;
         for (int j = 0; j < i; ++j) first_part[j] = args[j];
         first_part[i] = NULL;
         if (first_part[0] == NULL) {
            printf("myshell: syntax error near unexpected token '|'\n");
            break;
         }
         // Open a pipe
         pipe(fd);


         // Start the first process
         switch (pid = fork()) {
         case 0:
            // Pipe stdout to output
            dup2(fd[1],1);
            if (args[i][1] == '&') dup2(fd[1],2);
            close(fd[0]); // Close unused part of pipe
            parse_args(first_part);
            exit(0);
         default: break;
         case -1:
            perror("fork eror");
            exit(1);
         }
         while ((pid = wait(&status)) != -1) {}


         // Start the second process
         switch (pid = fork()) {
         case 0:
            // Pipe input to stdin
            dup2(fd[0],0);
            close(fd[1]); // Close unused part of pipe
            parse_args(&args[i+1]);
            exit(0);
         default: break;
         case -1:
            perror("fork eror");
            exit(1);
         }
         close(fd[0]);
         close(fd[1]);
         // Wait for both processes to end
         while ((pid = wait(&status)) != -1) {}
         return;
      }
   }
   // If we make it to this point
   // There are no pipes or semicolons


   for (i = 0; args[i] != NULL; i++) {
      //
      // Check for "<" Character
      //
      if (args[i][0] ==  '<') {
         input_file = open(args[i+1], O_RDONLY);
         if (input_file == -1) {
            printf("myshell: Failed to open file!\n");
            i++;
            break;
         }
         else {
            dup2(input_file, 0);
            i++;
         }
         close(input_file);
      }
      //
      // Check for ">" Character
      //
      else if (args[i][0] == '>') {
         // By default, we want to TRUNCATE and EXCLUSIVE mode,
         // meaning if the file already exists there's an error.
         int open_type = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL;
         // If ">>" form is used, change open flags to append mode
         if (args[i][1] == '>') open_type = O_CREAT|O_APPEND|O_WRONLY;
         out_file = open(args[i+1], open_type, 0644);
         if (out_file == -1) {
            printf("myshell: Failed to open file!\n");
            i++;
            break;
         }
         else dup2(out_file, 1);
         // Also check for '&'
         if (args[i][1] == '&' || args[i][2] == '&') dup2(out_file, 2);
         i++;
         close(out_file);
      }
      //
      // Argument isn't a special character
      //
      else {
         wanted[wanted_index] = args[i];
         wanted_index++;
      }
   }
   wanted[i] = NULL;
   // Run the process with all superficial arguments removed
   run_process(args[0], wanted);
}


int main() {
    int         i;
    char **     args;
    char        og_cwd[PATH_MAX];


    // Used to restore stdin/stdout/stderr
    int         stdin_copy = dup(0);
    int         stdout_copy = dup(1);
    int         stderr_copy = dup(2);


    //Get the original cwd for the
    //arg-less "cd" command
    getcwd(og_cwd, PATH_MAX);


    // Main Loop
    while (1) {
       printf ("[MyShell]$ ");
       args = get_args();


       if (args[0] == NULL) continue;
       else if ( !strcmp (args[0], "exit")) {
            // "EXIT" COMMAND
            // -prints an exit message like bash
            printf("exit\n");
            close(stdin_copy);
            close(stdout_copy);
            exit(0);
            return 0;
       }
       else if ( !strcmp (args[0], "cd")) {
            // "CD" COMMAND
            //If there are no arguments to "cd"
            //we want to chdir to the original wd
            if (args[1] == NULL) chdir(og_cwd);
            //Ignores arguments to 'cd' other than the first
            else if (chdir(args[1]) == -1) {
               printf("myshell: cd: %s: No such file or directory\n", args[1]);
            }
       }
       else { //Run the command in args[0]
          // Iterate through the arguments
          // looking for special characters
          parse_args(args);
          // Return control of stidn/stdout to default
          dup2(stdin_copy, 0);
          dup2(stdout_copy, 1);
          dup2(stderr_copy, 2);
       }
    }
    free(args);
}