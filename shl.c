// Compile with gcc -o shl shl.c

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CSH_RL_BUFSIZE 1024
#define CSH_TOK_BUFSIZE 64
#define CSH_TOK_DELIM " \t\r\n\a"

int csh_execute(char **args);
/*
  Function Declarations for builtin shell commands:
  I still don't understand why we need it.
 */
int csh_cd(char **args);
int csh_help(char **args);
int csh_exit(char **args);
int csh_num_builtins();
int csh_cd(char **args);
int csh_help(char **args);
int csh_exit(char **args);
int csh_launch(char **args);
char** csh_split_line(char *line);
char* csh_read_line(void);
void csh_loop(void);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};

/*
  - builtin_func is an array of function pointers.
  - Each function pointer points to a function that takes an array of
    strings (pointer to a pointer to a char) and returns an integer.
*/
int (*builtin_func[]) (char **) = {
  &csh_cd,
  &csh_help,
  &csh_exit
};

/**
 * @brief This is the main entry point
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return status code
 */
int main(int argc, char **argv)
{
  // Run command loop
  csh_loop();

  return EXIT_SUCCESS;
}

// ------------------------------------------------------------------------
// Definitions
int csh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < csh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return csh_launch(args);
}

// ------------------ Built-in shell commands ------------------
/*
  Count builtins
*/
int csh_num_builtins() 
{
  return sizeof(builtin_str) / sizeof(char *);
}

int csh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "csh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("csh");
    }
  }
  return 1;
}

int csh_help(char **args)
{
  int i;
  printf("Amedeo Chiefa's Shell\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < csh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

int csh_exit(char **args)
{
  return 0;
}
// -----------------------------------------------

/**
 * @brief Shell startup
 * 
 * @param args 
 * @return int 
 */
int csh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  // Create the child process
  // The child process and the parent process run in separate memory
  // spaces.  At the time of fork() both memory spaces have the same
  // content.
  pid = fork(); // From this point we have two processes running

  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("csh"); // Print the error message with the name of the program
    }
    exit(EXIT_FAILURE); // Exit so that the shell can continue
  } else if (pid < 0) {
    // Error forking
    perror("csh");
  } else { // fork executed successfully
    // Parent process
    do {
      wpid = waitpid(pid, &status, WUNTRACED); // Wait for the child process to finish
    } while(!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
 * @brief Parse the line into a list of arguments
 * 
 * Quoting and backslash escaping are not supported. Only whitespaces
 * are supported to separate the arguments. We then "tokenize" the string
 * using whitespaces as delimeter through strtok.
 * 
 * It returns an array of tokens, where the last element is NULL. These
 * tokens are ready to be executed by the shell.
 * 
 * @param line 
 * @return char** 
 */
char** csh_split_line(char *line)
{
  int bufsize = CSH_TOK_BUFSIZE;
  int position = 0;
  char **tokens = malloc(bufsize * sizeof(char*)); // array of pointers
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  // Note that strtok modifies the original string.
  // Moreover, strtok returns a pointer to the first token.
  // That's why need the array of pointers `tokens`.
  // In particular, `strtok` stores the pointer internally,
  // so that `strtok(NULL, CSH_TOK_DELIM)` returns the pointer
  // to the string it left off. Citing cplusplus.com:
  // "Alternativelly, a null pointer may be specified, in which case the 
  // function continues scanning where a previous successful call to the function
  // ended." and "A null pointer is always returned when the end of the string 
  // i.e., a null character) is reached in the string being scanned.".
  token = strtok(line, CSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    // Check that the buffer is big enough
    if (position >= bufsize) {
      bufsize += CSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "csh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, CSH_TOK_DELIM);
  }
  
  tokens[position] = NULL;
  return tokens;
}

/**
 * @brief Read the line from stdin
 * 
 * Reading the line from stdin is not simple in C.
 * The reason being that we can't anticipate how much
 * text the user will be using, and the block that we
 * have allocated may not be enough. We need to dynamically
 * check that.
 * 
 * In `stdin.h` there is the function `getline` that does
 * this job for us. The could would be
 * 
 * ```
 * char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us

  if (getline(&line, &bufsize, stdin) == -1){
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);  // We recieved an EOF
    } else  {
      perror("readline");
      exit(EXIT_FAILURE);
    }
  }

  return line;
  ```
 * 
 * @return char* 
 */
char* csh_read_line(void)
{
  int bufsize = CSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "csh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read the character
    // Note, we store tha character as an int, 
    // because EOF is an integer.
    c = getchar(); 

    // If we hit the EOF, replace it with a null character and return
    if (c == EOF || c == '\n') {
      buffer[position] = '\0'; // null character
      return buffer;
    }
    else {
      buffer[position] = c;
    }
    position++;
    
    // Check whether the next character will go outside of the
    // current buffer size. If so, reallocate
    if (position >= bufsize) {
      bufsize += CSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "csh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

/**
 * @brief The main shell loop
 * 
 */
void csh_loop(void)
{
  char *line;
  char **args;
  int status;
  
  do {
    printf("> ");
    line = csh_read_line();
    args = csh_split_line(line);
    status = csh_execute(args);

    free(line);
    free(args);
  } while (status);
}