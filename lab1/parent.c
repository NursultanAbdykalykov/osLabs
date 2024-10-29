#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 2048

int main() {
    int pipe1[2];
    pid_t pid;
    char buffer[BUFFER_SIZE];
    char filename[100];
    ssize_t bytes_read, bytes_written;

    // Создание pipe1
    if (pipe(pipe1) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Сообщение пользователю для ввода имени файла
    const char *msg = "Введите имя файла: ";
    bytes_written = write(STDOUT_FILENO, msg, strlen(msg));
    if (bytes_written == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    // Чтение имени файла с консоли
    bytes_read = read(STDIN_FILENO, filename, sizeof(filename) - 1);
    if (bytes_read == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // Удаление символа новой строки, если он был введен
    if (bytes_read > 0 && filename[bytes_read - 1] == '\n') {
        filename[bytes_read - 1] = '\0';
    }

    // Создание дочернего процесса
    pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Дочерний процесс
        close(pipe1[1]);  // Закрываем запись в pipe для дочернего процесса

        // Перенаправление стандартного ввода на pipe
        dup2(pipe1[0], STDIN_FILENO);
        close(pipe1[0]);

        // Запуск дочерней программы
        execlp("./child", "child", filename, NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс
        close(pipe1[0]);  // Закрываем чтение из pipe для родительского процесса

        // Сообщение для ввода чисел
        const char *msg2 = "Введите числа через пробел: ";
        write(STDOUT_FILENO, msg2, strlen(msg2));

        // Чтение чисел с консоли
        bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Отправляем данные в pipe
        write(pipe1[1], buffer, bytes_read);
        close(pipe1[1]);  // Закрываем запись после отправки данных

        // Ожидание завершения дочернего процесса
        wait(NULL);
    }

    return 0;
}
