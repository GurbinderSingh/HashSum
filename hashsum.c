/**
 * @file hashsum.c
 * @author Gurbinder Singh
 * @date 2018-05-18
 *
 * @brief Implementation of the "hashsum" command.
 *        Takes a directory as an argument and displays
 *        the md5 hash and type of every file in directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/types.h>


#define BUFLEN 200

char *progName = NULL;
char *ignorePrefix = NULL;
char *directory = NULL;
char *fileNames = NULL;

static void parseArg(int, char**);
static void createPipes(int*, int*, int*);
static int prefixDiff(char*);

static void usage(void);
static void exit_error(int, char*);
static void cleanup(void);

static void getFileNames(int);
static void getMD5hash(int*, char*);
static void getFileType(int*, char*);
static void getFilePath(char*, char*);

static void runLS(int*);
static void runMD5(int*, char*);
static void runFile(int*, char*);




/**
 * @brief Does the setup and executes the commands.
 * 
 * @param argc The number of arguments in argv.
 * @param argv The arguments passed hashsum.
 * 
 * @return On success returns EXIT_SUCCESS otherwise EXIT_FAILURE.
 */
int main(int argc, char **argv)
{
    int pipe_ls[2];
    int pipe_md5[2];
    int pipe_file[2];
    
    
    
    parseArg(argc, argv);
    
    createPipes(pipe_ls, pipe_md5, pipe_file);
    
    
    pid_t pid_ls = fork();
    if(pid_ls == 0)   // child process
    {
        runLS(pipe_ls);
    }
    else if(pid_ls > 0)   // parent process
    {
        int status = -1;
        
        
        if(-1 == wait(&status))
        {
            exit_error(EXIT_FAILURE, "Child (ls) could not be terminated");
        }
        int exitStat_ls = WEXITSTATUS(status);
        if(exitStat_ls != EXIT_SUCCESS)
        {
            usage();
        }
        
        getFileNames(pipe_ls[0]);
        
        for(int i = 0; fileNames[i] != '\0'; i++)
        {
            char fileName[BUFLEN] = "";
            char filePath[BUFLEN] = "";
            
            
            for(int j = 0; fileNames[i] != '\n'; j++, i++)
            {
                fileName[j] = fileNames[i];
            }
            if(0 == prefixDiff(fileName))
            {
                continue;
            }
            
            getFilePath(fileName, filePath);
            
            pid_t pid_md5 = fork();
            if(pid_md5 == 0)   // child
            {
                runMD5(pipe_md5, filePath);
            }
            else if(pid_md5 > 0)
            {
                if(-1 == wait(&status))
                {
                    exit_error(EXIT_FAILURE, "Child (md5sum) could not be terminated");
                }
                int exitStat_md5 = WEXITSTATUS(status);
                if(exitStat_md5 != EXIT_SUCCESS)
                {
                    continue;
                }
                
                char hash[BUFLEN] = "";
                
                getMD5hash(pipe_md5, hash);
                
                
                pid_t pid_file = fork();
                
                if(pid_file == 0)    // child
                {
                    runFile(pipe_file, filePath);
                }
                else if(pid_file > 0)   // parent
                {
                    if(-1 == wait(&status))
                    {
                        exit_error(EXIT_FAILURE, "Child (file) could not be terminated");
                    }
                    int exitStat_file = WEXITSTATUS(status);
                    if(exitStat_file != EXIT_SUCCESS)
                    {
                        continue;
                    }
                    
                    char fileType[BUFLEN] = "";
                    
                    getFileType(pipe_file, fileType);
                    
                    printf("%s %s %s", fileName, hash, fileType);
                }
                else
                {
                    exit_error(EXIT_FAILURE, "Failed to create child process (file)");
                }
            }
            else
            {
                exit_error(EXIT_FAILURE, "Failed to create child process (md5sum)");
            }
        }
    }
    else
    {
        exit_error(EXIT_FAILURE, "Failed to create child process (ls)");
    }
    
    cleanup();
    
    return EXIT_SUCCESS;
}



/**
 * @brief Checks if there are any differences in the supplied 
 *        file name and the prefix passed as a argument.
 * 
 * @param fileName The name of the file to be compared against a prefix.
 * 
 * @return Returns the number of differences, or -1 if no prefix was supplied.
 */
static int prefixDiff(char *fileName)
{
    if(ignorePrefix == NULL)
    {
        return -1;
    }
    
    int diff = 0;
    
    for(int i = 0; ignorePrefix[i] != '\0'; i++)
    {
        if(ignorePrefix[i] != fileName[i] && (ignorePrefix[i] + 32) != fileName[i] && ignorePrefix[i] != (fileName[i] + 32))
        {
            diff++;
        }
    }
    
    return diff;
}



/**
 * @brief Reads the file type from a pipe.
 * 
 * @param pipe_file Is the pipe from which you want to read.
 * @param typeBuffer Is a variable where the result is stored.
 */
static void getFileType(int *pipe_file, char *typeBuffer)
{
    int readBytes = read(pipe_file[0], typeBuffer, BUFLEN - 1);
    
    
    if(readBytes < 0)
    {
        exit_error(EXIT_FAILURE, "Failed to get file output");
    }
}



/**
 * @brief Reads the md5 hash from a pipe.
 * 
 * @param pipe_md5 Is the pipe from which you want to read.
 * @param hashBuffer Is a variable in which the result is stored
 */
static void getMD5hash(int *pipe_md5, char *hashBuffer)
{
    char temp[BUFLEN] = "";
    int readBytes = read(pipe_md5[0], temp, sizeof(temp) - 1);
    
    
    if(readBytes < 0)
    {
        exit_error(EXIT_FAILURE, "Failed to get md5sum output");
    }
    else if(readBytes > 0)
    {
        for(int i = 0; temp[i] != '\0' && temp[i] != ' '; i++)
        {
            hashBuffer[i] = temp[i];
        }
    }
}




/**
 * @brief Reads the file names from a pipe and stores them in a global string.
 * 
 * @param read_pipefd Is the pipe from which you want to read.
 */
static void getFileNames(int read_pipefd)
{
    int readBytes = 0;
    int size_fN = BUFLEN;    // this is to keep track of the size of the list array fileNames
    
    
    fileNames = calloc(size_fN, sizeof(char));    // create an emtpy array with some basic size to keep a "list"
    
    if(fileNames == NULL)
    {
        exit_error(EXIT_FAILURE, "Failed to allocate memory");
    }
    do
    {
        char buffer[BUFLEN] = "";
        
        readBytes = read(read_pipefd, buffer, sizeof(buffer) - 1);    // we read as many chars as fit in the buffer
        
        if(readBytes != -1)
        {
            strcat(fileNames, buffer);    // append them at the end of the list array
        }
        else
        {
            exit_error(EXIT_FAILURE, "Failed to read ls output");
        }
        
        if(readBytes == BUFLEN - 1)    // if the buffer was completely filled => high probability that there is more to come
        {
            fileNames = realloc(fileNames, size_fN += BUFLEN);    // so we increase the size of our list array
        }
        
    }
    while(readBytes == BUFLEN - 1);    // if no chars were read, we are done
    
}



/**
 * @brief Creates a pipe for every child process.
 * 
 * @param pipe_ls The pipe for the ls command.
 * @param pipe_md5 The pipe for the md5sum command.
 * @param pipe_file The pipe for the file command.
 */
static void createPipes(int *pipe_ls, int *pipe_md5, int *pipe_file)
{
    if(-1 == pipe(pipe_ls))
    {
        exit_error(EXIT_FAILURE, "Pipe creation failed");
    }
    if(-1 == (pipe(pipe_md5)))
    {
        exit_error(EXIT_FAILURE, "Pipe creation failed");
    }
    if(-1 == (pipe(pipe_file)))
    {
        exit_error(EXIT_FAILURE, "Pipe creation failed");
    }
}



/**
 * @brief Takes a file name and retuns the full path.
 * 
 * @param fileName Is the name of the file.
 * @param filePath Is a variable where the path will be stored.
 */
static void getFilePath(char *fileName, char *filePath)
{
    if(NULL == strcpy(filePath, directory))
    {
        exit_error(EXIT_FAILURE, "A problem occured while fetching file path");
    }
    if(NULL == strcat(filePath, "/"))
    {
        exit_error(EXIT_FAILURE, "A problem occured while fetching file path");
    }
    if(NULL == strcat(filePath, fileName))
    {
        exit_error(EXIT_FAILURE, "A problem occured while fetching file path");
    }
}




/**
 * @brief Redirects the output from stdout to a pipe and runs the "file" command.
 * 
 * @param pipefd Is the pipe where the stdout ouput will be redirected to.
 * @param filePath is the full path of the file who's information you want.
 */
static void runFile(int *pipefd, char *filePath)
{
    if(-1 == dup2(pipefd[1], STDOUT_FILENO))
    {
        exit_error(EXIT_FAILURE, "STDOUT redirection failed");
    }
    if(-1 == close(pipefd[0]))
    {
        exit_error(EXIT_FAILURE, "Failed to close pipe");
    }
    
    if(-1 == execlp("file", "file", "-b", filePath, (char*) NULL))
    {
        exit_error(EXIT_FAILURE, "Failed to execute \"file\" command");
    }
}




/**
 * @brief Redirects the output from stdout to a pipe and runs the "md5sum" command.
 * 
 * @param pipefd Is the pipe where the stdout ouput will be redirected to.
 * @param filePath is the full path of the file who's information you want.
 */
static void runMD5(int *pipefd, char *filePath)
{
    if(-1 == dup2(pipefd[1], STDOUT_FILENO))
    {
        exit_error(EXIT_FAILURE, "STDOUT redirection failed");
    }
    if(-1 == close(pipefd[0]))
    {
        exit_error(EXIT_FAILURE, "Failed to close pipe");
    }
    if(-1 == close(STDERR_FILENO))
    {
        exit_error(EXIT_FAILURE, "Failed to close stderr");
    }
    
    if(-1 == execlp("md5sum", "md5sum", filePath, (char*) NULL))
    {
        exit_error(EXIT_FAILURE, "Failed to execute \"md5sum\" command");
    }
    cleanup();
    exit(EXIT_SUCCESS);
}




/**
 * @brief Redirects the output from stdout to a pipe and runs the "ls" command.
 * 
 * @param pipefd Is the pipe where the stdout ouput will be redirected to.
 */
static void runLS(int *pipefd)
{
    if(-1 == dup2(pipefd[1], STDOUT_FILENO))
    {
        exit_error(EXIT_FAILURE, "STDOUT redirection failed");
    }
    if(-1 == close(pipefd[0]))
    {
        exit_error(EXIT_FAILURE, "Failed to close pipe");
    }
    if(-1 == execlp("ls", "ls", "-1a", directory, (char*) NULL))
    {
        exit_error(EXIT_FAILURE, "Failed to execute \"ls\" command");
    }
    cleanup();
    exit(EXIT_SUCCESS);
}




/**
 * @brief Parses the arguments passed to the program.
 * 
 * @param argc Is the number of arguments in argv.
 * @param argv Is the array containing the arguments.
 */
static void parseArg(int argc, char **argv)
{
    int opt = 0;
    int opt_i = 0;
    
    
    progName = argv[0];
    
    while(-1 != (opt = getopt(argc, argv, "i:")))
    {
        switch(opt)
        {
            case 'i':
                if(opt_i != 0)
                {
                    usage();
                }
                opt_i = 1;
                
                ignorePrefix = optarg;
                break;
                
            default:
                usage();
                assert(0);
                break;
        }
    }
    if(argc <= optind)
    {
        usage();
    }
    directory = argv[optind];
}



/**
 * @brief Displays the usage message and exits the program.
 */
static void usage(void)
{
    fprintf(stderr, "Usage: %s [-i ignoreprefix] <directory>\n", progName);
    cleanup();
    exit(EXIT_FAILURE);
}


/**
 * @brief Frees all resources.
 */
static void cleanup()
{
    free(fileNames);
}


/**
 * @brief Prints an error message to stderr, after performin a clean up of resources.
 * 
 * @param exitCode Is the exit code with which to terminate the program.
 * @param msg Is the message to be printed to stderr.
 */
static void exit_error(int exitCode, char *msg)
{
    cleanup();
    
    if (msg != NULL)
    {
        fprintf(stderr, "%s: ", progName);
        fprintf(stderr, "%s\n", msg);
    }
    if (errno != 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
    }
    cleanup();
    exit(exitCode);
}


























