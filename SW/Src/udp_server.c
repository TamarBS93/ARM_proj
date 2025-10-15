/**
  * @brief Testing Program for UUT Peripherals
  * 
  * The Program is creating a connection to a UUT through ethernet and sends a testing command.
  * The test result is logged according to its ID (generated automatically) and includes testing time. 
  * 
  * @param 1: Peripheral to test (UART/I2C/SPI/TIMER/ADC)
  * @param 2: Number of iterations to test 
  * @param 3: A testing character pattern - Not mandatory
  * @retval None
  */
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Project_header.h"
#define COUNT_FILE "calls_count.txt"
#define LOG_FILE "testing_log.txt"
#define COMMAND_ERR 0

test_command_t test_request_init(int argc, char *argv[]);
int get_id_num();
int file_exists(const char *filename);
void logging(result_pro_t result, struct timeval sent, double duration);

int main(int argc, char *argv[])
{
    // Check the amount of arguments that were provided besides the program name
    if (argc < 3) {
        printf("Not enough arguments: Please specify a Peripheral, and number of iterations to check and a pattern\n");
        return 1;
    }
    else if(argc > 4){
        printf("Too many arguments\n");
        return 1;
    }

    int sockfd;
    struct sockaddr_in server_addr, uut_addr;
    socklen_t addr_len = sizeof(uut_addr);

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server on port %d.\n", SERVER_PORT);
    
    test_command_t test_pack = test_request_init(argc,argv);
    if (test_pack.peripheral == COMMAND_ERR || test_pack.iterations == COMMAND_ERR) return 1;
    
    result_pro_t result_pack;
    FILE * logging_fd;
    uint8_t i;

    // Set STM32 target address
    memset(&uut_addr, 0, sizeof(uut_addr));
    uut_addr.sin_family = AF_INET;
    uut_addr.sin_port = htons(5005);  // Port STM32 is listening on

    // STM32 IP address:
    uut_addr.sin_addr.s_addr = inet_addr(CLIENT_IP);

    struct timeval sent_time;
    struct timeval recv_time;
    double sent_sec, test_len;

    // Send message
    ssize_t sent_bytes = sendto(sockfd, (const void *)&test_pack, sizeof(test_pack), 0,
                            (struct sockaddr*)&uut_addr, sizeof(uut_addr));
    if (sent_bytes < 0) 
    {
        perror("sendto failed");
        close(sockfd);
        return 1;
    }
    else {
        // Get the current time
        gettimeofday(&sent_time, NULL);
        sent_sec = sent_time.tv_sec + (double)sent_time.tv_usec / 1000000.0;
    }
      
    // Receive message from UUT
    int n = recvfrom(sockfd, &result_pack, sizeof(result_pro_t), 0, (struct sockaddr *)&uut_addr, &addr_len);
    if (n < 0){
        perror("Receive failed\n");
        close(sockfd);
        return 1;
    }
    else{
        gettimeofday(&recv_time, NULL);
        test_len = (recv_time.tv_sec + (double)recv_time.tv_usec / 1000000.0) - sent_sec;
    }
   
    logging(result_pack, sent_time, test_len);
    close(sockfd);
    return 0;
}

test_command_t test_request_init(int argc, char *argv[]){
    
    test_command_t test_request;

    if (strcmp(argv[1], "TIMER") == 0) test_request.peripheral =  TIMER;
    else if (strcmp(argv[1], "UART") == 0) test_request.peripheral = UART;
    else if (strcmp(argv[1], "SPI") == 0) test_request.peripheral = SPI;
    else if (strcmp(argv[1], "I2C") == 0) test_request.peripheral = I2C;
    else if (strcmp(argv[1], "ADC") == 0) test_request.peripheral = ADC_P;
    else {
        printf("Invalid peripheral input.\n");
        test_request.peripheral = COMMAND_ERR;
        return test_request; 
    }

    test_request.iterations = atoi(argv[2]);
    if (test_request.iterations < 1){
        printf("Invalid Iterations input.\n");
        test_request.iterations = COMMAND_ERR;
        return test_request; 
    }

    test_request.test_id =  get_id_num();
    printf("Testing UUT's %s peripheral with %d iterations:\n", argv[1], test_request.iterations);

    int len;
    if (argc==4){
        len = strlen(argv[3]);
        strncpy(test_request.bit_pattern, argv[3], len);
    }
    else{
        strncpy(test_request.bit_pattern, "This is the testing pattern!", 28);
        len = 28;
    }
    test_request.bit_pattern[len] = '\0'; 
    test_request.bit_pattern_length = len+1;
    memset(test_request.bit_pattern + test_request.bit_pattern_length, 0, MAX_BIT_PATTERN_LENGTH - test_request.bit_pattern_length);

    return test_request;
}

int get_id_num(){
    FILE *file_ptr;
    int current_count = 0;

    if (file_exists(COUNT_FILE)) {
        // Open the file in read mode.
        file_ptr = fopen(COUNT_FILE, "r");
        fscanf(file_ptr, "%d", &current_count);
        // Close the file after reading.
        fclose(file_ptr);
    }
    // If the file doesn't exist yet (this is the first run), so we'll start with the default count of 0.

    // Increment the count and print the result
    current_count++;
    printf("Test ID %d:\n", current_count);
    
    // Write the new count back to the file
    file_ptr = fopen(COUNT_FILE, "w");

    // Check if the file was opened for writing successfully.
    if (file_ptr != NULL) {
        // Write the new count back to the file.
        fprintf(file_ptr, "%d", current_count);
        // Close the file after writing.
        fclose(file_ptr);
    } else {
        // Handle the case where the file can't be created or written to.
        perror("Error: Could not write to the count file.");
        return 1;
    }
    return current_count;
}

// Function to check if a file exists
int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Logging:
void logging(result_pro_t result, struct timeval sent, double duration){

    // Convert the seconds part of the sent value to a calendar time structure
    struct tm *tm_info = localtime(&sent.tv_sec);
    // Create a buffer to hold the formatted string
    char time_str[100];
    // Format the date and time part using strftime()
    strftime(time_str, sizeof(time_str), "%d-%m-%Y %H:%M:%S", tm_info);

    const char *result_str;
    switch (result.test_result)
    {
    case TEST_ERR:
        result_str = "TEST_ERR";
        printf("An error occured while executing the test. please try again");
        break;
    case TEST_PASS:
        result_str = "TEST_PASS";
        break;
    case TEST_FAIL:
        result_str = "TEST_FAIL";
        break;
    default:
        perror("Error with test_result");
        return;
        break;
    }
    
    FILE *logging_fd;

    // Open the file to write the header if it doesn't exist.
    if (!file_exists(LOG_FILE)) {
        logging_fd = fopen(LOG_FILE, "w");
        if (logging_fd) {
            fprintf(logging_fd, "%-9s %-25s %-15s %s\n", "Test ID", "Sent At", "Result", "Duration (s)");
            fclose(logging_fd);
        } else {
            perror("Error: Could not open log file for writing header");
            return; // Exit the function on error
        }
    }
    
    printf("%s\n", result_str);

    logging_fd = fopen(LOG_FILE, "a");
    if (logging_fd) {
        fprintf(logging_fd, "%-9d %-25s %-15s %f\n", result.test_id, time_str, result_str, duration);
        fflush(logging_fd);
        fclose(logging_fd);
    } else {
        perror("Error: Could not open log file for appending");
    }   
}