#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For read, write, close, sysconf
#include <fcntl.h>  // For open
#include <errno.h>  // For errno
#include <string.h> // For strerror

/**
 * @brief 确定用于I/O操作的最佳块大小。
 *
 * 此函数通过查询系统配置来获取内存页的大小。
 * 将缓冲区大小设置为页大小通常可以提高I/O性能，
 * 因为它与内核管理内存的方式对齐。
 *
 * @return long 系统的内存页大小（以字节为单位）。如果查询失败，返回一个默认的备用值 (4096)。
 */
long io_blocksize(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        // 如果 sysconf 出错，打印一个错误信息到 stderr
        perror("sysconf(_SC_PAGESIZE) failed");
        // 返回一个常见且安全的备用值
        return 4096;
    }
    return page_size;
}

/**
 * @brief 将指定文件的内容通过缓冲区写入到标准输出。
 *
 * @param fd 要读取的文件的文件描述符。
 * @param buf_size 用于读写的缓冲区大小。
 * @return int 成功返回 0, 失败返回 -1。
 */
int buffered_cat(int fd, long buf_size) {
    char *buffer = malloc(buf_size);
    if (buffer == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for buffer: %s\n", strerror(errno));
        return -1;
    }

    ssize_t bytes_read;
    // 循环读取文件，直到文件末尾
    while ((bytes_read = read(fd, buffer, buf_size)) > 0) {
        ssize_t total_bytes_written = 0;
        // 循环写入，确保从缓冲区读取的所有数据都被写入标准输出
        // 这是必要的，因为 write 不保证一次能写入所有请求的字节
        while (total_bytes_written < bytes_read) {
            ssize_t bytes_written = write(STDOUT_FILENO, buffer + total_bytes_written, bytes_read - total_bytes_written);
            if (bytes_written == -1) {
                fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
                free(buffer); // 在退出前释放内存
                return -1;
            }
            total_bytes_written += bytes_written;
        }
    }

    free(buffer); // 释放动态分配的内存

    // 检查 read 是否因为错误而终止
    if (bytes_read == -1) {
        fprintf(stderr, "Error reading from file: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    // 获取最佳的缓冲区大小
    long buf_size = io_blocksize();
    printf("Using buffer size: %ld bytes\n", buf_size);

    // 调用函数处理文件内容
    if (buffered_cat(fd, buf_size) == -1) {
        close(fd);
        return 1; // buffered_cat 内部已经打印了错误信息
    }

    // 关闭文件
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    return 0;
}
