#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

void print_str(const char* s) { write(STDOUT_FILENO, s, strlen(s)); }

void print_error(const char* s) { write(STDERR_FILENO, s, strlen(s)); }

int read_line(char* buf, size_t size) {
	size_t i = 0;
	char c;
	ssize_t n;
	while (i < size - 1) {
		n = read(STDIN_FILENO, &c, 1);
		if (n < 0) return -1;  // ошибка чтения
		if (n == 0)            // EOF
			break;
		if (c == '\n') break;
		buf[i++] = c;
	}
	buf[i] = '\0';
	return (int)i;
}

void print_matrix(const char* title, double* m, int rows, int cols) {
	write(STDOUT_FILENO, title, strlen(title));
	write(STDOUT_FILENO, "\n", 1);
	char buf[128];
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < cols; j++) {
			int len = snprintf(buf, sizeof(buf), "%.2f ", m[i * cols + j]);
			if (len > 0) write(STDOUT_FILENO, buf, len);
		}
		write(STDOUT_FILENO, "\n", 1);
	}
}

typedef struct {
	int start_row;
	int end_row;
	int rows;
	int cols;
	int filter;  // 0 – эрозия, 1 – наращивание
	double* in;
	double* out;
} ThreadArgs;

void* compute_segment(void* arg) {
	ThreadArgs* args = (ThreadArgs*)arg;
	int rstart = args->start_row;
	int rend = args->end_row;
	int cols = args->cols;
	int total_rows = args->rows;
	for (int i = rstart; i < rend; i++) {
		for (int j = 0; j < cols; j++) {
			double result = args->in[i * cols + j];
			if (args->filter == 0) {
				// Эрозия: ищем минимальное значение
				for (int di = -1; di <= 1; di++) {
					for (int dj = -1; dj <= 1; dj++) {
						int ni = i + di;
						int nj = j + dj;
						if (ni >= 0 && ni < total_rows && nj >= 0 && nj < cols) {
							double val = args->in[ni * cols + nj];
							if (val < result) result = val;
						}
					}
				}
			} else {
				// Наращивание: ищем максимальное значение
				for (int di = -1; di <= 1; di++) {
					for (int dj = -1; dj <= 1; dj++) {
						int ni = i + di;
						int nj = j + dj;
						if (ni >= 0 && ni < total_rows && nj >= 0 && nj < cols) {
							double val = args->in[ni * cols + nj];
							if (val > result) result = val;
						}
					}
				}
			}
			args->out[i * cols + j] = result;
		}
	}
	return NULL;
}

int parse_double(const char* token, double* value) {
	char* endptr;
	*value = strtod(token, &endptr);
	return (endptr != token);
}

int main(int argc, char* argv[]) {
	if (argc < 5) {
		print_str("usage: program <max_threads> <K> <rows> <cols>\n");
		return 1;
	}

	const int max_threads = atoi(argv[1]);
	const int K = atoi(argv[2]);
	const int rows = atoi(argv[3]);
	const int cols = atoi(argv[4]);

	if (max_threads <= 0 || K <= 0 || rows <= 0 || cols <= 0) {
		print_str("Неправильно заданы параметры\n");
		return 1;
	}

	// Выделение памяти под исходную матрицу
	double* matrix = malloc(rows * cols * sizeof(double));
	if (!matrix) {
		print_str("Ошибка выделения памяти\n");
		return 1;
	}

	// Построчный ввод: читаем rows строк, каждая должна содержать ровно cols чисел
	char line[BUFFER_SIZE];
	for (int r = 0; r < rows; r++) {
		if (read_line(line, sizeof(line)) < 0) {
			perror("read_line");
			free(matrix);
			return 1;
		}
		int count = 0;
		char* saveptr;
		char* token = strtok_r(line, " \t", &saveptr);
		while (token != NULL) {
			if (count >= cols) {
				print_str("Введено слишком много столбцов\n");
				free(matrix);
				return 1;
			}
			if (!parse_double(token, &matrix[r * cols + count])) {
				print_error("Неправильное число в строке\n");
				free(matrix);
				return 1;
			}
			count++;
			token = strtok_r(NULL, " \t", &saveptr);
		}
		if (count != cols) {
			print_str("Введено слишком мало столбцов\n");
			free(matrix);
			return 1;
		}
	}

	// Выделение памяти под рабочие матрицы для эрозии и наращивания
	double* erosion_in = malloc(rows * cols * sizeof(double));
	double* erosion_out = malloc(rows * cols * sizeof(double));
	double* dilation_in = malloc(rows * cols * sizeof(double));
	double* dilation_out = malloc(rows * cols * sizeof(double));
	if (!erosion_in || !erosion_out || !dilation_in || !dilation_out) {
		print_str("Ошибка выделения памяти\n");
		free(matrix);
		free(erosion_in);
		free(erosion_out);
		free(dilation_in);
		free(dilation_out);
		return 1;
	}
	memcpy(erosion_in, matrix, rows * cols * sizeof(double));
	memcpy(dilation_in, matrix, rows * cols * sizeof(double));
	free(matrix);

	// Определяем число потоков: не больше max_threads и не больше числа строк
	int num_threads = (max_threads < rows) ? max_threads : rows;
	pthread_t threads[num_threads];
	ThreadArgs args[num_threads];

	// Замер времени исполнения
	struct timespec start_time, end_time;
	if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
		perror("clock_gettime");
		return 1;
	}

	// Эрозия: K итераций
	for (int iter = 0; iter < K; iter++) {
		int rows_per_thread = rows / num_threads;
		int remainder = rows % num_threads;
		int current_row = 0;
		for (int i = 0; i < num_threads; i++) {
			args[i].start_row = current_row;
			int extra = (i < remainder) ? 1 : 0;
			args[i].end_row = current_row + rows_per_thread + extra;
			current_row = args[i].end_row;
			args[i].rows = rows;
			args[i].cols = cols;
			args[i].filter = 0;  // эрозия
			args[i].in = erosion_in;
			args[i].out = erosion_out;
			if (pthread_create(&threads[i], NULL, compute_segment, &args[i]) != 0) {
				perror("pthread_create");
				free(erosion_in);
				free(erosion_out);
				free(dilation_in);
				free(dilation_out);
				return 1;
			}
		}
		for (int i = 0; i < num_threads; i++) {
			pthread_join(threads[i], NULL);
		}
		// Обмен указателей для следующей итерации
		double* temp = erosion_in;
		erosion_in = erosion_out;
		erosion_out = temp;
	}
	// Итоговый результат эрозии находится в erosion_in

	// Наращивание: K итераций
	for (int iter = 0; iter < K; iter++) {
		int rows_per_thread = rows / num_threads;
		int remainder = rows % num_threads;
		int current_row = 0;
		for (int i = 0; i < num_threads; i++) {
			args[i].start_row = current_row;
			int extra = (i < remainder) ? 1 : 0;
			args[i].end_row = current_row + rows_per_thread + extra;
			current_row = args[i].end_row;
			args[i].rows = rows;
			args[i].cols = cols;
			args[i].filter = 1;  // наращивание
			args[i].in = dilation_in;
			args[i].out = dilation_out;
			if (pthread_create(&threads[i], NULL, compute_segment, &args[i]) != 0) {
				perror("pthread_create");
				free(erosion_in);
				free(erosion_out);
				free(dilation_in);
				free(dilation_out);
				return 1;
			}
		}
		for (int i = 0; i < num_threads; i++) {
			pthread_join(threads[i], NULL);
		}
		double* temp = dilation_in;
		dilation_in = dilation_out;
		dilation_out = temp;
	}
	// Итоговый результат наращивания находится в dilation_in

	if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
		perror("clock_gettime");
		return 1;
	}
	double elapsed_sec = end_time.tv_sec - start_time.tv_sec;
	double elapsed_nsec = end_time.tv_nsec - start_time.tv_nsec;
	if (elapsed_nsec < 0) {
		elapsed_sec -= 1;
		elapsed_nsec += 1000000000;
	}

	print_matrix("Результат эрозии:", erosion_in, rows, cols);
	print_matrix("Результат наращивания:", dilation_in, rows, cols);

	char time_msg[128];
	int len = snprintf(time_msg, sizeof(time_msg), "Затрачено %lf секунд\n",
	                   elapsed_sec + elapsed_nsec / 1e9);
	if (len > 0) write(STDOUT_FILENO, time_msg, len);

	free(erosion_in);
	free(erosion_out);
	free(dilation_in);
	free(dilation_out);
	return 0;
}
