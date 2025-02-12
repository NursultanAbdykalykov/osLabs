#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define BUFFER_SIZE 2048

void convertStringToIntArray(const char *str, int intArray[], int *size) {
    int i = 0, num = 0;
    *size = 0;
    while (str[i] != '\n' && str[i] != '\0') {

        while (isspace(str[i])) {
            i++;
        }

        if (isdigit(str[i])) {
            num = 0;
            while (isdigit(str[i])) {
                num = num * 10 + (str[i] - '0');
                i++;
            }
            intArray[*size] = num;
            (*size)++;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char msg[] = "Usage: <program> <filename>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    // Открываем файл для записи результата
    char *filename = argv[1];
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Чтение данных из родительского процесса через pipe
    bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        const char msg[] = "error: failed to read from pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_read] = '\0'; 

    int array[BUFFER_SIZE];
    int size = 0;
    int sum = 0;

    convertStringToIntArray(buffer, array, &size);


    for (int i = 0; i < size; ++i) {
        sum += array[i];
    }

    // Запись
    char output[128];
    int len = snprintf(output, sizeof(output), "Сумма: %d\n", sum);
    if (write(file, output, len) == -1) {
        perror("write to file");
        close(file);
        exit(EXIT_FAILURE);
    }

    close(file);
    return 0;
}
