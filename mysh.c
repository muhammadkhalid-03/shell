#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// shell implementation in c

// This is the maximum number of arguments your shell should handle for one command
#define MAX_ARGS 128

/**
 * removes all leading/trailing spaces
 */
void remove_outer_spaces(char *word)
{
  int skip_pos = 0;
  // remove leading spaces
  while (word[skip_pos] == ' ')
  {
    skip_pos++;
  }
  word = word + skip_pos;
  // remove trialing spaces
  int del_pos = 0;
  int len = strlen(word);
  while (word[len - 1 - del_pos] == ' ')
  {
    del_pos++;
  }
  word[len - del_pos] = '\0';
}

/**
 * Splits the string into an array of strings where any delimiter char is found
 */
char **split_string(char *str, char *delimiter)
{
  char *current_position = str;
  char **arguments = malloc(sizeof(char *) * (MAX_ARGS + 1));
  int total_args = 0;
  while (true)
  {
    if (total_args == MAX_ARGS)
    {
      printf("Maximum Arguments Reached");
      exit(EXIT_FAILURE);
    }
    // Call strpbrk to find the next occurrence of a delimeter
    char *delim_position = strpbrk(current_position, delimiter);
    // if delimiter was at the start, skip it
    if (delim_position == current_position)
    {
      current_position++;
      continue;
    }

    if (delim_position == NULL)
    {
      // There were no more delimeters.

      char *delim_position = strpbrk(current_position, "\n");
      // if we have a newline, replace it with null character
      if (delim_position != NULL)
      {
        *delim_position = '\0';
      }
      arguments[total_args] = current_position;
      // printf("Look here: %ssadf\n", arguments[total_args]);
      total_args++;
      arguments[total_args] = NULL;

      return arguments;
    }
    else
    {
      // Overwrite the delimeter with a null terminator so we can print just this fragment
      *delim_position = '\0';
      arguments[total_args] = current_position;
      total_args++;
    }

    // Move our current position in the string to one character past the delimeter
    current_position = delim_position + 1;
  }
}

/**
 * Collects and displays all finished background processes
 */
void collect_exits()
{
  int wstatus;
  // get any running or finished child process
  pid_t child_id = waitpid(-1, &wstatus, WNOHANG);
  // while finished child processes exist
  while (child_id != 0)
  {
    // no child process found
    if (child_id == -1)
    {
      return;
      // child process finished running
    }
    else if (child_id > 0)
    {
      printf("[background process %d exited with status %d]\n", child_id, WEXITSTATUS(wstatus));
      // get next child process
      child_id = waitpid(-1, &wstatus, WNOHANG);
    }
  }
}

/**
 * Handles arguments one by one depending on whether its a foreground or background command
 */
void handle_arg(char **arguments, int bg_flag)
{ // handles single command

  if (strcmp(arguments[0], "\0") == 0)
  { // check for empty input
    return;
  }
  if (strcmp(arguments[0], "exit") == 0)
  { // check for exit command
    exit(EXIT_SUCCESS);
  }
  if (strcmp(arguments[0], "cd") == 0)
  { // check for cd command
    chdir(arguments[1]);
    return;
  }
  pid_t child_id = fork();
  if (child_id == -1)
  { // check for fork error
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  if (child_id == 0)
  {
    // In the child
    int pos_err = execvp(arguments[0], arguments);
    if (pos_err < 0)
    {
      perror("Exec failed");
      exit(EXIT_FAILURE);
    }
    exit(6);
  }
  else
  {
    // In the parent
    int wstatus;
    if (bg_flag == 0)
    {
      // wait for the child process with pid child_id to finish
      if (waitpid(child_id, &wstatus, 0) == -1)
      {
        perror("wait failed");
        exit(EXIT_FAILURE);
      }
      printf("[%s exited with status %d]\n", arguments[0], WEXITSTATUS(wstatus));
    }
    collect_exits();
  }
  return;
}

int main(int argc, char **argv)
{
  // If there was a command line option passed in, use that file instead of stdin
  if (argc == 2)
  {
    // Try to open the file
    int new_input = open(argv[1], O_RDONLY);
    if (new_input == -1)
    {
      fprintf(stderr, "Failed to open input file %s\n", argv[1]);
      exit(1);
    }

    // Now swap this file in and use it as stdin
    if (dup2(new_input, STDIN_FILENO) == -1)
    {
      fprintf(stderr, "Failed to set new file as input\n");
      exit(2);
    }
  }

  char *line = NULL;    // Pointer that will hold the line we read in
  size_t line_size = 0; // The number of bytes available in line
  char **arguments;

  // Loop forever
  while (true)
  {
    // Print the shell prompt
    printf("$ ");

    // Get a line of stdin, storing the string pointer in line
    if (getline(&line, &line_size, stdin) == -1)
    {
      if (errno == EINVAL)
      {
        perror("Unable to read command line");
        exit(2);
      }
      else
      {
        // Must have been end of file (ctrl+leep 1)
        printf("\nShutting down...\n");

        // Exit the infinite loop
        break;
      }
    }

    arguments = split_string(line, ";"); // array storing all arguments
    int cur_arg_pos = 0;

    while (arguments[cur_arg_pos] != NULL)
    {
      // remove leading/trailing space and save ptr
      remove_outer_spaces(arguments[cur_arg_pos]);

      char *amp_position = strpbrk(arguments[cur_arg_pos], "&");

      while (amp_position != NULL)
      { // & exists
        // get space between start of argument and &
        size_t len = amp_position - arguments[cur_arg_pos];
        *amp_position = '\0';
        // allocate space to copy the argument excluding the &
        char *dest_space = malloc(sizeof(char) * (len));
        // copy and clean the argument
        strcpy(dest_space, arguments[cur_arg_pos]);
        remove_outer_spaces(dest_space);
        //  split it into its parts and handle with a flag
        //  for background processes
        char **split_dest = split_string(dest_space, " ");
        handle_arg(split_dest, 1);
        // move the pointer for the current argument to after the &
        arguments[cur_arg_pos] = amp_position + 1;
        amp_position = strpbrk(arguments[cur_arg_pos], "&");
        free(dest_space);
      }
      // clean and split args
      remove_outer_spaces(arguments[cur_arg_pos]);
      char **cur_arg = split_string(arguments[cur_arg_pos], " ");
      cur_arg_pos++;
      // skip over non-arguments
      if (strcmp(cur_arg[0], "\0") == 0)
      {
        continue;
      }
      handle_arg(cur_arg, 0);
    }
  }

  // If we read in at least one line, free this space
  if (line != NULL)
  {
    free(line);
  }
  return 0;
}
